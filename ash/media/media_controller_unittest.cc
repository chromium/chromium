// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_controller_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"

namespace ash {

class MediaControllerTest : public AshTestBase {
 public:
  MediaControllerTest() = default;

  MediaControllerTest(const MediaControllerTest&) = delete;
  MediaControllerTest& operator=(const MediaControllerTest&) = delete;

  ~MediaControllerTest() override = default;

  // AshTestBase
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = std::make_unique<media_session::test::TestMediaController>();

    MediaControllerImpl* media_controller = Shell::Get()->media_controller();
    media_controller->SetMediaSessionControllerForTest(
        controller_->CreateMediaControllerRemote());
    media_controller->FlushForTesting();

    {
      std::vector<media_session::mojom::MediaSessionAction> actions;
      actions.push_back(media_session::mojom::MediaSessionAction::kPlay);
      controller_->SimulateMediaSessionActionsChanged(actions);
    }

    {
      media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());

      session_info->state =
          media_session::mojom::MediaSessionInfo::SessionState::kActive;
      session_info->playback_state =
          media_session::mojom::MediaPlaybackState::kPlaying;
      controller_->SimulateMediaSessionInfoChanged(std::move(session_info));
    }

    Flush();
  }

  media_session::test::TestMediaController* controller() const {
    return controller_.get();
  }

  void SimulateSessionLock() {
    SessionInfo info;
    info.state = session_manager::SessionState::LOCKED;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  void Flush() {
    controller_->Flush();
    Shell::Get()->media_controller()->FlushForTesting();
  }

  void HandleMediaKeys() {
    Shell::Get()->media_controller()->HandleMediaPlayPause();
    Flush();
    Shell::Get()->media_controller()->HandleMediaPlay();
    Flush();
    Shell::Get()->media_controller()->HandleMediaPause();
    Flush();
    Shell::Get()->media_controller()->HandleMediaStop();
    Flush();
    Shell::Get()->media_controller()->HandleMediaPrevTrack();
    Flush();
    Shell::Get()->media_controller()->HandleMediaNextTrack();
    Flush();
    Shell::Get()->media_controller()->HandleMediaSeekBackward();
    Flush();
    Shell::Get()->media_controller()->HandleMediaSeekForward();
    Flush();
  }

 private:
  std::unique_ptr<media_session::test::TestMediaController> controller_;
};

TEST_F(MediaControllerTest, EnableMediaKeysWhenUnlocked) {
  EXPECT_EQ(0, controller()->suspend_count());
  EXPECT_EQ(0, controller()->resume_count());
  EXPECT_EQ(0, controller()->stop_count());
  EXPECT_EQ(0, controller()->previous_track_count());
  EXPECT_EQ(0, controller()->next_track_count());
  EXPECT_EQ(0, controller()->seek_backward_count());
  EXPECT_EQ(0, controller()->seek_forward_count());

  HandleMediaKeys();

  EXPECT_EQ(2, controller()->suspend_count());
  EXPECT_EQ(1, controller()->resume_count());
  EXPECT_EQ(1, controller()->stop_count());
  EXPECT_EQ(1, controller()->previous_track_count());
  EXPECT_EQ(1, controller()->next_track_count());
  EXPECT_EQ(1, controller()->seek_backward_count());
  EXPECT_EQ(1, controller()->seek_forward_count());
}

TEST_F(MediaControllerTest, EnableLockScreenMediaKeys) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kLockScreenMediaControlsEnabled, true);

  EXPECT_TRUE(
      Shell::Get()->media_controller()->AreLockScreenMediaKeysEnabled());
}

TEST_F(MediaControllerTest, DisableLockScreenMediaKeysIfPreferenceDisabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  prefs->SetBoolean(prefs::kLockScreenMediaControlsEnabled, false);

  EXPECT_FALSE(
      Shell::Get()->media_controller()->AreLockScreenMediaKeysEnabled());
}

TEST_F(MediaControllerTest, EnableMediaKeysWhenLockedAndControlsEnabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  prefs->SetBoolean(prefs::kLockScreenMediaControlsEnabled, true);

  EXPECT_EQ(0, controller()->suspend_count());
  EXPECT_EQ(0, controller()->resume_count());
  EXPECT_EQ(0, controller()->stop_count());
  EXPECT_EQ(0, controller()->previous_track_count());
  EXPECT_EQ(0, controller()->next_track_count());
  EXPECT_EQ(0, controller()->seek_backward_count());
  EXPECT_EQ(0, controller()->seek_forward_count());

  SimulateSessionLock();

  HandleMediaKeys();

  EXPECT_EQ(2, controller()->suspend_count());
  EXPECT_EQ(1, controller()->resume_count());
  EXPECT_EQ(1, controller()->stop_count());
  EXPECT_EQ(1, controller()->previous_track_count());
  EXPECT_EQ(1, controller()->next_track_count());
  EXPECT_EQ(1, controller()->seek_backward_count());
  EXPECT_EQ(1, controller()->seek_forward_count());
}

TEST_F(MediaControllerTest, DisableMediaKeysWhenLockedAndControlsDisabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  prefs->SetBoolean(prefs::kLockScreenMediaControlsEnabled, false);

  EXPECT_EQ(0, controller()->suspend_count());
  EXPECT_EQ(0, controller()->resume_count());
  EXPECT_EQ(0, controller()->stop_count());
  EXPECT_EQ(0, controller()->previous_track_count());
  EXPECT_EQ(0, controller()->next_track_count());
  EXPECT_EQ(0, controller()->seek_backward_count());
  EXPECT_EQ(0, controller()->seek_forward_count());

  SimulateSessionLock();

  HandleMediaKeys();

  EXPECT_EQ(0, controller()->suspend_count());
  EXPECT_EQ(0, controller()->resume_count());
  EXPECT_EQ(0, controller()->stop_count());
  EXPECT_EQ(0, controller()->previous_track_count());
  EXPECT_EQ(0, controller()->next_track_count());
  EXPECT_EQ(0, controller()->seek_backward_count());
  EXPECT_EQ(0, controller()->seek_forward_count());
}

}  // namespace ash
