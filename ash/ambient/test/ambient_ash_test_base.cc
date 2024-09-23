// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/ambient/test/ambient_ash_test_base.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_managed_photo_controller.h"
#include "ash/ambient/ambient_managed_slideshow_ui_launcher.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/ambient_photo_cache_settings.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/ambient/test/ambient_test_util.h"
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
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
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
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr base::TimeDelta kWaitForWidgetsTimeout = base::Seconds(10);

std::map<int, ::ambient::PhotoCacheEntry> GetCachedFilesFromStore(
    ambient_photo_cache::Store store) {
  std::map<int, ::ambient::PhotoCacheEntry> cached_files;
  for (int i = 0; i < kMaxNumberOfCachedImages; ++i) {
    base::test::TestFuture<::ambient::PhotoCacheEntry> future;
    ambient_photo_cache::ReadPhotoCache(store, i, future.GetCallback());
    ::ambient::PhotoCacheEntry entry = future.Get();
    if (!entry.primary_photo().image().empty()) {
      cached_files[i] = std::move(entry);
    }
  }
  return cached_files;
}

}  // namespace

class AmbientAshTestBase::FakePhotoDownloadServer {
 public:
  explicit FakePhotoDownloadServer(
      network::TestURLLoaderFactory& url_loader_factory)
      : url_loader_factory_(&url_loader_factory) {
    url_loader_factory.SetInterceptor(
        base::BindRepeating(&FakePhotoDownloadServer::InterceptIncomingRequest,
                            weak_factory_.GetWeakPtr()));
  }
  FakePhotoDownloadServer(const FakePhotoDownloadServer&) = delete;
  FakePhotoDownloadServer& operator=(const FakePhotoDownloadServer&) = delete;
  ~FakePhotoDownloadServer() = default;

  void set_download_data(std::unique_ptr<std::string> download_data) {
    download_data_ = std::move(download_data);
  }

  std::map<GURL, std::string>& download_data_per_url() {
    return download_data_per_url_;
  }

  void set_download_delay(base::TimeDelta delay) { download_delay_ = delay; }

  void set_custom_image_size(gfx::Size custom_image_size) {
    custom_image_size_ = std::move(custom_image_size);
  }

  void set_custom_image_color(SkColor custom_image_color) {
    custom_image_color_ = std::move(custom_image_color);
  }

  void set_image_codec(data_decoder::mojom::ImageCodec image_codec) {
    image_codec_ = image_codec;
  }

 private:
  static constexpr int kTestImageMinSize = 25;
  static constexpr int kTestImageMaxSize = 50;

  void InterceptIncomingRequest(const network::ResourceRequest& request) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakePhotoDownloadServer::RespondToPendingRequest,
                       weak_factory_.GetWeakPtr(), request.url.spec(),
                       GetDownloadData(request.url)),
        download_delay_);
  }

  void RespondToPendingRequest(const std::string& url,
                               const std::string& content) {
    CHECK(url_loader_factory_->SimulateResponseForPendingRequest(url, content))
        << "Failed to find pending request for " << url;
  }

  std::string GetDownloadData(const GURL& url) {
    if (download_data_per_url_.count(url)) {
      return download_data_per_url_[url];
    }

    if (download_data_) {
      return *download_data_;
    }

    return CreateEncodedImageForTesting(GetNextTestImageSize(),
                                        GetNextTestImageColor(), image_codec_);
  }

  gfx::Size GetNextTestImageSize() {
    if (custom_image_size_) {
      return *custom_image_size_;
    }

    gfx::Size test_size = gfx::Size(test_image_size_, test_image_size_);
    ++test_image_size_;
    if (test_image_size_ > kTestImageMaxSize) {
      test_image_size_ = kTestImageMinSize;
    }
    return test_size;
  }

  SkColor GetNextTestImageColor() {
    if (custom_image_color_) {
      SkColor custom_image_color = *custom_image_color_;
      custom_image_color_.reset();
      return custom_image_color;
    }
    SkColor test_color =
        SkColorSetRGB(test_image_color_, test_image_color_, test_image_color_);
    ++test_image_color_;
    return test_color;
  }

  const raw_ptr<network::TestURLLoaderFactory> url_loader_factory_;
  // Specific download data per url. Takes priority over `download_data_` if
  // a match is not found in this map.
  std::map<GURL, std::string> download_data_per_url_;
  // If not null, will return an arbitrary photo when downloading.
  std::unique_ptr<std::string> download_data_;
  base::TimeDelta download_delay_;

  // Automatically generates images of different sizes and colors for every
  // request to prevent duplicate photos being returned. This simulates the
  // real backend behavior.
  int test_image_size_ = kTestImageMinSize;
  uint8_t test_image_color_ = 0;

  // If not specified, the size is automatically generated using
  // `test_image_size_`.
  std::optional<gfx::Size> custom_image_size_;
  std::optional<SkColor> custom_image_color_;

  data_decoder::mojom::ImageCodec image_codec_ =
      data_decoder::mojom::ImageCodec::kDefault;

  base::WeakPtrFactory<FakePhotoDownloadServer> weak_factory_{this};
};

