// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_ash_test_base.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_managed_photo_controller.h"
#include "ash/ambient/ambient_managed_slideshow_ui_launcher.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/ambient/ui/ambient_animation_view.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/ambient_info_view.h"
#include "ash/ambient/ui/ambient_slideshow_peripheral_ui.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TestAmbientPhotoCacheImpl : public AmbientPhotoCache {
 public:
  TestAmbientPhotoCacheImpl() = default;
  ~TestAmbientPhotoCacheImpl() override = default;

  static std::unique_ptr<AmbientPhotoCache> Create() {
    return std::make_unique<TestAmbientPhotoCacheImpl>();
  }

  // AmbientPhotoCache:
  void DownloadPhoto(
      const std::string& url,
      base::OnceCallback<void(std::string&&)> callback) override {
    // Pretend to respond asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetDownloadData()),
        photo_download_delay_);
  }

  void DownloadPhotoToFile(const std::string& url,
                           int cache_index,
                           base::OnceCallback<void(bool)> callback) override {
    std::string download_data = GetDownloadData();
    if (download_data.empty()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), /*success=*/false));
      return;
    }
    ::ambient::PhotoCacheEntry cache_entry;
    cache_entry.mutable_primary_photo()->set_image(std::move(download_data));

    files_.insert(
        std::pair<int, ::ambient::PhotoCacheEntry>(cache_index, cache_entry));

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
  }

  void DecodePhoto(
      const std::string& data,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) override {
    gfx::ImageSkia image = decoded_image_ ? *decoded_image_
                                          : CreateSolidColorTestImage(
                                                decoded_size_, decoded_color_);

    // Only use once.
    decoded_image_.reset();

    // Pretend to respond asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), image));
  }

  void WritePhotoCache(int cache_index,
                       const ::ambient::PhotoCacheEntry& cache_entry,
                       base::OnceClosure callback) override {
    files_.insert(
        std::pair<int, ::ambient::PhotoCacheEntry>(cache_index, cache_entry));
    std::move(callback).Run();
  }

  void ReadPhotoCache(
      int cache_index,
      base::OnceCallback<void(::ambient::PhotoCacheEntry)> callback) override {
    auto it = files_.find(cache_index);
    if (it == files_.end()) {
      std::move(callback).Run(::ambient::PhotoCacheEntry());
      return;
    }

    std::move(callback).Run(it->second);
  }

  void Clear() override {
    download_count_ = 0;
    download_data_.reset();
    decoded_size_ = gfx::Size(10, 20);
    decoded_image_.reset();
    files_.clear();
  }

  void SetDownloadData(std::unique_ptr<std::string> download_data) {
    download_data_ = std::move(download_data);
  }

  void SetDecodedPhotoSize(int width, int height) {
    decoded_size_.set_width(width);
    decoded_size_.set_height(height);
  }

  void SetDecodedPhotoColor(SkColor color) { decoded_color_ = color; }

  void SetDecodedPhoto(const gfx::ImageSkia& image) { decoded_image_ = image; }

  void SetPhotoDownloadDelay(base::TimeDelta delay) {
    photo_download_delay_ = delay;
  }

  const std::map<int, ::ambient::PhotoCacheEntry>& get_files() {
    return files_;
  }

 private:
  std::string GetDownloadData() {
    // Reply with a unique string each time to avoid check to skip loading
    // duplicate images.
    return download_data_
               ? *download_data_
               : base::StringPrintf("test_image_%i", download_count_++);
  }

  int download_count_ = 0;

  // If not null, will return this data when downloading.
  std::unique_ptr<std::string> download_data_;

  // Color of the test images.
  SkColor decoded_color_ = SK_ColorGREEN;
  // Width and height of test images.
  gfx::Size decoded_size_{10, 20};
  // If set, will replay this image.
  absl::optional<gfx::ImageSkia> decoded_image_;

  std::map<int, ::ambient::PhotoCacheEntry> files_;

  base::TimeDelta photo_download_delay_ = base::Milliseconds(1);
};

AmbientAshTestBase::AmbientAshTestBase()
    : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  recorder_ = AuthEventsRecorder::CreateForTesting();
}

