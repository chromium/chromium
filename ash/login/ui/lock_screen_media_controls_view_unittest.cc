// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_controls_view.h"
#include "ash/login/ui/lock_contents_view_test_api.h"

#include "ash/constants/ash_features.h"
#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/media_controls_header_view.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "components/media_message_center/media_controls_progress_view.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

using media_session::mojom::MediaSessionAction;
using media_session::test::TestMediaController;

namespace {

const int kAppIconSize = 20;
constexpr int kArtworkViewHeight = 48;
constexpr int kArtworkCornerRadius = 4;

const std::u16string kTestAppName = u"Test app";

MediaSessionAction kActionButtonOrder[] = {
    MediaSessionAction::kPreviousTrack, MediaSessionAction::kSeekBackward,
    MediaSessionAction::kPause, MediaSessionAction::kSeekForward,
    MediaSessionAction::kNextTrack};

// Checks if the view class name is used by a media button.
bool IsMediaButtonType(const char* class_name) {
  return class_name == views::ImageButton::kViewClassName ||
         class_name == views::ToggleImageButton::kViewClassName;
}

class AnimationWaiter : public ui::LayerAnimationObserver,
                        public ui::LayerObserver {
 public:
  explicit AnimationWaiter(views::View* host) : layer_(host->layer()) {
    layer_->AddObserver(this);
    layer_->GetAnimator()->AddObserver(this);
  }

  AnimationWaiter(const AnimationWaiter&) = delete;
  AnimationWaiter& operator=(const AnimationWaiter&) = delete;

  ~AnimationWaiter() override {
    if (layer_) {
      layer_->RemoveObserver(this);
      layer_->GetAnimator()->RemoveObserver(this);
    }
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    if (!layer_->GetAnimator()->is_animating()) {
      layer_->GetAnimator()->RemoveObserver(this);
      layer_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  void LayerDestroyed(ui::Layer* layer) override {
    layer_->RemoveObserver(this);
    layer_->GetAnimator()->RemoveObserver(this);
    layer_ = nullptr;
  }

  void Wait() { run_loop_.Run(); }

 private:
  ui::Layer* layer_;
  base::RunLoop run_loop_;
};

}  // namespace

class LockScreenMediaControlsViewTest : public LoginTestBase {
 public:
  LockScreenMediaControlsViewTest() = default;

  LockScreenMediaControlsViewTest(const LockScreenMediaControlsViewTest&) =
      delete;
  LockScreenMediaControlsViewTest& operator=(
      const LockScreenMediaControlsViewTest&) = delete;

  ~LockScreenMediaControlsViewTest() override = default;

  void SetUp() override {
    set_start_session(true);

    // Enable media controls.
    feature_list.InitAndEnableFeature(features::kLockScreenMediaControls);

    LoginTestBase::SetUp();

    lock_contents_view_ = new LockContentsView(
        mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
        DataDispatcher(),
        std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
    LockContentsViewTestApi lock_contents(lock_contents_view_);

    std::unique_ptr<views::Widget> widget =
        CreateWidgetWithContent(lock_contents_view_);
    SetWidget(std::move(widget));

    SetUserCount(1);

    media_controls_view_ = lock_contents.media_controls_view();

    animation_waiter_ = std::make_unique<AnimationWaiter>(contents_view());

    // Inject the test media controller into the media controls view.
    media_controller_ = std::make_unique<TestMediaController>();
    media_controls_view_->set_media_controller_for_testing(
        media_controller_->CreateMediaControllerRemote());
  }

  void TearDown() override {
    animation_waiter_.reset();
    actions_.clear();

    LoginTestBase::TearDown();
  }

  void EnableAllActions() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kPreviousTrack);
    actions_.insert(MediaSessionAction::kNextTrack);
    actions_.insert(MediaSessionAction::kSeekBackward);
    actions_.insert(MediaSessionAction::kSeekForward);
    actions_.insert(MediaSessionAction::kStop);

    NotifyUpdatedActions();
  }

  void EnableAction(MediaSessionAction action) {
    actions_.insert(action);
    NotifyUpdatedActions();
  }

  void DisableAction(MediaSessionAction action) {
    actions_.erase(action);
    NotifyUpdatedActions();
  }

  void SimulateSessionUnlock() {
    GetSessionControllerClient()->UnlockScreen();
    SetUserCount(1);
  }

  void SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState playback_state) {
    // Simulate media session change.
    media_controls_view_->MediaSessionChanged(base::UnguessableToken::Create());

    // Create media session information.
    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->playback_state = playback_state;

    // Simulate media session information change.
    media_controls_view_->MediaSessionInfoChanged(session_info.Clone());
  }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->GetVisible());

    // Send event to click media playback action button.
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
  }

  void SimulateTab() {
    ui::KeyEvent pressed_tab(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
    media_controls_view_->GetFocusManager()->OnKeyEvent(pressed_tab);
  }

  views::Button* GetButtonForAction(MediaSessionAction action) const {
    const auto& buttons = media_action_buttons();
    const auto it = base::ranges::find(buttons, static_cast<int>(action),
                                       &views::Button::tag);

    if (it == buttons.end()) {
      return nullptr;
    }

    return *it;
  }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  views::View* contents_view() const {
    return media_controls_view_->contents_view_;
  }

  MediaControlsHeaderView* header_row() const {
    return media_controls_view_->header_row_;
  }

  NonAccessibleView* button_row() const {
    return media_controls_view_->button_row_;
  }

  views::ImageView* artwork_view() const {
    return media_controls_view_->session_artwork_;
  }

  views::Label* title_label() const {
    return media_controls_view_->title_label_;
  }

  views::Label* artist_label() const {
    return media_controls_view_->artist_label_;
  }

  media_message_center::MediaControlsProgressView* progress_view() const {
    return media_controls_view_->progress_;
  }

  views::ImageButton* close_button() const {
    return header_row()->close_button_for_testing();
  }

  std::vector<views::Button*>& media_action_buttons() const {
    return media_controls_view_->media_action_buttons_;
  }

  bool CloseButtonHasImage() const {
    return !close_button()
                ->GetImage(views::Button::ButtonState::STATE_NORMAL)
                .isNull();
  }

  const views::ImageView* icon_view() const {
    return header_row()->app_icon_for_testing();
  }

  const std::u16string& GetAppName() const {
    return header_row()->app_name_for_testing();
  }

  const SkPath GetArtworkClipPath() const {
    return media_controls_view_->GetArtworkClipPath();
  }

  LockScreenMediaControlsView* media_controls_view_ = nullptr;
  std::unique_ptr<AnimationWaiter> animation_waiter_;
  base::test::ScopedPowerMonitorTestSource test_power_monitor_source_;

 private:
  void NotifyUpdatedActions() {
    media_controls_view_->MediaSessionActionsChanged(
        std::vector<MediaSessionAction>(actions_.begin(), actions_.end()));
  }

  base::test::ScopedFeatureList feature_list;

  LockContentsView* lock_contents_view_ = nullptr;
  std::unique_ptr<TestMediaController> media_controller_;
  std::set<MediaSessionAction> actions_;
};

