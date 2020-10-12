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
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {
constexpr float kFastForwardFactor = 1.0001;
}  // namespace

class TestAmbientURLLoaderImpl : public AmbientURLLoader {
 public:
  TestAmbientURLLoaderImpl() = default;
  ~TestAmbientURLLoaderImpl() override = default;

  // AmbientURLLoader:
  void Download(
      const std::string& url,
      network::SimpleURLLoader::BodyAsStringCallback callback) override {
    // Reply with a unique string each time to avoid check to skip loading
    // duplicate images.
    std::unique_ptr<std::string> data = std::make_unique<std::string>(
        data_ ? *data_ : base::StringPrintf("test_image_%i", count_));
    count_++;
    // Pretend to respond asynchronously.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(data)),
        base::TimeDelta::FromMilliseconds(1));
  }
  void DownloadToFile(
      const std::string& url,
      network::SimpleURLLoader::DownloadToFileCompleteCallback callback,
      const base::FilePath& file_path) override {
    if (!data_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::FilePath()));
      return;
    }

    if (!WriteFile(file_path, *data_)) {
      LOG(WARNING) << "error writing file to file_path: " << file_path;

      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::FilePath()));
      return;
    }

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), file_path));
  }

  void SetData(std::unique_ptr<std::string> data) { data_ = std::move(data); }

 private:
  bool WriteFile(const base::FilePath& file_path, const std::string& data) {
    base::ScopedBlockingCall blocking(FROM_HERE, base::BlockingType::MAY_BLOCK);
    return base::WriteFile(file_path, data);
  }
  int count_ = 0;
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
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chromeos::features::kAmbientModeFeature,
      {{"GeoPhotosEnabled", "true"},
       {"CapturedOnPixelPhotosEnabled", "false"}});
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
  ambient_controller()->ShowUi();
  // The UI only shows when images are downloaded to avoid showing blank screen.
  FastForwardToNextImage();
  // Flush the message loop to finish all async calls.
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::HideAmbientScreen() {
  ambient_controller()->ShowHiddenUi();
}

void AmbientAshTestBase::CloseAmbientScreen() {
  ambient_controller()->CloseUi();
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

views::View* AmbientAshTestBase::GetMediaStringViewTextContainer() {
  return GetMediaStringView()->media_text_container_for_testing();
}

views::Label* AmbientAshTestBase::GetMediaStringViewTextLabel() {
  return GetMediaStringView()->media_text_label_for_testing();
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
      kFastForwardFactor * AmbientController::kAutoShowWaitTimeInterval);
}

void AmbientAshTestBase::FastForwardToNextImage() {
  task_environment()->FastForwardBy(kFastForwardFactor * kPhotoRefreshInterval);
}

void AmbientAshTestBase::FastForwardTiny() {
  // `TestAmbientURLLoaderImpl` has a small delay (1ms) to fake download delay,
  // here we fake plenty of time to download the image.
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(10));
}

void AmbientAshTestBase::FastForwardToLockScreen() {
  task_environment()->FastForwardBy(kFastForwardFactor * kLockScreenDelay);
}

void AmbientAshTestBase::FastForwardHalfLockScreenDelay() {
  task_environment()->FastForwardBy(0.5 * kFastForwardFactor *
                                    kLockScreenDelay);
}

void AmbientAshTestBase::SetPowerStateCharging() {
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  PowerStatus::Get()->SetProtoForTesting(proto);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::SetPowerStateDischarging() {
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  PowerStatus::Get()->SetProtoForTesting(proto);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::SetPowerStateFull() {
  power_manager::PowerSupplyProperties proto;
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  PowerStatus::Get()->SetProtoForTesting(proto);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::FastForwardToRefreshWeather() {
  task_environment()->FastForwardBy(1.2 * kWeatherRefreshInterval);
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

base::TimeDelta AmbientAshTestBase::GetRefreshTokenDelay() {
  return token_controller()->GetTimeUntilReleaseForTesting();
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

FakeAmbientBackendControllerImpl* AmbientAshTestBase::backend_controller() {
  return static_cast<FakeAmbientBackendControllerImpl*>(
      ambient_controller()->ambient_backend_controller());
}

void AmbientAshTestBase::FetchTopics() {
  photo_controller()->FetchTopicsForTesting();
}

void AmbientAshTestBase::FetchImage() {
  photo_controller()->FetchImageForTesting();
}

void AmbientAshTestBase::FetchBackupImages() {
  photo_controller()->FetchBackupImagesForTesting();
}

void AmbientAshTestBase::SetUrlLoaderData(std::unique_ptr<std::string> data) {
  auto* url_loader_ = static_cast<TestAmbientURLLoaderImpl*>(
      photo_controller()->get_url_loader_for_testing());

  url_loader_->SetData(std::move(data));
}

void AmbientAshTestBase::SetImageDecoderImage(const gfx::ImageSkia& image) {
  auto* image_decoder = static_cast<TestAmbientImageDecoderImpl*>(
      photo_controller()->get_image_decoder_for_testing());

  image_decoder->SetImage(image);
}

}  // namespace ash
