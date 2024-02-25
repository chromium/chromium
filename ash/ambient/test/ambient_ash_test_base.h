// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
#define ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ui/ambient_animation_view.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/ambient/ui/ambient_info_view.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AmbientAccessTokenController;
class AmbientContainerView;
class AmbientManagedPhotoController;
class AmbientPhotoController;
class AmbientUiSettings;
class FakeAmbientBackendControllerImpl;
class MediaStringView;
class ScreensaverImagesPolicyHandler;

namespace {

// The default factor to multiply ambient timeouts by. Slightly greater than 1
// to reduce flakiness by making sure the timeouts have expired.
inline constexpr float kDefaultFastForwardFactor = 1.01;

}  // namespace

// The base class to test the Ambient Mode in Ash.
class AmbientAshTestBase : public AshTestBase {
 public:
  AmbientAshTestBase();
  ~AmbientAshTestBase() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Enables/disables ambient mode for the currently active user session.
  void SetAmbientModeEnabled(bool enabled);

  // Enables/disables the managed ambient mode pref in the currently active pref
  // service.
  void SetAmbientModeManagedScreensaverEnabled(bool enabled);

  // Sets the |AmbientUiSettings| to use when ShowAmbientScreen() is called.
  // To reflect real world usage, the incoming settings do not take effect
  // immediately if the test is currently displaying the ambient screen. In that
  // case, the ambient screen must be closed, and the new settings will take
  // effect with the next call to ShowAmbientScreen().
  void SetAmbientUiSettings(const AmbientUiSettings& settings);
  AmbientUiSettings GetCurrentUiSettings();

  // Convenient form of the above that only sets |AmbientUiSettings::theme| and
  // leaves the rest of the settings unset.
  void SetAmbientTheme(personalization_app::mojom::AmbientTheme theme);

  // Sets jitters configs to zero for pixel testing.
  void DisableJitter();

  // Creates ambient screen in its own widget.
  void SetAmbientShownAndWaitForWidgets();
  void SetAmbientPreviewAndWaitForWidgets();

  // Hides ambient screen. Can only be called after |ShowAmbientScreen| has been
  // called.
  void HideAmbientScreen();

  // Closes ambient screen. Can only be called after |ShowAmbientScreen| has
  // been called.
  void CloseAmbientScreen();

  // Simulates user locks/unlocks screen which will result in ambient widget
  // shown/closed.
  void LockScreen();
  void UnlockScreen();
  // Whether lockscreen is shown.
  bool IsLocked();

  // Simulates the system starting to suspend with Reason |reason|.
  // Wait until the event has been processed.
  void SimulateSystemSuspendAndWait(
      power_manager::SuspendImminent::Reason reason);

  // Return all media string view text containers. There is one per display.
  std::vector<views::View*> GetMediaStringViewTextContainers();
  // Return the media string view text container for the ambient mode container
  // on the default display.
  views::View* GetMediaStringViewTextContainer();

  // Return all media string view text labels. There is one per display.
  std::vector<views::Label*> GetMediaStringViewTextLabels();
  // Return the media string view text label for the ambient mode container on
  // the default display.
  views::Label* GetMediaStringViewTextLabel();

  // Simulates the system starting to resume.
  // Wait until the event has been processed.
  void SimulateSystemResumeAndWait();

  // Simulates a screen idle state event.
  // Wait until the event has been processed.
  void SetScreenIdleStateAndWait(bool is_screen_dimmed, bool is_off);

  // Simulates clicking the power button.
  void SimulatePowerButtonClick();

  void SimulateMediaMetadataChanged(media_session::MediaMetadata metadata);

