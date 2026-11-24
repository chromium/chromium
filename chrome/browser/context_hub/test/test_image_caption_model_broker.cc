// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/test/test_image_caption_model_broker.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/test/test_future.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/mojom/download_observer.mojom-forward.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace context_hub {

ImageFetchAction FulfillImageFetch(gfx::Image image) {
  return [image = std::move(image)](
             const GURL& /*image_url*/,
             image_fetcher::ImageDataFetcherCallback* /*image_data_callback*/,
             image_fetcher::ImageFetcherCallback* image_callback,
             image_fetcher::ImageFetcherParams /*params*/) {
    std::move(*image_callback).Run(image, image_fetcher::RequestMetadata());
  };
}

HeldImageFetch::HeldImageFetch() = default;
HeldImageFetch::~HeldImageFetch() = default;

ImageFetchAction HeldImageFetch::Hold() {
  return
      [this](const GURL& /*image_url*/,
             image_fetcher::ImageDataFetcherCallback* /*image_data_callback*/,
             image_fetcher::ImageFetcherCallback* image_callback,
             image_fetcher::ImageFetcherParams /*params*/) {
        saved_callback = std::move(*image_callback);
        started_future.SetValue();
      };
}

bool HeldImageFetch::WaitUntilStarted() {
  return started_future.Wait();
}

void HeldImageFetch::Complete(gfx::Image image) {
  std::move(saved_callback).Run(image, image_fetcher::RequestMetadata());
}

TestImageCaptionModelBroker::TestImageCaptionModelBroker(
    std::string response_text)
    : response_text_(std::move(response_text)) {}

TestImageCaptionModelBroker::~TestImageCaptionModelBroker() = default;

std::unique_ptr<optimization_guide::ModelBrokerClient>
TestImageCaptionModelBroker::CreateClient() {
  return std::make_unique<optimization_guide::ModelBrokerClient>(
      broker_receiver_.BindNewPipeAndPassRemote(),
      /*logger=*/nullptr);
}

void TestImageCaptionModelBroker::SetAvailable(bool available) {
  unavailable_reason_ =
      available ? std::nullopt
                : std::optional(optimization_guide::mojom::
                                    ModelUnavailableReason::kNotSupported);
}

void TestImageCaptionModelBroker::SetUnavailable(
    optimization_guide::mojom::ModelUnavailableReason reason) {
  unavailable_reason_ = reason;
  for (const auto& subscriber : subscribers_) {
    subscriber->Unavailable(reason, /*detailed_reason=*/std::nullopt);
  }
  subscribers_.FlushForTesting();
}

void TestImageCaptionModelBroker::SetDefaultUseCase(std::string use_case) {
  if (!prompt_api_feature_config_) {
    prompt_api_feature_config_.emplace();
  }
  prompt_api_feature_config_->set_default_use_case(std::move(use_case));
}

void TestImageCaptionModelBroker::SetExperimentalUseCase(
    std::string model_version,
    std::string use_case) {
  if (!prompt_api_feature_config_) {
    prompt_api_feature_config_.emplace();
  }
  (*prompt_api_feature_config_
        ->mutable_experimental_use_cases())[std::move(model_version)] =
      std::move(use_case);
}

void TestImageCaptionModelBroker::FlushForTesting() {
  solution_receivers_.FlushForTesting();
}

void TestImageCaptionModelBroker::Subscribe(
    optimization_guide::mojom::ModelSubscriptionOptionsPtr options,
    mojo::PendingRemote<optimization_guide::mojom::ModelSubscriber>
        subscriber) {
  last_subscribed_use_case_ = options->use_case;
  mojo::RemoteSetElementId id = subscribers_.Add(std::move(subscriber));
  optimization_guide::mojom::ModelSubscriber* remote = subscribers_.Get(id);

  std::optional<optimization_guide::mojom::ModelUnavailableReason>
      unavailable_reason = unavailable_reason_;
  if (!unavailable_reason.has_value() && !available_use_case_.empty() &&
      options->use_case != available_use_case_) {
    unavailable_reason =
        optimization_guide::mojom::ModelUnavailableReason::kPendingAssets;
  }

  on_device_model::Capabilities capabilities;
  if (!unavailable_reason.has_value()) {
    capabilities.Put(on_device_model::CapabilityFlags::kImageInput);
  }
  remote->CapabilitiesUpdated(capabilities);
  if (unavailable_reason.has_value()) {
    remote->Unavailable(*unavailable_reason,
                        /*detailed_reason=*/std::nullopt);
    return;
  }
  mojo::PendingRemote<optimization_guide::mojom::ModelSolution> solution;
  solution_receivers_.Add(this, solution.InitWithNewPipeAndPassReceiver());
  auto config = optimization_guide::mojom::ModelSolutionConfig::New();
  config->feature_config = mojo_base::ProtoWrapper(
      optimization_guide::proto::OnDeviceModelExecutionFeatureConfig());
  config->model_versions = mojo_base::ProtoWrapper(
      optimization_guide::proto::OnDeviceModelVersions());
  config->model_capabilities = capabilities;
  remote->Available(std::move(config), std::move(solution));
}