AmbientAshTestBase::~AmbientAshTestBase() = default;

void AmbientAshTestBase::SetUp() {
  AmbientPhotoCache::SetFactoryForTesting(
      base::BindRepeating(&TestAmbientPhotoCacheImpl::Create));
  AshTestBase::SetUp();

  // Need to reset first and then assign the TestPhotoClient because can only
  // have one instance of AmbientBackendController.
  ambient_controller()->set_backend_controller_for_testing(nullptr);
  ambient_controller()->set_backend_controller_for_testing(
      std::make_unique<FakeAmbientBackendControllerImpl>());
  token_controller()->SetTokenUsageBufferForTesting(base::Seconds(30));
  SetAmbientModeEnabled(true);
  base::RunLoop().RunUntilIdle();
}

void AmbientAshTestBase::TearDown() {
  AshTestBase::TearDown();
  AmbientPhotoCache::SetFactoryForTesting(base::NullCallback());
}

void AmbientAshTestBase::SetAmbientModeEnabled(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      ambient::prefs::kAmbientModeEnabled, enabled);

  if (enabled) {
    DisableBackupCacheDownloads();
  }
}

void AmbientAshTestBase::SetAmbientUiSettings(
    const AmbientUiSettings& settings) {
  settings.WriteToPrefService(
      *Shell::Get()->session_controller()->GetActivePrefService());
  DisableBackupCacheDownloads();
}

void AmbientAshTestBase::DisableBackupCacheDownloads() {
  // Some |AmbientUiSettings| legitimately don't use a photo controller, in
  // which case backup photos are not downloaded anyways.
  if (photo_controller()) {
    photo_controller()->backup_photo_refresh_timer_for_testing().Stop();
  }
}

void AmbientAshTestBase::SetAmbientModeManagedScreensaverEnabled(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      ambient::prefs::kAmbientModeManagedScreensaverEnabled, enabled);
}

void AmbientAshTestBase::SetAmbientTheme(AmbientTheme theme) {
  SetAmbientUiSettings(AmbientUiSettings(theme));
}

void AmbientAshTestBase::DisableJitter() {
  AmbientUiModel::Get()->set_jitter_config_for_testing(
      {.step_size = 0,
       .x_min_translation = 0,
       .x_max_translation = 0,
       .y_min_translation = 0,
       .y_max_translation = 0});
}

void AmbientAshTestBase::SetAmbientShownAndWaitForWidgets() {
  // The widget will be destroyed in |AshTestBase::TearDown()|.
  ambient_controller()->SetUiVisibilityShown();

  static constexpr base::TimeDelta kTimeout = base::Seconds(10);
  base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE, kTimeout);
  base::RunLoop run_loop;
  task_environment()->GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AmbientAshTestBase::SpinWaitForAmbientViewAvailable,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

void AmbientAshTestBase::SpinWaitForAmbientViewAvailable(
    const base::RepeatingClosure& quit_closure) {
  if (GetContainerView()) {
    quit_closure.Run();
  } else {
    static constexpr base::TimeDelta kPollingPeriod = base::Milliseconds(250);
    task_environment()->GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AmbientAshTestBase::SpinWaitForAmbientViewAvailable,
                       base::Unretained(this), quit_closure),
        kPollingPeriod);
  }
}

void AmbientAshTestBase::HideAmbientScreen() {
  ambient_controller()->SetUiVisibilityHidden();
}