TEST_F(LockScreenMediaControlsViewTest, DoNotUpdateMetadataBetweenSessions) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Set metadata for current session
  media_session::MediaMetadata metadata;
  metadata.source_title = kTestAppName;
  metadata.title = u"title";
  metadata.artist = u"artist";

  media_controls_view_->MediaSessionMetadataChanged(metadata);

  // Simulate new media session starting.
  metadata.source_title = u"AppName2";
  metadata.title = u"title2";
  metadata.artist = u"artist2";

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  media_controls_view_->MediaSessionMetadataChanged(metadata);

  EXPECT_EQ(kTestAppName, GetAppName());
  EXPECT_EQ(u"title", title_label()->GetText());
  EXPECT_EQ(u"artist", artist_label()->GetText());
}

TEST_F(LockScreenMediaControlsViewTest, DoNotUpdateArtworkBetweenSessions) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorMAGENTA);

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);

  EXPECT_TRUE(artwork_view()->GetImage().isNull());
}

TEST_F(LockScreenMediaControlsViewTest,
       DoNotUpdatePlaybackStateBetweenSessions) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPaused);

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPlay));
}

TEST_F(LockScreenMediaControlsViewTest, DoNotUpdateActionsBetweenSessions) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EXPECT_FALSE(
      GetButtonForAction(MediaSessionAction::kSeekForward)->GetVisible());

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(
      GetButtonForAction(MediaSessionAction::kSeekForward)->GetVisible());
}

