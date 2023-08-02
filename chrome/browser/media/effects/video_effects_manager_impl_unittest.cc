// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/effects/video_effects_manager_impl.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"

namespace {
video_capture::mojom::VideoEffectsConfigurationPtr GetConfigurationSync(
    mojo::Remote<video_capture::mojom::VideoEffectsManager>& effects_manager) {
  base::test::TestFuture<video_capture::mojom::VideoEffectsConfigurationPtr>
      output_configuration;
  effects_manager->GetConfiguration(output_configuration.GetCallback());
  return output_configuration.Take();
}

class ConfigurationObserverImpl
    : public video_capture::mojom::VideoEffectsConfigurationObserver {
 public:
  ConfigurationObserverImpl() : receiver_(this) {}

  void OnConfigurationChanged(video_capture::mojom::VideoEffectsConfigurationPtr
                                  configuration) override {
    received_configurations_.push_back(configuration.Clone());
  }

  mojo::PendingRemote<video_capture::mojom::VideoEffectsConfigurationObserver>
  GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<video_capture::mojom::VideoEffectsConfigurationPtr>&
  received_configurations() {
    return received_configurations_;
  }

 private:
  std::vector<video_capture::mojom::VideoEffectsConfigurationPtr>
      received_configurations_;

  mojo::Receiver<video_capture::mojom::VideoEffectsConfigurationObserver>
      receiver_;
};
}  // namespace

class VideoEffectsManagerImplTest : public testing::Test {
 public:
  VideoEffectsManagerImplTest()
      : effects_manager_(
            nullptr,
            base::BindOnce(
                &VideoEffectsManagerImplTest::OnLastReceiverDisconnected,
                base::Unretained(this))) {}

  mojo::Remote<video_capture::mojom::VideoEffectsManager>
  GetEffectManagerRemote() {
    mojo::Remote<video_capture::mojom::VideoEffectsManager> remote;
    effects_manager_.Bind(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  void OnLastReceiverDisconnected() { ++last_receiver_disconnect_count_; }

 protected:
  size_t last_receiver_disconnect_count_ = 0;

 private:
  VideoEffectsManagerImpl effects_manager_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(VideoEffectsManagerImplTest, SetGetConfiguration) {
  auto remote = GetEffectManagerRemote();

  const float kPaddingRatio = 0.342;
  base::test::TestFuture<video_capture::mojom::SetConfigurationResult>
      result_future;
  remote->SetConfiguration(
      video_capture::mojom::VideoEffectsConfiguration::New(
          nullptr, nullptr,
          video_capture::mojom::Framing::New(gfx::InsetsF{kPaddingRatio})),
      result_future.GetCallback());

  EXPECT_EQ(video_capture::mojom::SetConfigurationResult::kOk,
            result_future.Get());
  EXPECT_EQ(kPaddingRatio,
            GetConfigurationSync(remote)->framing->padding_ratios.top());
}

TEST_F(VideoEffectsManagerImplTest, AddObserver) {
  auto remote = GetEffectManagerRemote();

  ConfigurationObserverImpl configuration_observer;
  remote->AddObserver(configuration_observer.GetRemote());

  for (const auto& padding_ratio :
       std::vector<float>{0.1, 0.2343, 0.3435, 0.38500}) {
    base::test::TestFuture<video_capture::mojom::SetConfigurationResult>
        result_future;
    remote->SetConfiguration(
        video_capture::mojom::VideoEffectsConfiguration::New(
            nullptr, nullptr,
            video_capture::mojom::Framing::New(gfx::InsetsF{padding_ratio})),
        result_future.GetCallback());

    EXPECT_EQ(video_capture::mojom::SetConfigurationResult::kOk,
              result_future.Get());
    EXPECT_EQ(padding_ratio, configuration_observer.received_configurations()
                                 .back()
                                 ->framing->padding_ratios.top());
  }

  EXPECT_EQ(4u, configuration_observer.received_configurations().size());
}
