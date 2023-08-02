// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/effects/media_effects_service.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"

namespace {

constexpr char kDeviceId[] = "test_device";

video_capture::mojom::VideoEffectsConfigurationPtr GetConfigurationSync(
    mojo::Remote<video_capture::mojom::VideoEffectsManager>& effects_manager) {
  base::test::TestFuture<video_capture::mojom::VideoEffectsConfigurationPtr>
      output_configuration;
  effects_manager->GetConfiguration(output_configuration.GetCallback());
  return output_configuration.Take();
}

void SetFramingSync(
    mojo::Remote<video_capture::mojom::VideoEffectsManager>& effects_manager,
    float framing_padding_ratio) {
  base::test::TestFuture<video_capture::mojom::SetConfigurationResult>
      result_future;
  effects_manager->SetConfiguration(
      video_capture::mojom::VideoEffectsConfiguration::New(
          nullptr, nullptr,
          video_capture::mojom::Framing::New(
              gfx::InsetsF{framing_padding_ratio})),
      result_future.GetCallback());
  EXPECT_EQ(video_capture::mojom::SetConfigurationResult::kOk,
            result_future.Get());
}
}  // namespace

class MediaEffectsServiceTest : public testing::Test {
 public:
  MediaEffectsServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("TestProfile");
    service_ = base::WrapUnique(StartNewMediaEffectsService());
  }

  void TearDown() override {
    service_->Shutdown();
    service_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  MediaEffectsService* StartNewMediaEffectsService() {
    MediaEffectsService* service = new MediaEffectsService(profile_);
    // base::RunLoop().RunUntilIdle();
    return service;
  }

 protected:
  std::unique_ptr<MediaEffectsService> service_;

 private:
  base::raw_ptr<Profile> profile_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(MediaEffectsServiceTest, BindVideoEffectsManager) {
  mojo::Remote<video_capture::mojom::VideoEffectsManager> effects_manager;
  service_->BindVideoEffectsManager(
      kDeviceId, effects_manager.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(GetConfigurationSync(effects_manager)->framing.is_null());

  const float kFramingPaddingRatio = 0.2;
  SetFramingSync(effects_manager, kFramingPaddingRatio);

  auto configuration = GetConfigurationSync(effects_manager);
  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            configuration->framing->padding_ratios);
}

TEST_F(MediaEffectsServiceTest,
       BindVideoEffectsManager_TwoRegistrantsWithSameIdConnectToSameManager) {
  mojo::Remote<video_capture::mojom::VideoEffectsManager> effects_manager1;
  service_->BindVideoEffectsManager(
      kDeviceId, effects_manager1.BindNewPipeAndPassReceiver());

  const float kFramingPaddingRatio = 0.234;
  SetFramingSync(effects_manager1, kFramingPaddingRatio);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager1)->framing->padding_ratios);

  mojo::Remote<video_capture::mojom::VideoEffectsManager> effects_manager2;
  service_->BindVideoEffectsManager(
      kDeviceId, effects_manager2.BindNewPipeAndPassReceiver());

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager2)->framing->padding_ratios);
}

TEST_F(
    MediaEffectsServiceTest,
    BindVideoEffectsManager_TwoRegistrantsWithDifferentIdConnectToDifferentManager) {
  mojo::Remote<video_capture::mojom::VideoEffectsManager> effects_manager1;
  service_->BindVideoEffectsManager(
      "test_device_1", effects_manager1.BindNewPipeAndPassReceiver());

  const float kFramingPaddingRatio = 0.234;
  SetFramingSync(effects_manager1, kFramingPaddingRatio);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager1)->framing->padding_ratios);

  mojo::Remote<video_capture::mojom::VideoEffectsManager> effects_manager2;
  service_->BindVideoEffectsManager(
      "test_device_2", effects_manager2.BindNewPipeAndPassReceiver());

  // Expect `framing` to be unset because it is a separate instance of
  // `VideoEffectsManager`.
  auto framing = std::move(GetConfigurationSync(effects_manager2)->framing);
  EXPECT_TRUE(framing.is_null());
}

TEST_F(
    MediaEffectsServiceTest,
    OnLastReceiverDisconnected_ErasesTheManagerWhenAllReceiversAreDisconnected) {
  mojo::Remote<video_capture::mojom::VideoEffectsManager> effects_manager1;
  service_->BindVideoEffectsManager(
      kDeviceId, effects_manager1.BindNewPipeAndPassReceiver());
  mojo::Remote<video_capture::mojom::VideoEffectsManager> effects_manager2;
  service_->BindVideoEffectsManager(
      kDeviceId, effects_manager2.BindNewPipeAndPassReceiver());

  const float kFramingPaddingRatio = 0.234;

  SetFramingSync(effects_manager1, kFramingPaddingRatio);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager1)->framing->padding_ratios);

  EXPECT_EQ(gfx::InsetsF{kFramingPaddingRatio},
            GetConfigurationSync(effects_manager2)->framing->padding_ratios);

  effects_manager1.reset();
  effects_manager2.reset();
  // Wait for the reset to complete
  base::RunLoop().RunUntilIdle();

  mojo::Remote<video_capture::mojom::VideoEffectsManager> effects_manager3;
  service_->BindVideoEffectsManager(
      kDeviceId, effects_manager3.BindNewPipeAndPassReceiver());

  // Expect `framing` to be unset because it is a new instance of
  // `VideoEffectsManager`.
  EXPECT_TRUE(GetConfigurationSync(effects_manager3)->framing.is_null());
}