  void SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState state);

  // Set the size all subsequent images that will be loaded.
  void SetDecodedPhotoSize(int width, int height);

  // Set the color of the next image that will be loaded. Afterwards, the color
  // will be randomly generated.
  void SetNextDecodedPhotoColor(SkColor color);

  // Useful if the decoded ambient images must be deterministic (ex: writing
  // test expectations on the images' pixel content).
  void UseLosslessPhotoCompression(bool use_lossless_photo_compression);

  void SetPhotoOrientation(bool portrait);

  void SetPhotoTopicType(::ambient::TopicType topic_type);

  // Advance the task environment timer to expire the lock screen inactivity
  // timer, scaled by `factor`.
  void FastForwardByLockScreenInactivityTimeout(
      float factor = kDefaultFastForwardFactor);

  // Approximately how much of the lock screen inactivity timeout is left.
  // Bounded to [0,1], 1 meaning that the timer just started. If the lock screen
  // inactivity timer is not running, returns null.
  std::optional<float> GetRemainingLockScreenTimeoutFraction();

  // Advance the task environment timer to load the next photo, scaled by
  // `factor`.
  void FastForwardByPhotoRefreshInterval(
      float factor = kDefaultFastForwardFactor);

  // Advance the task environment timer a tiny amount. This is intended to
  // trigger any pending async operations.
  void FastForwardTiny();

  // Advance the task environment timer to load the weather info.
  void FastForwardByWeatherRefreshInterval();

  // Advance the task environment timer to ambient mode lock screen delay,
  // scaled by `factor`.
  void FastForwardByBackgroundLockScreenTimeout(
      float factor = kDefaultFastForwardFactor);

  // Advance the task environment timer to screen saver duration in minutes.
  void FastForwardByDurationInMinutes(int minutes);

  void SetPowerStateCharging();
  void SetPowerStateDischarging();
  void SetPowerStateFull();

  // An official, non-USB external power is connected.
  void SetExternalPowerConnected();

  // A USB external power is connected.
  void SetExternalUsbPowerConnected();

  // No external power of any form is connected.
  void SetExternalPowerDisconnected();

  void SetBatteryPercent(double percent);

  // Returns the number of active wake locks of type |type|.
  int GetNumOfActiveWakeLocks(device::mojom::WakeLockType type);

  // Simulate to issue an |access_token|.
  // If |is_empty| is true, will return an empty access token.
  void IssueAccessToken(bool is_empty);

  bool IsAccessTokenRequestPending();

  base::TimeDelta GetRefreshTokenDelay();

  // Returns the ambient image view for each display.
  std::vector<AmbientBackgroundImageView*> GetAmbientBackgroundImageViews();
  // Returns the AmbientBackgroundImageView for the default display.
  AmbientBackgroundImageView* GetAmbientBackgroundImageView();

  // Returns the media string views for displaying ongoing media info.
  std::vector<MediaStringView*> GetMediaStringViews();
  // Returns the media string view for the default display.
  MediaStringView* GetMediaStringView();
  PhotoView* GetPhotoView();
  AmbientAnimationView* GetAmbientAnimationView();
  AmbientInfoView* GetAmbientInfoView();
  AmbientSlideshowPeripheralUi* GetAmbientSlideshowPeripheralUi();

  std::map<int, ::ambient::PhotoCacheEntry> GetCachedFiles();
  std::map<int, ::ambient::PhotoCacheEntry> GetBackupCachedFiles();

  AmbientController* ambient_controller();

  AmbientUiLauncher* ambient_ui_launcher();

  AmbientPhotoController* photo_controller();

  AmbientManagedPhotoController* managed_photo_controller();

  ScreensaverImagesPolicyHandler* managed_policy_handler();

  AmbientWeatherController* weather_controller();

  // Returns the top-level views which contains all the ambient components.
  std::vector<AmbientContainerView*> GetContainerViews();
  // Returns the top level ambient container view for the primary root window.
  AmbientContainerView* GetContainerView();

  AmbientAccessTokenController* token_controller();

  FakeAmbientBackendControllerImpl* backend_controller();

  void FetchTopics();

  void FetchImage();

  void FetchBackupImages();

  void SetDownloadPhotoData(std::string data);

  void ClearDownloadPhotoData();

  // Takes priority over `SetDownloadPhotoData()`, which applies to all urls if
  // a specific `SetDownloadPhotoDataForUrl()` was not made.
  void SetDownloadPhotoDataForUrl(GURL url, std::string data);

  void SetPhotoDownloadDelay(base::TimeDelta delay);

  void CreateTestImageJpegFile(base::FilePath path,
                               size_t width,
                               size_t height,
                               SkColor color);
  void DisableBackupCacheDownloads();

  void SetScreenSaverDuration(int minutes);

  int GetScreenSaverDuration();

 private:
  class FakePhotoDownloadServer;

  // Waits for the ambient UI to start rendering (i.e. a widget is created and
  // the ambient UI is visible to the user). A fatal error occurs if the
  // `timeout` elapses before the UI starts rendering.
  void WaitForWidgets(base::TimeDelta timeout);
  void SpinWaitForAmbientViewAvailable(
      const base::RepeatingClosure& quit_closure);

  InProcessDataDecoder decoder_;
  TestAshWebViewFactory web_view_factory_;
  std::unique_ptr<views::Widget> widget_;
  power_manager::PowerSupplyProperties proto_;
  TestImageDownloader image_downloader_;
  std::unique_ptr<ash::AuthEventsRecorder> recorder_;
  std::unique_ptr<FakePhotoDownloadServer> fake_photo_download_server_;
  base::ScopedTempDir primary_cache_dir_;
  base::ScopedTempDir backup_cache_dir_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
