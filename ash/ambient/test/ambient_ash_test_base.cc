// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_ash_test_base.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

class TestAmbientURLLoaderImpl : public AmbientURLLoader {
 public:
  TestAmbientURLLoaderImpl() = default;
  ~TestAmbientURLLoaderImpl() override = default;

  // AmbientURLLoader:
  void Download(
      const std::string& url,
      network::SimpleURLLoader::BodyAsStringCallback callback) override {
    std::string data = data_ ? *data_ : "test";
    // Pretend to respond asynchronously.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::make_unique<std::string>(data)),
        base::TimeDelta::FromMilliseconds(1));
  }

  void SetData(std::unique_ptr<std::string> data) { data_ = std::move(data); }

 private:
  // If not null, will return this data.
  std::unique_ptr<std::string> data_;
};

class TestAmbientImageDecoderImpl : public AmbientImageDecoder {
 public:
  TestAmbientImageDecoderImpl() = default;
  ~TestAmbientImageDecoderImpl() override = default;

  // AmbientImageDecoder:
  void Decode(
      const std::vector<uint8_t>& encoded_bytes,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) override {
    gfx::ImageSkia image =
        image_ ? *image_ : gfx::test::CreateImageSkia(width_, height_);
    // Only use once.
    image_.reset();

    // Pretend to respond asynchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), image));
  }

  void SetImageSize(int width, int height) {
    width_ = width;
    height_ = height;
  }

  void SetImage(const gfx::ImageSkia& image) { image_ = image; }

 private:
  // Width and height of test images.
  int width_ = 10;
  int height_ = 20;

  // If set, will replay this image.
  base::Optional<gfx::ImageSkia> image_;
};

AmbientAshTestBase::AmbientAshTestBase()
    : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

AmbientAshTestBase::~AmbientAshTestBase() = default;

void AmbientAshTestBase::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(
      chromeos::features::kAmbientModeFeature);
  image_downloader_ = std::make_unique<TestImageDownloader>();
  ambient_client_ = std::make_unique<TestAmbientClient>(&wake_lock_provider_);
  chromeos::PowerManagerClient::InitializeFake();

  AshTestBase::SetUp();

  // Need to reset first and then assign the TestPhotoClient because can only
  // have one instance of AmbientBackendController.
  ambient_controller()->set_backend_controller_for_testing(nullptr);
  ambient_controller()->set_backend_controller_for_testing(
      std::make_unique<FakeAmbientBackendControllerImpl>());
  photo_controller()->set_url_loader_for_testing(
      std::make_unique<TestAmbientURLLoaderImpl>());
  photo_controller()->set_image_decoder_for_testing(
      std::make_unique<TestAmbientImageDecoderImpl>());
  token_controller()->SetTokenUsageBufferForTesting(
      base::TimeDelta::FromSeconds(30));
  SetAmbientModeEnabled(true);
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::TearDown() {
  ambient_client_.reset();
  image_downloader_.reset();

  AshTestBase::TearDown();
}

void AmbientAshTestBase::SetAmbientModeEnabled(bool enabled) {
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      ambient::prefs::kAmbientModeEnabled, enabled);
}

