// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TEST_TEST_IMAGE_CAPTION_MODEL_BROKER_H_
#define CHROME_BROWSER_CONTEXT_HUB_TEST_TEST_IMAGE_CAPTION_MODEL_BROKER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/test/test_future.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/mojom/download_observer.mojom-forward.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/size.h"

class GURL;

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcherParams;
}  // namespace image_fetcher

namespace optimization_guide {
class ModelBrokerClient;
}  // namespace optimization_guide

namespace context_hub {

using ImageFetchAction =
    testing::Action<void(const GURL&,
                         image_fetcher::ImageDataFetcherCallback*,
                         image_fetcher::ImageFetcherCallback*,
                         image_fetcher::ImageFetcherParams)>;

// Returns a GMock action for `MockImageFetcher::FetchImageAndData_` that
// immediately runs the image callback with `image`.
ImageFetchAction FulfillImageFetch(gfx::Image image);

// Captures an in-flight `MockImageFetcher::FetchImageAndData_` callback so
// tests can pause an asynchronous image fetch, mutate service or model state,
// and then resume the fetch via `Complete()`.
struct HeldImageFetch {
  HeldImageFetch();
  HeldImageFetch(const HeldImageFetch&) = delete;
  HeldImageFetch& operator=(const HeldImageFetch&) = delete;
  ~HeldImageFetch();

  ImageFetchAction Hold();
  bool WaitUntilStarted();
  void Complete(gfx::Image image);

  image_fetcher::ImageFetcherCallback saved_callback;
  base::test::TestFuture<void> started_future;
};

// Fake `ModelBroker`, `ModelSolution`, and `Session` implementation for testing
// on-device multimodal image captioning.
class TestImageCaptionModelBroker
    : public optimization_guide::mojom::ModelBroker,
      public optimization_guide::mojom::ModelSolution,
      public on_device_model::mojom::Session {
 public:
  explicit TestImageCaptionModelBroker(std::string response_text);
  TestImageCaptionModelBroker(const TestImageCaptionModelBroker&) = delete;
  TestImageCaptionModelBroker& operator=(const TestImageCaptionModelBroker&) =
      delete;
  ~TestImageCaptionModelBroker() override;

  std::unique_ptr<optimization_guide::ModelBrokerClient> CreateClient();

  void SetAvailable(bool available);
  void SetUnavailable(optimization_guide::mojom::ModelUnavailableReason reason);
  void SetDefaultUseCase(std::string use_case);
  void SetExperimentalUseCase(std::string model_version, std::string use_case);

  void set_available_use_case(std::string use_case) {
    available_use_case_ = std::move(use_case);
  }
  void set_disconnect_on_generate(bool disconnect_on_generate) {
    disconnect_on_generate_ = disconnect_on_generate;
  }

  bool received_image_input() const { return received_image_input_; }
  const gfx::Size& received_bitmap_size() const {
    return received_bitmap_size_;
  }
  const std::string& received_prompt_text() const {
    return received_prompt_text_;
  }
  const std::string& last_subscribed_use_case() const {
    return last_subscribed_use_case_;
  }
  bool reported_healthy_completion() const {
    return reported_healthy_completion_;
  }

  void FlushForTesting();

 private:
  // optimization_guide::mojom::ModelBroker:
  void Subscribe(optimization_guide::mojom::ModelSubscriptionOptionsPtr options,
                 mojo::PendingRemote<optimization_guide::mojom::ModelSubscriber>
                     subscriber) override;
  void GetConfig(optimization_guide::mojom::OnDeviceFeature feature,
                 GetConfigCallback callback) override;
  void AddModelDownloadProgressObserver(
      const std::string& use_case,
      mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer)
      override;
  void RequestAssetsFor(const std::string& use_case) override;

  // optimization_guide::mojom::ModelSolution:
  void CreateSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> session,
      on_device_model::mojom::SessionParamsPtr params) override;
  void CreateDecisionSession(
      on_device_model::mojom::DecisionQueryPtr query,
      mojo::PendingReceiver<on_device_model::mojom::DecisionSession> session)
      override;
  void ReportHealthyCompletion() override;

  // on_device_model::mojom::Session:
  void Append(on_device_model::mojom::AppendOptionsPtr options,
              mojo::PendingRemote<on_device_model::mojom::ContextClient> client)
      override;
  void Generate(on_device_model::mojom::GenerateOptionsPtr options,
                mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                    response) override;
  void GetSizeInTokens(on_device_model::mojom::InputPtr input,
                       GetSizeInTokensCallback callback) override;
  void Score(const std::string& text, ScoreCallback callback) override;
  void GetProbabilitiesBlocking(
      const std::string& text,
      GetProbabilitiesBlockingCallback callback) override;
  void Clone(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) override;
  void SetPriority(on_device_model::mojom::Priority priority) override;
  void AsrStream(
      on_device_model::mojom::AsrStreamOptionsPtr options,
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> stream,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder)
      override;
  void Hint(on_device_model::mojom::HintOptionsPtr options) override;

  std::string response_text_;
  std::optional<optimization_guide::mojom::ModelUnavailableReason>
      unavailable_reason_;
  std::optional<optimization_guide::proto::PromptApiFeatureConfig>
      prompt_api_feature_config_;
  std::string available_use_case_;
  bool disconnect_on_generate_ = false;
  bool received_image_input_ = false;
  gfx::Size received_bitmap_size_;
  std::string received_prompt_text_;
  std::string last_subscribed_use_case_;
  bool reported_healthy_completion_ = false;
  mojo::Receiver<optimization_guide::mojom::ModelBroker> broker_receiver_{this};
  mojo::RemoteSet<optimization_guide::mojom::ModelSubscriber> subscribers_;
  mojo::ReceiverSet<optimization_guide::mojom::ModelSolution>
      solution_receivers_;
  mojo::ReceiverSet<on_device_model::mojom::Session> session_receivers_;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TEST_TEST_IMAGE_CAPTION_MODEL_BROKER_H_
