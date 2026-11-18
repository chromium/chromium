// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/image_caption_job.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

// Maximum width and height, in pixels, of bitmaps serialized over Mojo to the
// on-device vision encoder (which downscales inputs internally). Larger images
// are downscaled on a background thread while preserving aspect ratio so a 1 MB
// compressed download cannot expand to a 40-50 MB IPC payload.
constexpr int kMaxImageCaptionDimension = 1024;

constexpr char kImageCaptionPrompt[] =
    "In 30 words or less, describe the main subject and setting of this "
    "image, and transcribe any prominent visible text.";

constexpr uint32_t kImageCaptionMaxOutputTokens = 128;

// Resolves the subscriber use-case name for `OnDeviceFeature::kPromptApi` from
// the manifest feature config when available (honoring
// `kAIApiFoundationalModel` experimental use-case overrides), falling back to
// `"prompt_api"`.
std::string ResolvePromptApiUseCase(
    const std::optional<mojo_base::ProtoWrapper>& config_wrapper) {
  const std::string fallback_use_case = optimization_guide::ToUseCaseName(
      optimization_guide::mojom::OnDeviceFeature::kPromptApi);
  if (!config_wrapper.has_value()) {
    return fallback_use_case;
  }
  std::optional<optimization_guide::proto::Any> any_config =
      config_wrapper->As<optimization_guide::proto::Any>();
  if (!any_config.has_value()) {
    return fallback_use_case;
  }
  optimization_guide::proto::PromptApiFeatureConfig feature_config;
  if (!feature_config.ParseFromString(any_config->value())) {
    return fallback_use_case;
  }

  if (base::FeatureList::IsEnabled(kAIApiFoundationalModel)) {
    constexpr char kModelVersionParam[] = "model_version";
    const std::string model_version = base::GetFieldTrialParamValueByFeature(
        kAIApiFoundationalModel, kModelVersionParam);
    auto it = feature_config.experimental_use_cases().find(model_version);
    if (it != feature_config.experimental_use_cases().end() &&
        !it->second.empty()) {
      return it->second;
    }
  }
  if (!feature_config.default_use_case().empty()) {
    return feature_config.default_use_case();
  }
  return fallback_use_case;
}

// TODO(crbug.com/570703760): Share downscaling logic with
// `EncodeChatImageToPng()` in `context_hub_service.cc`.
SkBitmap DownscaleBitmapToFit(const SkBitmap& bitmap, int max_dimension) {
  const int longest_side = std::max(bitmap.width(), bitmap.height());
  CHECK_GT(longest_side, max_dimension);
  gfx::Size scaled_size =
      gfx::ScaleToRoundedSize(gfx::Size(bitmap.width(), bitmap.height()),
                              static_cast<float>(max_dimension) / longest_side);
  // Ensure neither dimension rounds down to 0 for extreme aspect ratios.
  scaled_size.SetToMax(gfx::Size(1, 1));
  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BEST, scaled_size.width(),
      scaled_size.height());
}

}  // namespace

ImageCaptionJob::ImageCaptionJob(
    MemoryBankEntry entry,
    image_fetcher::ImageFetcher* image_fetcher,
    optimization_guide::ModelBrokerClient* model_broker_client,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    CaptionCallback callback)
    : entry_(std::move(entry)),
      image_fetcher_(CHECK_DEREF(image_fetcher)),
      model_broker_client_(CHECK_DEREF(model_broker_client)),
      traffic_annotation_(traffic_annotation),
      callback_(std::move(callback)) {
  CHECK_EQ(entry_.type, MemoryBankType::kImage);
  CHECK(entry_.image_url.has_value());
  CHECK(entry_.image_url->SchemeIsHTTPOrHTTPS());
  CHECK(callback_);
}

ImageCaptionJob::~ImageCaptionJob() = default;

void ImageCaptionJob::Start() {
  CHECK(!is_running_);
  CHECK(callback_);
  is_running_ = true;
  model_broker_client_->GetConfig(
      optimization_guide::mojom::OnDeviceFeature::kPromptApi,
      base::BindOnce(&ImageCaptionJob::OnGetConfig,
                     weak_factory_.GetWeakPtr()));
}

void ImageCaptionJob::OnGetConfig(
    std::optional<mojo_base::ProtoWrapper> config_wrapper) {
  use_case_ = ResolvePromptApiUseCase(config_wrapper);
  // Intentionally check `CanCreateSession()` without calling
  // `RequestAssetsFor()` so saving an image never triggers a multi-gigabyte
  // background model download when the model is not already installed.
  model_broker_client_->GetSubscriber(use_case_).CanCreateSession(
      {on_device_model::CapabilityFlags::kImageInput},
      base::BindOnce(&ImageCaptionJob::OnCanCreateSession,
                     weak_factory_.GetWeakPtr()));
}