void AmbientAshTestBase::CloseAmbientScreen() {
  ambient_controller()->SetUiVisibilityClosed();
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

void AmbientAshTestBase::SimulatePowerButtonClick() {
  chromeos::FakePowerManagerClient::Get()->SendPowerButtonEvent(
      true, task_environment()->NowTicks());
  FastForwardTiny();
  chromeos::FakePowerManagerClient::Get()->SendPowerButtonEvent(
      false, task_environment()->NowTicks());
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

std::vector<views::View*>
AmbientAshTestBase::GetMediaStringViewTextContainers() {
  std::vector<views::View*> result;
  for (auto* view : GetMediaStringViews())
    result.push_back(view->media_text_container_for_testing());
  return result;
}

views::View* AmbientAshTestBase::GetMediaStringViewTextContainer() {
  return GetMediaStringView()->media_text_container_for_testing();
}

std::vector<views::Label*> AmbientAshTestBase::GetMediaStringViewTextLabels() {
  std::vector<views::Label*> result;
  for (auto* view : GetMediaStringViews())
    result.push_back(view->media_text_label_for_testing());
  return result;
}

views::Label* AmbientAshTestBase::GetMediaStringViewTextLabel() {
  return GetMediaStringView()->media_text_label_for_testing();
}

void AmbientAshTestBase::SimulateMediaMetadataChanged(
    media_session::MediaMetadata metadata) {
  for (auto* view : GetMediaStringViews())
    view->MediaSessionMetadataChanged(metadata);
}

void AmbientAshTestBase::SimulateMediaPlaybackStateChanged(
    media_session::mojom::MediaPlaybackState state) {
  for (auto* media_string_view : GetMediaStringViews()) {
    // Creates media session info.
    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->playback_state = state;

    // Simulate media session info changed.
    media_string_view->MediaSessionInfoChanged(std::move(session_info));
  }
}

void AmbientAshTestBase::SetDecodedPhotoSize(int width, int height) {
  auto* photo_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->ambient_photo_cache());

  photo_cache->SetDecodedPhotoSize(width, height);
}

void AmbientAshTestBase::SetDecodedPhotoColor(SkColor color) {
  auto* photo_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->ambient_photo_cache());

  photo_cache->SetDecodedPhotoColor(color);
}

void AmbientAshTestBase::SetPhotoOrientation(bool portrait) {
  backend_controller()->SetPhotoOrientation(portrait);
}

void AmbientAshTestBase::SetPhotoTopicType(::ambient::TopicType topic_type) {
  backend_controller()->SetPhotoTopicType(topic_type);
}

std::vector<AmbientBackgroundImageView*>
AmbientAshTestBase::GetAmbientBackgroundImageViews() {
  std::vector<AmbientBackgroundImageView*> result;
  for (auto* view : GetContainerViews()) {
    auto* background_image_view =
        view->GetViewByID(AmbientViewID::kAmbientBackgroundImageView);
    result.push_back(
        static_cast<AmbientBackgroundImageView*>(background_image_view));
  }
  return result;
}

AmbientBackgroundImageView*
AmbientAshTestBase::GetAmbientBackgroundImageView() {
  return static_cast<AmbientBackgroundImageView*>(
      GetContainerView()->GetViewByID(kAmbientBackgroundImageView));
}

std::vector<MediaStringView*> AmbientAshTestBase::GetMediaStringViews() {
  std::vector<MediaStringView*> result;
  for (auto* view : GetContainerViews()) {
    auto* media_string_view = view->GetViewByID(kAmbientMediaStringView);
    result.push_back(static_cast<MediaStringView*>(media_string_view));
  }
  return result;
}

MediaStringView* AmbientAshTestBase::GetMediaStringView() {
  return static_cast<MediaStringView*>(
      GetContainerView()->GetViewByID(kAmbientMediaStringView));
}

PhotoView* AmbientAshTestBase::GetPhotoView() {
  return static_cast<PhotoView*>(
      GetContainerView()->GetViewByID(kAmbientPhotoView));
}

AmbientInfoView* AmbientAshTestBase::GetAmbientInfoView() {
  return static_cast<AmbientInfoView*>(
      GetContainerView()->GetViewByID(kAmbientInfoView));
}

AmbientSlideshowPeripheralUi*
AmbientAshTestBase::GetAmbientSlideshowPeripheralUi() {
  return static_cast<AmbientSlideshowPeripheralUi*>(
      GetContainerView()->GetViewByID(kAmbientSlideshowPeripheralUi));
}

AmbientAnimationView* AmbientAshTestBase::GetAmbientAnimationView() {
  return static_cast<AmbientAnimationView*>(
      GetContainerView()->GetViewByID(kAmbientAnimationView));
}