TEST_F(LockScreenMediaControlsViewTest, ButtonsSanityCheck) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAllActions();

  EXPECT_TRUE(button_row()->GetVisible());
  EXPECT_EQ(5u, media_action_buttons().size());

  for (int i = 0; i < 5; /* size of |button_row| */ i++) {
    auto* child = media_action_buttons()[i];

    ASSERT_TRUE(IsMediaButtonType(child->GetClassName()));

    ASSERT_EQ(
        static_cast<MediaSessionAction>(views::Button::AsButton(child)->tag()),
        kActionButtonOrder[i]);

    EXPECT_TRUE(child->GetVisible());
    EXPECT_FALSE(views::Button::AsButton(child)->GetAccessibleName().empty());
  }

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPause));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekForward));

  // |kPlay| cannot be present if |kPause| is.
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPlay));
}

TEST_F(LockScreenMediaControlsViewTest, ButtonsFocusCheck) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAllActions();

  views::FocusManager* focus_manager = media_controls_view_->GetFocusManager();

  {
    // Focus the first action button - the close button.
    focus_manager->SetFocusedView(close_button());
    EXPECT_EQ(close_button(), focus_manager->GetFocusedView());
  }

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kPreviousTrack),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekBackward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kPause),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekForward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kNextTrack),
            focus_manager->GetFocusedView());
}

TEST_F(LockScreenMediaControlsViewTest, PlayPauseButtonTooltipCheck) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  auto* button = GetButtonForAction(MediaSessionAction::kPause);
  std::u16string tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(tooltip.empty());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  media_controls_view_->MediaSessionInfoChanged(session_info.Clone());

  std::u16string new_tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(new_tooltip.empty());
  EXPECT_NE(tooltip, new_tooltip);
}

TEST_F(LockScreenMediaControlsViewTest, ProgressBarVisibility) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify that the progress is not initially visible.
  EXPECT_FALSE(progress_view()->GetVisible());

  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  // Simulate position changing.
  media_controls_view_->MediaSessionPositionChanged(media_position);

  // Verify that the progress is now visible.
  EXPECT_TRUE(progress_view()->GetVisible());

  // Simulate position turning null.
  media_controls_view_->MediaSessionPositionChanged(absl::nullopt);

  // Verify that the progress is hidden again.
  EXPECT_FALSE(progress_view()->GetVisible());
}

TEST_F(LockScreenMediaControlsViewTest, CloseButtonVisibility) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EXPECT_TRUE(media_controls_view_->IsDrawn());
  EXPECT_TRUE(close_button()->IsDrawn());
  EXPECT_FALSE(CloseButtonHasImage());

  // Move the mouse inside |media_controls_view_|.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      media_controls_view_->GetBoundsInScreen().CenterPoint());

  // Verify that the close button is shown.
  EXPECT_TRUE(media_controls_view_->IsDrawn());
  EXPECT_TRUE(close_button()->IsDrawn());
  EXPECT_TRUE(CloseButtonHasImage());

  // Move the mouse outside |media_controls_view_|.
  generator->MoveMouseBy(500, 500);

  // Verify that the close button is hidden.
  EXPECT_TRUE(media_controls_view_->IsDrawn());
  EXPECT_TRUE(close_button()->IsDrawn());
  EXPECT_FALSE(CloseButtonHasImage());

  // Focusing the close button should show it.
  views::FocusManager* focus_manager = media_controls_view_->GetFocusManager();
  focus_manager->SetFocusedView(close_button());
  EXPECT_EQ(close_button(), focus_manager->GetFocusedView());
  EXPECT_TRUE(media_controls_view_->IsDrawn());
  EXPECT_TRUE(close_button()->IsDrawn());
  EXPECT_TRUE(CloseButtonHasImage());

  // Move focus somewhere else and the close button should hide.
  SimulateTab();
  EXPECT_NE(close_button(), focus_manager->GetFocusedView());
  EXPECT_TRUE(media_controls_view_->IsDrawn());
  EXPECT_TRUE(close_button()->IsDrawn());
  EXPECT_FALSE(CloseButtonHasImage());
}