void ImageCaptionJob::OnCanCreateSession(
    std::optional<optimization_guide::mojom::ModelUnavailableReason>
        unavailable_reason,
    std::optional<optimization_guide::mojom::ModelNotSupportedDetailedReason>
        detailed_reason) {
  if (unavailable_reason.has_value()) {
    Finish(/*caption=*/"");
    return;
  }

  image_fetcher::ImageFetcherParams params(traffic_annotation_,
                                           "ContextHubMemoryBank");
  params.set_max_download_size(
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
  image_fetcher_->FetchImage(*entry_.image_url,
                             base::BindOnce(&ImageCaptionJob::OnImageFetched,
                                            weak_factory_.GetWeakPtr()),
                             std::move(params));
}

void ImageCaptionJob::OnImageFetched(
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  SkBitmap bitmap = image.AsBitmap();
  if (bitmap.drawsNothing()) {
    Finish(/*caption=*/"");
    return;
  }

  if (std::max(bitmap.width(), bitmap.height()) <= kMaxImageCaptionDimension) {
    GenerateCaption(std::move(bitmap));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DownscaleBitmapToFit, std::move(bitmap),
                     kMaxImageCaptionDimension),
      base::BindOnce(&ImageCaptionJob::GenerateCaption,
                     weak_factory_.GetWeakPtr()));
}

void ImageCaptionJob::GenerateCaption(SkBitmap bitmap) {
  if (bitmap.drawsNothing()) {
    Finish(/*caption=*/"");
    return;
  }

  // Inspect the subscriber's current client directly rather than calling
  // `WaitForClient()`: if the model transitioned to `kPendingAssets` while
  // the image was being fetched or resized, `WaitForClient()` would wait
  // indefinitely because `ImageCaptionJob` does not request asset downloads.
  std::optional<optimization_guide::ModelClient>& subscriber_client =
      model_broker_client_->GetSubscriber(use_case_).client();
  model_client_ = subscriber_client ? subscriber_client->GetWeakPtr() : nullptr;
  if (!model_client_ || !model_client_->capabilities().Has(
                            on_device_model::CapabilityFlags::kImageInput)) {
    Finish(/*caption=*/"");
    return;
  }

  auto session_params = on_device_model::mojom::SessionParams::New();
  session_params->capabilities.Put(
      on_device_model::CapabilityFlags::kImageInput);
  session_params->top_k = 1;
  session_params->temperature = 0.0f;
  mojo::PendingReceiver<on_device_model::mojom::Session> session_receiver =
      session_.BindNewPipeAndPassReceiver();
  session_.set_disconnect_handler(base::BindOnce(&ImageCaptionJob::OnDisconnect,
                                                 weak_factory_.GetWeakPtr()));
  model_client_->solution().CreateSession(std::move(session_receiver),
                                          std::move(session_params));
  // Captions are not time critical, so yield to foreground requests to the
  // shared on-device model, such as Prompt API calls.
  session_->SetPriority(on_device_model::mojom::Priority::kBackground);

  auto append_options = on_device_model::mojom::AppendOptions::New();
  append_options->input = on_device_model::mojom::Input::New();
  append_options->input_source =
      on_device_model::mojom::InputSource::kUserInput;
  append_options->input->pieces.push_back(
      on_device_model::mojom::InputPiece::NewToken(ml::Token::kUser));
  append_options->input->pieces.push_back(
      on_device_model::mojom::InputPiece::NewBitmap(std::move(bitmap)));
  append_options->input->pieces.push_back(
      on_device_model::mojom::InputPiece::NewText(kImageCaptionPrompt));
  append_options->input->pieces.push_back(
      on_device_model::mojom::InputPiece::NewToken(ml::Token::kEnd));
  append_options->input->pieces.push_back(
      on_device_model::mojom::InputPiece::NewToken(ml::Token::kModel));
  session_->Append(std::move(append_options), /*client=*/mojo::NullRemote());

  auto generate_options = on_device_model::mojom::GenerateOptions::New();
  generate_options->max_output_tokens = kImageCaptionMaxOutputTokens;
  mojo::PendingRemote<on_device_model::mojom::StreamingResponder> responder =
      responder_receiver_.BindNewPipeAndPassRemote();
  responder_receiver_.set_disconnect_handler(base::BindOnce(
      &ImageCaptionJob::OnDisconnect, weak_factory_.GetWeakPtr()));
  session_->Generate(std::move(generate_options), std::move(responder));
}

void ImageCaptionJob::OnResponse(
    on_device_model::mojom::ResponseChunkPtr chunk) {
  if (chunk) {
    accumulated_caption_.append(chunk->text);
  }
}

void ImageCaptionJob::OnComplete(
    on_device_model::mojom::ResponseSummaryPtr summary) {
  if (model_client_) {
    model_client_->solution().ReportHealthyCompletion();
  }
  Finish(std::move(accumulated_caption_));
}

void ImageCaptionJob::OnToolCalls(
    std::vector<on_device_model::mojom::ToolCallPtr> tool_calls) {}

void ImageCaptionJob::OnDisconnect() {
  Finish(/*caption=*/"");
}

void ImageCaptionJob::Finish(std::string caption) {
  CHECK(callback_);
  is_running_ = false;
  responder_receiver_.reset();
  session_.reset();
  weak_factory_.InvalidateWeakPtrs();
  MemoryBankEntry entry = std::move(entry_);
  std::move(callback_).Run(std::move(entry), std::move(caption));
}

}  // namespace context_hub