AmbientAshTestBase::AmbientAshTestBase()
    : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  recorder_ = AuthEventsRecorder::CreateForTesting();
}

AmbientAshTestBase::~AmbientAshTestBase() = default;

void AmbientAshTestBase::SetUp() {
  ASSERT_TRUE(primary_cache_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(backup_cache_dir_.CreateUniqueTempDir());
  SetAmbientPhotoCacheRootDirForTesting(primary_cache_dir_.GetPath());
  SetAmbientBackupPhotoCacheRootDirForTesting(backup_cache_dir_.GetPath());
  AshTestBase::SetUp();

  GetAmbientAshTestHelper()->ambient_client().SetAutomaticalyIssueToken(true);
  fake_photo_download_server_ = std::make_unique<FakePhotoDownloadServer>(
      GetAmbientAshTestHelper()->ambient_client().test_url_loader_factory());

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
  fake_photo_download_server_.reset();
  AshTestBase::TearDown();
}

void AmbientAshTestBase::SetAmbientModeEnabled(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      ambient::prefs::kAmbientModeEnabled, enabled);
}

void AmbientAshTestBase::SetAmbientUiSettings(
    const AmbientUiSettings& settings) {
  settings.WriteToPrefService(
      *Shell::Get()->session_controller()->GetActivePrefService());
  DisableBackupCacheDownloads();
}

AmbientUiSettings AmbientAshTestBase::GetCurrentUiSettings() {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(pref_service);
  return AmbientUiSettings::ReadFromPrefService(*pref_service);
}

void AmbientAshTestBase::DisableBackupCacheDownloads() {
  // Some |AmbientUiSettings| legitimately don't use a photo controller, in
  // which case backup photos are not downloaded anyways.
  if (photo_controller()) {
    photo_controller()->backup_photo_refresh_timer_for_testing().Stop();
  }
}

void AmbientAshTestBase::SetAmbientModeManagedScreensaverEnabled(bool enabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(prefs);

  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      ambient::prefs::kAmbientModeManagedScreensaverEnabled,
      std::make_unique<base::Value>(enabled));
}

void AmbientAshTestBase::SetAmbientTheme(
    personalization_app::mojom::AmbientTheme theme) {
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
  ambient_controller()->SetUiVisibilityShouldShow();
  WaitForWidgets(kWaitForWidgetsTimeout);
}

void AmbientAshTestBase::SetAmbientPreviewAndWaitForWidgets() {
  ambient_controller()->SetUiVisibilityPreview();
  WaitForWidgets(kWaitForWidgetsTimeout);
}

