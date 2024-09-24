// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_view.h"

#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_contents_view_test_api.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/test/power_monitor_test.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

using media_session::mojom::MediaSessionAction;
using media_session::test::TestMediaController;

class LockScreenMediaViewTest : public LoginTestBase {
 public:
  LockScreenMediaViewTest() = default;
  LockScreenMediaViewTest(const LockScreenMediaViewTest&) = delete;
  LockScreenMediaViewTest& operator=(const LockScreenMediaViewTest&) = delete;
  ~LockScreenMediaViewTest() override = default;

  void SetUp() override {
    set_start_session(true);
    LoginTestBase::SetUp();

    LockContentsView* lock_contents_view = new LockContentsView(
        mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
        DataDispatcher(),
        std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
    LockContentsViewTestApi lock_contents(lock_contents_view);
    SetWidget(CreateWidgetWithContent(lock_contents_view));
    SetUserCount(1);
    media_view_ = lock_contents.media_view();

    media_controller_ = std::make_unique<TestMediaController>();
    media_view_->SetMediaControllerForTesting(
        media_controller_->CreateMediaControllerRemote());
  }

  void TearDown() override {
    media_view_ = nullptr;
    LoginTestBase::TearDown();
  }

  void SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState playback_state =
          media_session::mojom::MediaPlaybackState::kPlaying) {
    media_view_->MediaSessionChanged(base::UnguessableToken::Create());

    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->playback_state = playback_state;
    session_info->is_controllable = true;
    media_view_->MediaSessionInfoChanged(session_info.Clone());
  }

  LockScreenMediaView* media_view() { return media_view_; }

  global_media_controls::MediaItemUIDetailedView* media_detailed_view() {
    return media_view_->GetDetailedViewForTesting();
  }

  bool IsActionButtonVisible(MediaSessionAction action) {
    views::Button* button =
        media_detailed_view()->GetActionButtonForTesting(action);
    return button && button->GetVisible();
  }

  void Suspend() { power_monitor_source_.GenerateSuspendEvent(); }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

 private:
  raw_ptr<LockScreenMediaView> media_view_ = nullptr;
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;
  std::unique_ptr<TestMediaController> media_controller_;
};

TEST_F(LockScreenMediaViewTest, DoNotUpdateMetadataBetweenSessions) {
  media_session::MediaMetadata metadata;
  metadata.source_title = u"source title";
  metadata.title = u"title";
  metadata.artist = u"artist";
  SimulateMediaSessionChanged();
  media_view()->MediaSessionMetadataChanged(metadata);

  metadata.source_title = u"source title2";
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  SimulateMediaSessionChanged();
  media_view()->MediaSessionMetadataChanged(metadata);

  EXPECT_EQ(u"source title",
            media_detailed_view()->GetSourceLabelForTesting()->GetText());
  EXPECT_EQ(u"title",
            media_detailed_view()->GetTitleLabelForTesting()->GetText());
  EXPECT_EQ(u"artist",
            media_detailed_view()->GetArtistLabelForTesting()->GetText());
}

TEST_F(LockScreenMediaViewTest, DoNotUpdateActionsBetweenSessions) {
  std::set<MediaSessionAction> actions;
  actions.insert(MediaSessionAction::kPlay);
  actions.insert(MediaSessionAction::kPause);
  actions.insert(MediaSessionAction::kEnterPictureInPicture);
  actions.insert(MediaSessionAction::kExitPictureInPicture);
  SimulateMediaSessionChanged();
  media_view()->MediaSessionActionsChanged(
      std::vector<MediaSessionAction>(actions.begin(), actions.end()));

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPause));
  EXPECT_FALSE(
      IsActionButtonVisible(MediaSessionAction::kEnterPictureInPicture));
  EXPECT_FALSE(
      IsActionButtonVisible(MediaSessionAction::kExitPictureInPicture));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));

  actions.insert(MediaSessionAction::kPreviousTrack);
  SimulateMediaSessionChanged();
  media_view()->MediaSessionActionsChanged(
      std::vector<MediaSessionAction>(actions.begin(), actions.end()));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
}

TEST_F(LockScreenMediaViewTest, DoNotUpdatePositionBetweenSessions) {
  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  SimulateMediaSessionChanged();
  media_view()->MediaSessionPositionChanged(media_position);

  media_session::MediaPosition media_position_paused(
      /*playback_rate=*/0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);
  SimulateMediaSessionChanged();
  media_view()->MediaSessionPositionChanged(media_position_paused);
  EXPECT_EQ(media_position.playback_rate(),
            media_detailed_view()->GetPositionForTesting().playback_rate());
}

TEST_F(LockScreenMediaViewTest, DoNotUpdateArtworkBetweenSessions) {
  SimulateMediaSessionChanged();

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorGREEN);
  SimulateMediaSessionChanged();
  media_view()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);

  EXPECT_TRUE(
      media_detailed_view()->GetArtworkViewForTesting()->GetImage().isNull());
}

TEST_F(LockScreenMediaViewTest, DismissButtonCheck) {
  SimulateMediaSessionChanged();
  EXPECT_TRUE(media_view()->GetVisible());

  views::test::ButtonTestApi(media_view()->GetDismissButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  media_view()->FlushForTesting();
  EXPECT_EQ(1, media_controller()->stop_count());
  EXPECT_FALSE(media_view()->GetVisible());
}

TEST_F(LockScreenMediaViewTest, PowerSuspendState) {
  SimulateMediaSessionChanged();
  EXPECT_TRUE(media_view()->GetVisible());
  Suspend();
  EXPECT_FALSE(media_view()->GetVisible());
}

TEST_F(LockScreenMediaViewTest, AccessibleProperties) {
  SimulateMediaSessionChanged();
  EXPECT_TRUE(media_view()->GetVisible());
  ui::AXNodeData node_data;
  media_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kListItem);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF8(
                IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACCESSIBLE_NAME));
}

}  // namespace ash
