// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
#define ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_

#include <memory>
#include <string>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/public/cpp/test/test_ambient_client.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/widget/widget.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AmbientAccessTokenController;
class AmbientContainerView;
class AmbientPhotoController;
class FakeAmbientBackendControllerImpl;
class MediaStringView;

// The base class to test the Ambient Mode in Ash.
class AmbientAshTestBase : public AshTestBase {
 public:
  AmbientAshTestBase();
  ~AmbientAshTestBase() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Enables/disables ambient mode.
  void SetAmbientModeEnabled(bool enabled);

  // Creates ambient screen in its own widget.
  void ShowAmbientScreen();

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

  views::View* GetMediaStringViewTextContainer();

  views::Label* GetMediaStringViewTextLabel();

  // Simulates the system starting to resume.
  // Wait until the event has been processed.
  void SimulateSystemResumeAndWait();

  // Simulates a screen idle state event.
  // Wait until the event has been processed.
  void SetScreenIdleStateAndWait(bool is_screen_dimmed, bool is_off);

  // Simulates a screen brightness changed event.
  void SetScreenBrightnessAndWait(double percent);

  void SimulateMediaMetadataChanged(media_session::MediaMetadata metadata);

  void SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState state);

  // Set the size of the next image that will be loaded.
  void SetPhotoViewImageSize(int width, int height);

  // Advance the task environment timer to expire the inactivity monitor.
  void FastForwardToInactivity();

  // Advance the task environment timer to load the next photo.
  void FastForwardToNextImage();

  // Advance the task environment timer a tiny amount. This is intended to
  // trigger any pending async operations.
  void FastForwardTiny();

  // Advance the task environment timer to load the weather info.
  void FastForwardToRefreshWeather();

  // Advance the task environment timer to ambient mode lock screen delay.
  void FastForwardToLockScreen();
  void FastForwardHalfLockScreenDelay();

  void SetPowerStateCharging();
  void SetPowerStateDischarging();
  void SetPowerStateFull();

  // Returns the number of active wake locks of type |type|.
  int GetNumOfActiveWakeLocks(device::mojom::WakeLockType type);

  // Simulate to issue an |access_token|.
  // If |with_error| is true, will return an empty access token.
  void IssueAccessToken(const std::string& access_token, bool with_error);

  bool IsAccessTokenRequestPending() const;

  base::TimeDelta GetRefreshTokenDelay();

  AmbientBackgroundImageView* GetAmbientBackgroundImageView();

  // Returns the media string view for displaying ongoing media info.
  MediaStringView* GetMediaStringView();

  AmbientController* ambient_controller();

  AmbientPhotoController* photo_controller();

  // Returns the top-level view which contains all the ambient components.
  AmbientContainerView* container_view();

  AmbientAccessTokenController* token_controller();

  FakeAmbientBackendControllerImpl* backend_controller();

  void FetchTopics();

  void FetchImage();

  void FetchBackupImages();

  void SetUrlLoaderData(std::unique_ptr<std::string> data);

  void SetImageDecoderImage(const gfx::ImageSkia& image);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestImageDownloader> image_downloader_;

  device::TestWakeLockProvider wake_lock_provider_;
  std::unique_ptr<TestAmbientClient> ambient_client_;
  std::unique_ptr<views::Widget> widget_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_BASE_H_