TEST_F(LockScreenMediaControlsViewTest, CloseButtonClick) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EXPECT_TRUE(media_controls_view_->IsDrawn());

  // Move the mouse inside |media_controls_view_|.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      media_controls_view_->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(close_button()->IsDrawn());
  EXPECT_EQ(0, media_controller()->stop_count());

  // Send event to click the close button.
  generator->MoveMouseTo(close_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // Verify that the media was stopped.
  media_controls_view_->FlushForTesting();
  EXPECT_EQ(1, media_controller()->stop_count());

  // Verify that the controls were hidden.
  EXPECT_FALSE(media_controls_view_->IsDrawn());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kStop, 1);
}

TEST_F(LockScreenMediaControlsViewTest, PreviousTrackButtonClick) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_EQ(0, media_controller()->previous_track_count());

  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->previous_track_count());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kPreviousTrack, 1);
}

TEST_F(LockScreenMediaControlsViewTest, PlayButtonClick) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kPlay);

  EXPECT_EQ(0, media_controller()->resume_count());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  media_controls_view_->MediaSessionInfoChanged(session_info.Clone());

  SimulateButtonClick(MediaSessionAction::kPlay);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->resume_count());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kPlay, 1);
}

TEST_F(LockScreenMediaControlsViewTest, PauseButtonClick) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kPause);

  EXPECT_EQ(0, media_controller()->suspend_count());

  SimulateButtonClick(MediaSessionAction::kPause);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->suspend_count());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kPause, 1);
}

TEST_F(LockScreenMediaControlsViewTest, NextTrackButtonClick) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_EQ(0, media_controller()->next_track_count());

  SimulateButtonClick(MediaSessionAction::kNextTrack);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->next_track_count());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kNextTrack, 1);
}

TEST_F(LockScreenMediaControlsViewTest, SeekBackwardButtonClick) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_EQ(0, media_controller()->seek_backward_count());

  SimulateButtonClick(MediaSessionAction::kSeekBackward);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_backward_count());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kSeekBackward, 1);
}

TEST_F(LockScreenMediaControlsViewTest, SeekForwardButtonClick) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_EQ(0, media_controller()->seek_forward_count());

  SimulateButtonClick(MediaSessionAction::kSeekForward);
  media_controls_view_->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_forward_count());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kSeekForward, 1);
}

TEST_F(LockScreenMediaControlsViewTest, UpdateAppIcon) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  const bool should_use_dark_color =
      features::IsDarkLightModeEnabled() &&
      DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
  gfx::ImageSkia default_icon = gfx::CreateVectorIcon(
      message_center::kProductIcon, kAppIconSize,
      should_use_dark_color ? gfx::kGoogleGrey500 : gfx::kGoogleGrey700);

  // Verify that the icon is initialized to the default.
  EXPECT_TRUE(icon_view()->GetImage().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().width());
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().height());

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, SkBitmap());

  // Verify that the default icon is used if no icon is provided.
  EXPECT_TRUE(icon_view()->GetImage().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().width());
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().height());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(kAppIconSize, kAppIconSize);
  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, bitmap);

  // Verify that the provided icon is used.
  EXPECT_FALSE(icon_view()->GetImage().BackedBySameObjectAs(default_icon));
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().width());
  EXPECT_EQ(kAppIconSize, icon_view()->GetImage().height());
}