void TestImageCaptionModelBroker::GetConfig(
    optimization_guide::mojom::OnDeviceFeature feature,
    GetConfigCallback callback) {
  if (feature == optimization_guide::mojom::OnDeviceFeature::kPromptApi &&
      prompt_api_feature_config_.has_value()) {
    std::move(callback).Run(mojo_base::ProtoWrapper(
        optimization_guide::AnyWrapProto(*prompt_api_feature_config_)));
    return;
  }
  std::move(callback).Run(std::nullopt);
}

void TestImageCaptionModelBroker::AddModelDownloadProgressObserver(
    const std::string& use_case,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {}

void TestImageCaptionModelBroker::RequestAssetsFor(
    const std::string& use_case) {}

void TestImageCaptionModelBroker::CreateSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> session,
    on_device_model::mojom::SessionParamsPtr params) {
  EXPECT_TRUE(
      params->capabilities.Has(on_device_model::CapabilityFlags::kImageInput));
  session_receivers_.Add(this, std::move(session));
}

void TestImageCaptionModelBroker::CreateDecisionSession(
    on_device_model::mojom::DecisionQueryPtr query,
    mojo::PendingReceiver<on_device_model::mojom::DecisionSession> session) {}

void TestImageCaptionModelBroker::ReportHealthyCompletion() {
  reported_healthy_completion_ = true;
}

void TestImageCaptionModelBroker::Append(
    on_device_model::mojom::AppendOptionsPtr options,
    mojo::PendingRemote<on_device_model::mojom::ContextClient> client) {
  EXPECT_EQ(options->input_source,
            on_device_model::mojom::InputSource::kUserInput);
  for (const auto& piece : options->input->pieces) {
    if (piece->is_bitmap()) {
      const SkBitmap& bitmap = piece->get_bitmap();
      received_image_input_ = !bitmap.drawsNothing();
      received_bitmap_size_ = gfx::Size(bitmap.width(), bitmap.height());
    } else if (piece->is_text()) {
      received_prompt_text_ = piece->get_text();
    }
  }
}

void TestImageCaptionModelBroker::Generate(
    on_device_model::mojom::GenerateOptionsPtr options,
    mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response) {
  if (disconnect_on_generate_) {
    session_receivers_.Clear();
    return;
  }
  mojo::Remote<on_device_model::mojom::StreamingResponder> responder(
      std::move(response));
  if (!response_text_.empty()) {
    auto chunk = on_device_model::mojom::ResponseChunk::New();
    chunk->text = response_text_;
    responder->OnResponse(std::move(chunk));
  }
  responder->OnComplete(on_device_model::mojom::ResponseSummary::New());
}

void TestImageCaptionModelBroker::GetSizeInTokens(
    on_device_model::mojom::InputPtr input,
    GetSizeInTokensCallback callback) {
  std::move(callback).Run(0);
}

void TestImageCaptionModelBroker::Score(const std::string& text,
                                        ScoreCallback callback) {
  std::move(callback).Run(0.0f);
}

void TestImageCaptionModelBroker::GetProbabilitiesBlocking(
    const std::string& text,
    GetProbabilitiesBlockingCallback callback) {
  std::move(callback).Run({});
}

void TestImageCaptionModelBroker::Clone(
    mojo::PendingReceiver<on_device_model::mojom::Session> session) {}

void TestImageCaptionModelBroker::SetPriority(
    on_device_model::mojom::Priority priority) {}

void TestImageCaptionModelBroker::AsrStream(
    on_device_model::mojom::AsrStreamOptionsPtr options,
    mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> stream,
    mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder) {
}

void TestImageCaptionModelBroker::Hint(
    on_device_model::mojom::HintOptionsPtr options) {}

}  // namespace context_hub