void AmbientAshTestBase::ShowAmbientScreen() {
  // The widget will be destroyed in |AshTestBase::TearDown()|.
  ambient_controller()->ShowUi(AmbientUiMode::kInSessionUi);
  // The UI only shows when images are downloaded to avoid showing blank screen.
  FastForwardToNextImage();
  // Flush the message loop to finish all async calls.
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::HideAmbientScreen() {
  ambient_controller()->HideLockScreenUi();
}

void AmbientAshTestBase::CloseAmbientScreen() {
  ambient_controller()->ambient_ui_model()->SetUiVisibility(
      AmbientUiVisibility::kClosed);
}

void AmbientAshTestBase::LockScreen() {
  GetSessionControllerClient()->LockScreen();
}

void AmbientAshTestBase::UnlockScreen() {
  GetSessionControllerClient()->UnlockScreen();
}

bool AmbientAshTestBase::IsLocked() {
  return Shell::Get()->session_controller()->IsScreenLocked();
}

void AmbientAshTestBase::SimulateSystemSuspendAndWait(
    power_manager::SuspendImminent::Reason reason) {
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(reason);
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::SimulateSystemResumeAndWait() {
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::SetScreenIdleStateAndWait(bool is_screen_dimmed,
                                                   bool is_off) {
  power_manager::ScreenIdleState screen_idle_state;
  screen_idle_state.set_dimmed(is_screen_dimmed);
  screen_idle_state.set_off(is_off);
  chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(
      screen_idle_state);
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::SetScreenBrightnessAndWait(double percent) {
  power_manager::BacklightBrightnessChange change;
  change.set_percent(percent);

  chromeos::FakePowerManagerClient::Get()->SendScreenBrightnessChanged(change);
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::SimulateMediaMetadataChanged(
    media_session::MediaMetadata metadata) {
  GetMediaStringView()->MediaSessionMetadataChanged(metadata);
}

void AmbientAshTestBase::SimulateMediaPlaybackStateChanged(
    media_session::mojom::MediaPlaybackState state) {
  // Creates media session info.
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state = state;

  // Simulate media session info changed.
  GetMediaStringView()->MediaSessionInfoChanged(std::move(session_info));
}

void AmbientAshTestBase::SetPhotoViewImageSize(int width, int height) {
  auto* image_decoder = static_cast<TestAmbientImageDecoderImpl*>(
      photo_controller()->get_image_decoder_for_testing());

  image_decoder->SetImageSize(width, height);
}

AmbientBackgroundImageView*
AmbientAshTestBase::GetAmbientBackgroundImageView() {
  return static_cast<AmbientBackgroundImageView*>(container_view()->GetViewByID(
      AssistantViewID::kAmbientBackgroundImageView));
}

MediaStringView* AmbientAshTestBase::GetMediaStringView() {
  return static_cast<MediaStringView*>(
      container_view()->GetViewByID(AssistantViewID::kAmbientMediaStringView));
}

void AmbientAshTestBase::FastForwardToInactivity() {
  task_environment()->FastForwardBy(
      2 * AmbientController::kAutoShowWaitTimeInterval);
}

void AmbientAshTestBase::FastForwardToNextImage() {
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);
}

int AmbientAshTestBase::GetNumOfActiveWakeLocks(
    device::mojom::WakeLockType type) {
  base::RunLoop run_loop;
  int result_count = 0;
  wake_lock_provider_.GetActiveWakeLocksForTests(
      type, base::BindOnce(
                [](base::RunLoop* run_loop, int* result_count, int32_t count) {
                  *result_count = count;
                  run_loop->Quit();
                },
                &run_loop, &result_count));
  run_loop.Run();
  return result_count;
}

void AmbientAshTestBase::IssueAccessToken(const std::string& token,
                                          bool with_error) {
  ambient_client_->IssueAccessToken(token, with_error);
}

bool AmbientAshTestBase::IsAccessTokenRequestPending() const {
  return ambient_client_->IsAccessTokenRequestPending();
}

AmbientController* AmbientAshTestBase::ambient_controller() {
  return Shell::Get()->ambient_controller();
}

AmbientPhotoController* AmbientAshTestBase::photo_controller() {
  return ambient_controller()->ambient_photo_controller();
}

AmbientContainerView* AmbientAshTestBase::container_view() {
  return ambient_controller()->get_container_view_for_testing();
}

AmbientAccessTokenController* AmbientAshTestBase::token_controller() {
  return ambient_controller()->access_token_controller_for_testing();
}

void AmbientAshTestBase::FetchTopics() {
  photo_controller()->FetchTopicsForTesting();
}

void AmbientAshTestBase::FetchImage() {
  photo_controller()->FetchImageForTesting();
}

void AmbientAshTestBase::SetUrlLoaderData(std::unique_ptr<std::string> data) {
  auto* url_loader_ = static_cast<TestAmbientURLLoaderImpl*>(
      photo_controller()->get_url_loader_for_testing());

  url_loader_->SetData(std::move(data));
}

void AmbientAshTestBase::SeteImageDecoderImage(const gfx::ImageSkia& image) {
  auto* image_decoder = static_cast<TestAmbientImageDecoderImpl*>(
      photo_controller()->get_image_decoder_for_testing());

  image_decoder->SetImage(image);
}

}  // namespace ash