TEST_F(LockScreenMediaControlsViewTest, UpdateMetadata) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify that the app name is initialized to the default.
  EXPECT_EQ(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      GetAppName());

  media_session::MediaMetadata metadata;
  media_controls_view_->MediaSessionMetadataChanged(metadata);

  // Verify that default app name is used if no name is provided.
  EXPECT_EQ(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      GetAppName());

  metadata.source_title = kTestAppName;
  metadata.title = u"title";
  metadata.artist = u"artist";

  media_controls_view_->MediaSessionMetadataChanged(metadata);

  // Verify that the provided data is used.
  EXPECT_EQ(kTestAppName, GetAppName());
  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.artist, artist_label()->GetText());
}

TEST_F(LockScreenMediaControlsViewTest, UpdateImagesConvertColors) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  SkBitmap artwork;
  SkImageInfo artwork_info =
      SkImageInfo::Make(200, 200, kAlpha_8_SkColorType, kOpaque_SkAlphaType);
  artwork.allocPixels(artwork_info);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  // Verify the artwork color was converted.
  EXPECT_EQ(artwork_view()->GetImage().bitmap()->colorType(), kN32_SkColorType);

  // Verify the artwork is visible.
  EXPECT_TRUE(artwork_view()->GetVisible());

  SkBitmap icon;
  SkImageInfo icon_info =
      SkImageInfo::Make(20, 20, kAlpha_8_SkColorType, kOpaque_SkAlphaType);
  artwork.allocPixels(icon_info);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, icon);

  // Verify the icon color was converted.
  EXPECT_EQ(icon_view()->GetImage().bitmap()->colorType(), kN32_SkColorType);

  // Verify the icon is visible.
  EXPECT_TRUE(icon_view()->GetVisible());
}

TEST_F(LockScreenMediaControlsViewTest, UpdateArtwork) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify that the artwork is initially empty.
  EXPECT_TRUE(artwork_view()->GetImage().isNull());

  // Create artwork that must be scaled down to fit the view.
  SkBitmap artwork;
  artwork.allocN32Pixels(200, 100);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  {
    // Verify that the provided artwork is correctly scaled down.
    gfx::Rect expected_artwork_bounds(0, 12, 48, 24);
    gfx::Rect artwork_bounds = artwork_view()->GetImageBounds();
    EXPECT_EQ(expected_artwork_bounds, artwork_bounds);

    // Check the clip path uses the artwork bounds.
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(expected_artwork_bounds),
                      kArtworkCornerRadius, kArtworkCornerRadius);
    EXPECT_EQ(path, GetArtworkClipPath());
  }

  // Create artwork that must be scaled up to fit the view.
  artwork.allocN32Pixels(20, 40);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  {
    // Verify that the provided artwork is correctly scaled up.
    gfx::Rect expected_artwork_bounds(12, 0, 24, 48);
    gfx::Rect artwork_bounds = artwork_view()->GetImageBounds();
    EXPECT_EQ(expected_artwork_bounds, artwork_bounds);

    // Check the clip path uses the artwork bounds.
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(expected_artwork_bounds),
                      kArtworkCornerRadius, kArtworkCornerRadius);
    EXPECT_EQ(path, GetArtworkClipPath());
  }

  // Create artwork that already fits the view size.
  artwork.allocN32Pixels(30, kArtworkViewHeight);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  {
    // Verify that the provided artwork size doesn't change.
    gfx::Rect expected_artwork_bounds(9, 0, 30, 48);
    gfx::Rect artwork_bounds = artwork_view()->GetImageBounds();
    EXPECT_EQ(expected_artwork_bounds, artwork_bounds);

    // Check the clip path uses the artwork bounds.
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(expected_artwork_bounds),
                      kArtworkCornerRadius, kArtworkCornerRadius);
    EXPECT_EQ(path, GetArtworkClipPath());
  }
}

TEST_F(LockScreenMediaControlsViewTest, ArtworkVisibility) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EXPECT_FALSE(artwork_view()->GetVisible());

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorMAGENTA);

  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);
  EXPECT_TRUE(artwork_view()->GetVisible());

  // Don't hide artwork immediately after getting null image.
  image.reset();
  media_controls_view_->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);
  EXPECT_TRUE(artwork_view()->GetVisible());
}