void AmbientAshTestBase::FastForwardByLockScreenInactivityTimeout(
    float factor) {
  DCHECK_GT(factor, 0.f);
  task_environment()->FastForwardBy(factor *
                                    ambient_controller()
                                        ->ambient_ui_model()
                                        ->lock_screen_inactivity_timeout());
}

void AmbientAshTestBase::FastForwardByPhotoRefreshInterval(float factor) {
  task_environment()->FastForwardBy(
      factor *
      ambient_controller()->ambient_ui_model()->photo_refresh_interval());
}

absl::optional<float>
AmbientAshTestBase::GetRemainingLockScreenTimeoutFraction() {
  const auto& inactivity_timer = ambient_controller()->inactivity_timer_;
  if (!inactivity_timer.IsRunning()) {
    return absl::nullopt;
  }

  return (inactivity_timer.desired_run_time() - base::TimeTicks::Now()) /
         inactivity_timer.GetCurrentDelay();
}

void AmbientAshTestBase::FastForwardTiny() {
  // `TestAmbientURLLoaderImpl` has a small delay (1ms) to fake download delay,
  // here we fake plenty of time to download the image.
  task_environment()->FastForwardBy(base::Milliseconds(10));
}

void AmbientAshTestBase::FastForwardByBackgroundLockScreenTimeout(
    float factor) {
  task_environment()->FastForwardBy(factor *
                                    ambient_controller()
                                        ->ambient_ui_model()
                                        ->background_lock_screen_timeout());
}

void AmbientAshTestBase::SetPowerStateCharging() {
  proto_.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  PowerStatus::Get()->SetProtoForTesting(proto_);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::SetPowerStateDischarging() {
  proto_.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  PowerStatus::Get()->SetProtoForTesting(proto_);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::SetPowerStateFull() {
  proto_.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  PowerStatus::Get()->SetProtoForTesting(proto_);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::SetExternalPowerConnected() {
  proto_.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  PowerStatus::Get()->SetProtoForTesting(proto_);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::SetExternalPowerDisconnected() {
  proto_.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  PowerStatus::Get()->SetProtoForTesting(proto_);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::SetBatteryPercent(double percent) {
  proto_.set_battery_percent(percent);
  PowerStatus::Get()->SetProtoForTesting(proto_);
  ambient_controller()->OnPowerStatusChanged();
}

void AmbientAshTestBase::FastForwardByWeatherRefreshInterval() {
  task_environment()->FastForwardBy(1.2 * kWeatherRefreshInterval);
}

int AmbientAshTestBase::GetNumOfActiveWakeLocks(
    device::mojom::WakeLockType type) {
  base::RunLoop run_loop;
  int result_count = 0;
  GetAmbientAshTestHelper()->wake_lock_provider()->GetActiveWakeLocksForTests(
      type, base::BindOnce(
                [](base::RunLoop* run_loop, int* result_count, int32_t count) {
                  *result_count = count;
                  run_loop->Quit();
                },
                &run_loop, &result_count));
  run_loop.Run();
  return result_count;
}

void AmbientAshTestBase::IssueAccessToken(bool is_empty) {
  GetAmbientAshTestHelper()->IssueAccessToken(is_empty);
}

bool AmbientAshTestBase::IsAccessTokenRequestPending() {
  return GetAmbientAshTestHelper()->IsAccessTokenRequestPending();
}

base::TimeDelta AmbientAshTestBase::GetRefreshTokenDelay() {
  return token_controller()->GetTimeUntilReleaseForTesting();
}

const std::map<int, ::ambient::PhotoCacheEntry>&
AmbientAshTestBase::GetCachedFiles() {
  auto* photo_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->ambient_photo_cache());

  return photo_cache->get_files();
}

const std::map<int, ::ambient::PhotoCacheEntry>&
AmbientAshTestBase::GetBackupCachedFiles() {
  auto* photo_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->get_backup_photo_cache_for_testing());

  return photo_cache->get_files();
}

AmbientController* AmbientAshTestBase::ambient_controller() {
  return Shell::Get()->ambient_controller();
}

AmbientPhotoController* AmbientAshTestBase::photo_controller() {
  return ambient_controller()->ambient_photo_controller();
}

AmbientManagedPhotoController* AmbientAshTestBase::managed_photo_controller() {
  if (!ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return nullptr;
  }
  AmbientManagedSlideshowUiLauncher* ui_launcher =
      static_cast<AmbientManagedSlideshowUiLauncher*>(
          ambient_controller()->ambient_ui_launcher());
  return &ui_launcher->photo_controller_;
}

ScreensaverImagesPolicyHandler* AmbientAshTestBase::managed_policy_handler() {
  if (!ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return nullptr;
  }
  AmbientManagedSlideshowUiLauncher* ui_launcher =
      static_cast<AmbientManagedSlideshowUiLauncher*>(
          ambient_controller()->ambient_ui_launcher());
  return &ui_launcher->screensaver_images_policy_handler_;
}

AmbientPhotoCache* AmbientAshTestBase::photo_cache() {
  return ambient_controller()->ambient_photo_cache();
}

AmbientWeatherController* AmbientAshTestBase::weather_controller() {
  return ambient_controller()->ambient_weather_controller();
}

std::vector<AmbientContainerView*> AmbientAshTestBase::GetContainerViews() {
  std::vector<AmbientContainerView*> result;
  for (auto* ctrl : RootWindowController::root_window_controllers()) {
    auto* widget = ctrl->ambient_widget_for_testing();
    if (widget) {
      auto* view = widget->GetContentsView();
      DCHECK(view && view->GetID() == kAmbientContainerView);
      result.push_back(static_cast<AmbientContainerView*>(view));
    }
  }
  return result;
}

AmbientContainerView* AmbientAshTestBase::GetContainerView() {
  auto* widget =
      Shell::GetPrimaryRootWindowController()->ambient_widget_for_testing();

  if (widget) {
    auto* container_view = widget->GetContentsView();
    DCHECK(container_view && container_view->GetID() == kAmbientContainerView);
    return static_cast<AmbientContainerView*>(container_view);
  }

  return nullptr;
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

void AmbientAshTestBase::SetDownloadPhotoData(std::string data) {
  auto* photo_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->ambient_photo_cache());

  photo_cache->SetDownloadData(std::make_unique<std::string>(std::move(data)));
}

void AmbientAshTestBase::ClearDownloadPhotoData() {
  auto* photo_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->ambient_photo_cache());

  photo_cache->SetDownloadData(nullptr);
}

void AmbientAshTestBase::SetBackupDownloadPhotoData(std::string data) {
  auto* backup_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->get_backup_photo_cache_for_testing());

  backup_cache->SetDownloadData(std::make_unique<std::string>(std::move(data)));
}

void AmbientAshTestBase::ClearBackupDownloadPhotoData() {
  auto* backup_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->get_backup_photo_cache_for_testing());

  backup_cache->SetDownloadData(nullptr);
}

