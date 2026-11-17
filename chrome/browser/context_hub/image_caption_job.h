// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_IMAGE_CAPTION_JOB_H_
#define CHROME_BROWSER_CONTEXT_HUB_IMAGE_CAPTION_JOB_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-forward.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

class SkBitmap;

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace optimization_guide {
class ModelBrokerClient;
class ModelClient;
}  // namespace optimization_guide

namespace context_hub {

// Fetches a saved image and generates a concise visual caption using the
// on-device multimodal Prompt API model when already installed, falling back to
// an empty caption if the model is unavailable or the image cannot be fetched.
class ImageCaptionJob : public on_device_model::mojom::StreamingResponder {
 public:
  using CaptionCallback =
      base::OnceCallback<void(MemoryBankEntry entry, std::string caption)>;

  // Both `image_fetcher` and `model_broker_client` must be non-null and
  // outlive `this`. `entry` must be an image entry with a valid HTTP or HTTPS
  // `image_url`.
  ImageCaptionJob(MemoryBankEntry entry,
                  image_fetcher::ImageFetcher* image_fetcher,
                  optimization_guide::ModelBrokerClient* model_broker_client,
                  net::NetworkTrafficAnnotationTag traffic_annotation,
                  CaptionCallback callback);

  ImageCaptionJob(const ImageCaptionJob&) = delete;
  ImageCaptionJob& operator=(const ImageCaptionJob&) = delete;
  ~ImageCaptionJob() override;

  bool is_running() const { return is_running_; }

  void Start();

 private:
  void OnGetConfig(std::optional<mojo_base::ProtoWrapper> config_wrapper);
  void OnCanCreateSession(
      std::optional<optimization_guide::mojom::ModelUnavailableReason>
          unavailable_reason,
      std::optional<optimization_guide::mojom::ModelNotSupportedDetailedReason>
          detailed_reason);
  void OnImageFetched(const gfx::Image& image,
                      const image_fetcher::RequestMetadata& metadata);
  void GenerateCaption(SkBitmap bitmap);

  // on_device_model::mojom::StreamingResponder:
  void OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) override;
  void OnComplete(on_device_model::mojom::ResponseSummaryPtr summary) override;
  void OnToolCalls(
      std::vector<on_device_model::mojom::ToolCallPtr> tool_calls) override;

  void OnDisconnect();
  void Finish(std::string caption);

  MemoryBankEntry entry_;
  const raw_ref<image_fetcher::ImageFetcher> image_fetcher_;
  const raw_ref<optimization_guide::ModelBrokerClient> model_broker_client_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  CaptionCallback callback_;
  bool is_running_ = false;
  std::string use_case_;
  base::WeakPtr<optimization_guide::ModelClient> model_client_;
  std::string accumulated_caption_;
  mojo::Remote<on_device_model::mojom::Session> session_;
  mojo::Receiver<on_device_model::mojom::StreamingResponder>
      responder_receiver_{this};
  base::WeakPtrFactory<ImageCaptionJob> weak_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_IMAGE_CAPTION_JOB_H_