TEST_F(LockScreenMediaControlsViewTest, AccessibleNodeData) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  ui::AXNodeData data;
  media_controls_view_->GetAccessibleNodeData(&data);

  // Verify that the accessible name is initially empty.
  EXPECT_FALSE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  media_controls_view_->MediaSessionMetadataChanged(metadata);
  media_controls_view_->GetAccessibleNodeData(&data);

  // Verify that the accessible name updates with the metadata.
  EXPECT_TRUE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(u"title - artist",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(LockScreenMediaControlsViewTest, DismissControlsVelocity) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  gfx::Point scroll_start(
      media_controls_view_->GetBoundsInScreen().CenterPoint());
  gfx::Point scroll_end(scroll_start.x() + 50, scroll_start.y());

  // Simulate scroll with velocity past the threshold.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(scroll_start, scroll_end,
                                   base::Milliseconds(100), 3);

  animation_waiter_->Wait();

  // Verify the controls were hidden.
  EXPECT_FALSE(media_controls_view_->IsDrawn());
}

TEST_F(LockScreenMediaControlsViewTest, DismissControlsDistance) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  gfx::Point scroll_start(
      media_controls_view_->GetBoundsInScreen().CenterPoint());
  gfx::Point scroll_end(media_controls_view_->GetBoundsInScreen().right() - 10,
                        scroll_start.y());

  // Simulate scroll with distance past the threshold.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(scroll_start, scroll_end, base::Seconds(3),
                                   3);

  animation_waiter_->Wait();

  // Verify the controls were hidden.
  EXPECT_FALSE(media_controls_view_->IsDrawn());
}

TEST_F(LockScreenMediaControlsViewTest, DragReset) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify |contents_view()| is in its initial position.
  EXPECT_EQ(media_controls_view_->GetBoundsInScreen(),
            contents_view()->GetBoundsInScreen());
  EXPECT_TRUE(media_controls_view_->IsDrawn());

  gfx::Point scroll_start(
      media_controls_view_->GetBoundsInScreen().CenterPoint());
  gfx::Point scroll_end(scroll_start.x() + 10, scroll_start.y());

  // Simulate scroll with neither distance nor velocity past the thresholds.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(scroll_start, scroll_end, base::Seconds(3),
                                   3);

  animation_waiter_->Wait();

  // Verify |contents_view()| is reset to its initial position.
  EXPECT_EQ(media_controls_view_->GetBoundsInScreen(),
            contents_view()->GetBoundsInScreen());
  EXPECT_TRUE(media_controls_view_->IsDrawn());
}

TEST_F(LockScreenMediaControlsViewTest, DragBounds) {
  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify |contents_view()| is in its initial position.
  EXPECT_EQ(media_controls_view_->GetBoundsInScreen(),
            contents_view()->GetBoundsInScreen());
  EXPECT_TRUE(media_controls_view_->IsDrawn());

  gfx::Point scroll_start(
      media_controls_view_->GetBoundsInScreen().CenterPoint());
  gfx::Point scroll_end(scroll_start.x() - 10, scroll_start.y());

  // Simulate scroll that attempts to go below the view bounds.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(scroll_start, scroll_end, base::Seconds(3),
                                   3);

  animation_waiter_->Wait();

  // Verify |contents_view()| does not go below its initial position.
  EXPECT_EQ(media_controls_view_->GetBoundsInScreen(),
            contents_view()->GetBoundsInScreen());
  EXPECT_TRUE(media_controls_view_->IsDrawn());
}

TEST_F(LockScreenMediaControlsViewTest, SeekToClick) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EXPECT_EQ(0, media_controller()->seek_to_count());

  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);

  // Simulate initial position change.
  media_controls_view_->MediaSessionPositionChanged(media_position);

  // Click exactly halfway on the progress bar.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(progress_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // Verify the media was seeked to its halfway point.
  media_controls_view_->FlushForTesting();
  EXPECT_EQ(1, media_controller()->seek_to_count());
  EXPECT_EQ(base::Seconds(300), media_controller()->seek_to_time());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kSeekTo, 1);
}