void AmbientAshTestBase::SetDecodePhotoImage(const gfx::ImageSkia& image) {
  auto* photo_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->ambient_photo_cache());

  photo_cache->SetDecodedPhoto(image);
}

void AmbientAshTestBase::SetPhotoDownloadDelay(base::TimeDelta delay) {
  auto* photo_cache = static_cast<TestAmbientPhotoCacheImpl*>(
      ambient_controller()->ambient_photo_cache());

  photo_cache->SetPhotoDownloadDelay(delay);
}

void AmbientAshTestBase::CreateTestImageJpegFile(base::FilePath path,
                                                 size_t width,
                                                 size_t height,
                                                 SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  std::vector<unsigned char> data;
  ASSERT_TRUE(gfx::JPEGCodec::Encode(bitmap, /*quality=*/50, &data));
  size_t bytes_written = base::WriteFile(
      path, reinterpret_cast<const char*>(data.data()), data.size());
  ASSERT_EQ(data.size(), bytes_written);
}

void AmbientAshTestBase::SetScreenSaverDuration(int minutes) {
  ambient_controller()->SetScreenSaverDuration(minutes);
}

int AmbientAshTestBase::GetScreenSaverDuration() {
  return Shell::Get()
      ->session_controller()
      ->GetPrimaryUserPrefService()
      ->GetInteger(ambient::prefs::kAmbientModeRunningDurationMinutes);
}

}  // namespace ash