void AmbientAshTestBase::WaitForWidgets(base::TimeDelta timeout) {
  base::test::ScopedRunLoopTimeout loop_timeout(FROM_HERE, timeout);
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
  fake_photo_download_server_->set_custom_image_size(gfx::Size(width, height));
}

void AmbientAshTestBase::SetNextDecodedPhotoColor(SkColor color) {
  fake_photo_download_server_->set_custom_image_color(std::move(color));
}

void AmbientAshTestBase::UseLosslessPhotoCompression(
    bool use_lossless_photo_compression) {
  data_decoder::mojom::ImageCodec codec =
      use_lossless_photo_compression
          ? data_decoder::mojom::ImageCodec::kPng
          : data_decoder::mojom::ImageCodec::kDefault;
  photo_controller()->set_image_codec_for_testing(codec);
  fake_photo_download_server_->set_image_codec(codec);
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

std::optional<float>
AmbientAshTestBase::GetRemainingLockScreenTimeoutFraction() {
  const auto& inactivity_timer = ambient_controller()->inactivity_timer_;
  if (!inactivity_timer.IsRunning()) {
    return std::nullopt;
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

void AmbientAshTestBase::FastForwardByDurationInMinutes(int minutes) {
  task_environment()->FastForwardBy(base::Minutes(minutes));
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

void AmbientAshTestBase::SetExternalUsbPowerConnected() {
  proto_.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
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

std::map<int, ::ambient::PhotoCacheEntry> AmbientAshTestBase::GetCachedFiles() {
  return GetCachedFilesFromStore(ambient_photo_cache::Store::kPrimary);
}

std::map<int, ::ambient::PhotoCacheEntry>
AmbientAshTestBase::GetBackupCachedFiles() {
  return GetCachedFilesFromStore(ambient_photo_cache::Store::kBackup);
}

AmbientController* AmbientAshTestBase::ambient_controller() {
  return Shell::Get()->ambient_controller();
}

AmbientUiLauncher* AmbientAshTestBase::ambient_ui_launcher() {
  return ambient_controller()->ambient_ui_launcher();
}

AmbientPhotoController* AmbientAshTestBase::photo_controller() {
  return ambient_ui_launcher()->GetAmbientPhotoController();
}

AmbientManagedPhotoController* AmbientAshTestBase::managed_photo_controller() {
  if (!ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return nullptr;
  }
  AmbientManagedSlideshowUiLauncher* ui_launcher =
      static_cast<AmbientManagedSlideshowUiLauncher*>(ambient_ui_launcher());
  return &ui_launcher->photo_controller_;
}

ScreensaverImagesPolicyHandler* AmbientAshTestBase::managed_policy_handler() {
  if (!ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return nullptr;
  }

  return ambient_controller()->screensaver_images_policy_handler_.get();
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
  return ambient_controller()->access_token_controller();
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
  fake_photo_download_server_->set_download_data(
      std::make_unique<std::string>(data));
}

void AmbientAshTestBase::ClearDownloadPhotoData() {
  fake_photo_download_server_->set_download_data(nullptr);
}

void AmbientAshTestBase::SetDownloadPhotoDataForUrl(GURL url,
                                                    std::string data) {
  fake_photo_download_server_->download_data_per_url()[std::move(url)] =
      std::move(data);
}

void AmbientAshTestBase::SetPhotoDownloadDelay(base::TimeDelta delay) {
  fake_photo_download_server_->set_download_delay(delay);
}

void AmbientAshTestBase::CreateTestImageJpegFile(base::FilePath path,
                                                 size_t width,
                                                 size_t height,
                                                 SkColor color) {
  SkBitmap bitmap = gfx::test::CreateBitmap(width, height, color);
  std::vector<unsigned char> data;
  ASSERT_TRUE(gfx::JPEGCodec::Encode(std::move(bitmap), /*quality=*/50, &data));
  ASSERT_TRUE(base::WriteFile(path, data));
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