TEST_F(LockScreenMediaControlsViewTest, SeekToTouch) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  EXPECT_EQ(0, media_controller()->seek_to_count());

  media_session::MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(100), /*end_of_media=*/false);

  // Simulate initial position change.
  media_controls_view_->MediaSessionPositionChanged(media_position);

  // Tap exactly halfway on the progress bar.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(progress_view()->GetBoundsInScreen().CenterPoint());

  // Verify the media was seeked to its halfway point.
  media_controls_view_->FlushForTesting();
  EXPECT_EQ(1, media_controller()->seek_to_count());
  EXPECT_EQ(base::Seconds(300), media_controller()->seek_to_time());

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsUserActionHistogramName,
      MediaSessionAction::kSeekTo, 1);
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Shown_ControlsDisabled) {
  base::HistogramTester tester;

  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kLockScreenMediaControls);

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kNotShownControlsDisabled, 1);
  EXPECT_EQ(
      0U, tester
              .GetAllSamples(
                  LockScreenMediaControlsView::kMediaControlsHideHistogramName)
              .size());
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Shown_NoSession) {
  base::HistogramTester tester;

  media_controls_view_->MediaSessionInfoChanged(nullptr);

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kNotShownNoSession, 1);
  EXPECT_EQ(
      0U, tester
              .GetAllSamples(
                  LockScreenMediaControlsView::kMediaControlsHideHistogramName)
              .size());
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Shown_SessionPaused) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPaused);

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kNotShownSessionPaused, 1);
  EXPECT_EQ(
      0U, tester
              .GetAllSamples(
                  LockScreenMediaControlsView::kMediaControlsHideHistogramName)
              .size());
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Shown_Visible) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kShown, 1);
  EXPECT_EQ(
      1U, tester
              .GetAllSamples(
                  LockScreenMediaControlsView::kMediaControlsHideHistogramName)
              .size());
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Shown_SessionSensitive) {
  base::HistogramTester tester;

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_sensitive = true;

  media_controls_view_->MediaSessionChanged(base::UnguessableToken::Create());
  media_controls_view_->MediaSessionInfoChanged(session_info.Clone());

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kNotShownSessionSensitive, 1);
  EXPECT_EQ(
      0U, tester
              .GetAllSamples(
                  LockScreenMediaControlsView::kMediaControlsHideHistogramName)
              .size());
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Hide_SessionChanged) {
  base::HistogramTester tester;

  auto mock_timer_unique = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* mock_timer = mock_timer_unique.get();
  media_controls_view_->set_timer_for_testing(std::move(mock_timer_unique));

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Simulate media session stopping and delay.
  media_controls_view_->MediaSessionChanged(absl::nullopt);
  mock_timer->Fire();

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsHideHistogramName,
      LockScreenMediaControlsView::HideReason::kSessionChanged, 1);
  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kShown, 1);
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Hide_DismissedByUser) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  // Move the mouse inside |media_controls_view_| and click the close button.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      media_controls_view_->GetBoundsInScreen().CenterPoint());
  generator->MoveMouseTo(close_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsHideHistogramName,
      LockScreenMediaControlsView::HideReason::kDismissedByUser, 1);
  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kShown, 1);
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Hide_Unlocked) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsHideHistogramName,
      LockScreenMediaControlsView::HideReason::kUnlocked, 1);
  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kShown, 1);
}

TEST_F(LockScreenMediaControlsViewTest, Histogram_Hide_DeviceSleep) {
  base::HistogramTester tester;

  SimulateMediaSessionChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  test_power_monitor_source_.GenerateSuspendEvent();

  SimulateSessionUnlock();

  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsHideHistogramName,
      LockScreenMediaControlsView::HideReason::kDeviceSleep, 1);
  tester.ExpectUniqueSample(
      LockScreenMediaControlsView::kMediaControlsShownHistogramName,
      LockScreenMediaControlsView::Shown::kShown, 1);
}

}  // namespace ash
