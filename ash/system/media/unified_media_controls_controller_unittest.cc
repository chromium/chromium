// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_controller.h"

#include "ash/system/media/unified_media_controls_view.h"
#include "ash/test/ash_test_base.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

using media_session::mojom::MediaSessionAction;
using media_session::test::TestMediaController;

namespace {

constexpr int kFreezeControlsTime = 2000; /* in milliseconds */
constexpr int kHideArtworkDelay = 2000;  /* in milliseconds */
constexpr int kArtworkCornerRadius = 4;
constexpr int kArtworkHeight = 40;

class MockMediaControlsDelegate
    : public UnifiedMediaControlsController::Delegate {
 public:
  MockMediaControlsDelegate() = default;
  ~MockMediaControlsDelegate() override = default;

  void ShowMediaControls() override { visible_ = true; }
  MOCK_METHOD0(OnMediaControlsViewClicked, void());

  bool IsControlsVisible() { return visible_; }

 private:
  bool visible_ = false;
};

}  // namespace

class UnifiedMediaControlsControllerTest : public AshTestBase {
 public:
  UnifiedMediaControlsControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~UnifiedMediaControlsControllerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControlsForChromeOS);
    AshTestBase::SetUp();

    mock_delegate_ = std::make_unique<MockMediaControlsDelegate>();
    controller_ =
        std::make_unique<UnifiedMediaControlsController>(mock_delegate_.get());
    media_controls_.reset(
        static_cast<UnifiedMediaControlsView*>(controller_->CreateView()));

    media_controller_ = std::make_unique<TestMediaController>();
    controller_->set_media_controller_for_testing(
        media_controller_->CreateMediaControllerRemote());
  }

  void TearDown() override {
    media_controls_.reset();
    controller_.reset();
    mock_delegate_.reset();
    widget_.reset();
    AshTestBase::TearDown();
  }

  // Create widget if we are testing views.
  void CreateWidget() {
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 400, 80);
    widget_ = std::make_unique<views::Widget>();
    widget_->Init(std::move(params));
    widget_->SetContentsView(media_controls_.get());
    widget_->Show();
  }

  void EnableAction(MediaSessionAction action) {
    actions_.insert(action);
    NotifyActionsChanged();
  }

  void DisableAction(MediaSessionAction action) {
    actions_.erase(action);
    NotifyActionsChanged();
  }

  void SimulateNewMediaSessionWithData(base::UnguessableToken request_id) {
    controller()->MediaSessionChanged(request_id);

    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->is_controllable = true;
    controller()->MediaSessionInfoChanged(session_info.Clone());

    media_session::MediaMetadata metadata;
    metadata.title = u"foo";
    controller()->MediaSessionMetadataChanged(metadata);
  }

  void SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState playback_state) {
    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->playback_state = playback_state;
    session_info->is_controllable = true;
    controller()->MediaSessionInfoChanged(session_info.Clone());
  }

  void SimulateButtonClicked(MediaSessionAction action) {
    views::Button* button = GetActionButton(action);
    EXPECT_NE(button, nullptr);
    EXPECT_TRUE(button->GetVisible() && button->GetEnabled());

    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
  }

  views::Button* GetActionButton(MediaSessionAction action) {
    const auto it =
        base::ranges::find(button_row()->children(), static_cast<int>(action),
                           [](views::View* child) {
                             return static_cast<views::Button*>(child)->tag();
                           });

    if (it == button_row()->children().end())
      return nullptr;

    return static_cast<views::Button*>(*it);
  }

  bool IsMediaControlsInEmptyState() const {
    return media_controls_->is_in_empty_state_;
  }

  SkPath GetArtworkClipPath() { return media_controls_->GetArtworkClipPath(); }

  views::View* button_row() { return media_controls_->button_row_; }

  views::Label* title_label() { return media_controls_->title_label_; }

  views::Label* artist_label() { return media_controls_->artist_label_; }

  views::ImageView* artwork_view() { return media_controls_->artwork_view_; }

  UnifiedMediaControlsController* controller() { return controller_.get(); }

  MockMediaControlsDelegate* delegate() { return mock_delegate_.get(); }

  TestMediaController* media_controller() { return media_controller_.get(); }

  UnifiedMediaControlsView* media_controls_view() {
    return media_controls_.get();
  }

 private:
  void NotifyActionsChanged() {
    controller_->MediaSessionActionsChanged(
        std::vector<MediaSessionAction>(actions_.begin(), actions_.end()));
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<UnifiedMediaControlsController> controller_;
  std::unique_ptr<UnifiedMediaControlsView> media_controls_;
  std::unique_ptr<MockMediaControlsDelegate> mock_delegate_;
  std::unique_ptr<TestMediaController> media_controller_;

  std::set<MediaSessionAction> actions_;
};

TEST_F(UnifiedMediaControlsControllerTest, ActionButtonsTest) {
  CreateWidget();
  SimulateNewMediaSessionWithData(base::UnguessableToken::Create());

  EnableAction(MediaSessionAction::kPreviousTrack);
  EnableAction(MediaSessionAction::kNextTrack);
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  // Previous track button test.
  EXPECT_EQ(0, media_controller()->previous_track_count());
  SimulateButtonClicked(MediaSessionAction::kPreviousTrack);
  controller()->FlushForTesting();
  EXPECT_EQ(1, media_controller()->previous_track_count());

  // Next track button test.
  EXPECT_EQ(0, media_controller()->next_track_count());
  SimulateButtonClicked(MediaSessionAction::kNextTrack);
  controller()->FlushForTesting();
  EXPECT_EQ(1, media_controller()->next_track_count());

  // Pause button test.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_EQ(0, media_controller()->suspend_count());
  SimulateButtonClicked(MediaSessionAction::kPause);
  controller()->FlushForTesting();
  EXPECT_EQ(1, media_controller()->suspend_count());

  // Play button test.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  EXPECT_EQ(0, media_controller()->resume_count());
  SimulateButtonClicked(MediaSessionAction::kPlay);
  controller()->FlushForTesting();
  EXPECT_EQ(1, media_controller()->resume_count());
}

TEST_F(UnifiedMediaControlsControllerTest, ActionButtonVisibility) {
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  EnableAction(MediaSessionAction::kPause);
  EnableAction(MediaSessionAction::kPlay);
  EXPECT_EQ(GetActionButton(MediaSessionAction::kPlay), nullptr);
  EXPECT_TRUE(GetActionButton(MediaSessionAction::kPause)->GetVisible());
  EXPECT_FALSE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());
  EXPECT_FALSE(GetActionButton(MediaSessionAction::kNextTrack)->GetVisible());

  EnableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_TRUE(GetActionButton(MediaSessionAction::kPause)->GetVisible());
  EXPECT_TRUE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());
  EXPECT_FALSE(GetActionButton(MediaSessionAction::kNextTrack)->GetVisible());

  EnableAction(MediaSessionAction::kNextTrack);
  DisableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_TRUE(GetActionButton(MediaSessionAction::kPause)->GetVisible());
  EXPECT_FALSE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());
  EXPECT_TRUE(GetActionButton(MediaSessionAction::kNextTrack)->GetVisible());

  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  EXPECT_EQ(GetActionButton(MediaSessionAction::kPause), nullptr);
  EXPECT_TRUE(GetActionButton(MediaSessionAction::kPlay)->GetVisible());
}

TEST_F(UnifiedMediaControlsControllerTest, MetadataUpdate) {
  SimulateNewMediaSessionWithData(base::UnguessableToken::Create());

  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  controller()->MediaSessionMetadataChanged(metadata);

  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.artist, artist_label()->GetText());
}

TEST_F(UnifiedMediaControlsControllerTest, UpdateArtworkConvertColor) {
  CreateWidget();
  SkBitmap artwork;
  SkImageInfo image_info =
      SkImageInfo::Make(200, 200, kAlpha_8_SkColorType, kOpaque_SkAlphaType);
  artwork.allocPixels(image_info);

  // Verify that color type is converted.
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  EXPECT_EQ(artwork_view()->GetImage().bitmap()->colorType(), kN32_SkColorType);
}

TEST_F(UnifiedMediaControlsControllerTest, UpdateArtwork) {
  CreateWidget();
  EXPECT_TRUE(artwork_view()->GetImage().isNull());

  SkBitmap artwork;

  // Test that artwork will be scaled down if too large.
  artwork.allocN32Pixels(200, 100);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  {
    gfx::Rect expect_bounds(-20, 0, 80, 40);
    gfx::Rect artwork_bounds = artwork_view()->GetImageBounds();
    EXPECT_EQ(artwork_bounds, expect_bounds);

    SkPath path;
    path.addRoundRect(
        gfx::RectToSkRect(gfx::Rect(0, 0, kArtworkHeight, kArtworkHeight)),
        kArtworkCornerRadius, kArtworkCornerRadius);
    EXPECT_EQ(path, GetArtworkClipPath());
  }

  // Test that artwork will be scaled up if too small.
  artwork.allocN32Pixels(20, 40);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  {
    gfx::Rect expect_bounds(0, -20, 40, 80);
    gfx::Rect artwork_bounds = artwork_view()->GetImageBounds();
    EXPECT_EQ(artwork_bounds, expect_bounds);
  }

  // Test that artwork fit right in to the artwork view.
  artwork.allocN32Pixels(60, kArtworkHeight);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  {
    gfx::Rect expect_bounds(-10, 0, 60, kArtworkHeight);
    gfx::Rect artwork_bounds = artwork_view()->GetImageBounds();
    EXPECT_EQ(artwork_bounds, expect_bounds);
  }

  // Test that artwork view will be hidden after |kHideArtworkDelay| ms if
  // we get an empty artowrk.
  artwork.reset();
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  task_environment()->FastForwardBy(base::Milliseconds(kHideArtworkDelay));
  EXPECT_FALSE(artwork_view()->GetVisible());
}

// Test that artwork views hides after a certain delay
// when received a null artwork image.
TEST_F(UnifiedMediaControlsControllerTest, HideArtwork) {
  // Artwork view starts hidden.
  EXPECT_FALSE(artwork_view()->GetVisible());

  // Artwork view should be visible after getting an artowrk image update.
  SkBitmap artwork;
  artwork.allocN32Pixels(40, 40);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  EXPECT_TRUE(artwork_view()->GetVisible());

  // Artwork should still be visible after getting an empty artowrk.
  artwork.reset();
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  EXPECT_TRUE(artwork_view()->GetVisible());

  // Artwork should still be visible if we are within hide artwork delay
  // time frame.
  task_environment()->FastForwardBy(base::Milliseconds(kHideArtworkDelay - 1));
  EXPECT_TRUE(artwork_view()->GetVisible());

  // Artwork should be visible after getting an artwork update and the
  // timer should be stopped.
  artwork.allocN32Pixels(40, 40);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  EXPECT_TRUE(artwork_view()->GetVisible());

  // Artwork should stay visible.
  task_environment()->FastForwardBy(base::Milliseconds(kHideArtworkDelay));
  EXPECT_TRUE(artwork_view()->GetVisible());

  // Wait for |kHideartworkDelay| ms after getting an empty artwork,
  // artwork view should now be hidden.
  artwork.reset();
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  task_environment()->FastForwardBy(base::Milliseconds(kHideArtworkDelay));
  EXPECT_FALSE(artwork_view()->GetVisible());
}

TEST_F(UnifiedMediaControlsControllerTest,
       UpdateControlsStateOnMediaSessionChanged) {
  auto request_id = base::UnguessableToken::Create();

  EXPECT_FALSE(delegate()->IsControlsVisible());

  // Don't show controls if we don't have session info.
  controller()->MediaSessionChanged(request_id);
  EXPECT_FALSE(delegate()->IsControlsVisible());

  // Test that we need to session info to display the controls.
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->is_controllable = true;
  controller()->MediaSessionInfoChanged(session_info.Clone());
  EXPECT_TRUE(delegate()->IsControlsVisible());

  // Test that we should not display a non-controllable session.
  session_info->is_controllable = false;
  controller()->MediaSessionInfoChanged(session_info.Clone());
  media_session::MediaMetadata metadata;
  metadata.title = u"foo";
  controller()->MediaSessionMetadataChanged(metadata);
  EXPECT_TRUE(IsMediaControlsInEmptyState());

  // Controls should show with metadata and controllable session.
  SimulateNewMediaSessionWithData(request_id);
  EXPECT_TRUE(delegate()->IsControlsVisible());
  EXPECT_FALSE(IsMediaControlsInEmptyState());

  controller()->MediaSessionChanged(absl::nullopt);
  EXPECT_FALSE(IsMediaControlsInEmptyState());

  // Still in normal state since we are within waiting delay time frame.
  task_environment()->FastForwardBy(
      base::Milliseconds(kFreezeControlsTime - 1));
  EXPECT_FALSE(IsMediaControlsInEmptyState());

  // Session resumes, controls should still be in normal state.
  SimulateNewMediaSessionWithData(request_id);
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(IsMediaControlsInEmptyState());

  // Hide controls timer expired, controls should be in empty state.
  controller()->MediaSessionChanged(absl::nullopt);
  task_environment()->FastForwardBy(base::Milliseconds(kFreezeControlsTime));
  EXPECT_TRUE(IsMediaControlsInEmptyState());
  EXPECT_TRUE(delegate()->IsControlsVisible());
}

TEST_F(UnifiedMediaControlsControllerTest, MediaControlsEmptyState) {
  CreateWidget();

  // Show media controls.
  auto request_id = base::UnguessableToken::Create();
  SimulateNewMediaSessionWithData(request_id);
  EXPECT_TRUE(delegate()->IsControlsVisible());
  EXPECT_FALSE(IsMediaControlsInEmptyState());

  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);
  EnableAction(MediaSessionAction::kPreviousTrack);
  EnableAction(MediaSessionAction::kNextTrack);

  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  controller()->MediaSessionMetadataChanged(metadata);

  EXPECT_TRUE(artist_label()->GetVisible());
  EXPECT_FALSE(artwork_view()->GetVisible());
  for (views::View* button : button_row()->children())
    EXPECT_TRUE(button->GetEnabled());

  // Media controls should be in empty state after getting empty session.
  controller()->MediaSessionChanged(absl::nullopt);
  task_environment()->FastForwardBy(base::Milliseconds(kFreezeControlsTime));

  EXPECT_TRUE(IsMediaControlsInEmptyState());

  // When in empty state, artist label should be hidden; artwork view
  // should be hidden since it was hidden before getting into empty
  // state; all action buttons should be disabled.
  EXPECT_FALSE(artist_label()->GetVisible());
  EXPECT_FALSE(artwork_view()->GetVisible());
  for (views::View* button : button_row()->children())
    EXPECT_FALSE(button->GetEnabled());

  // Tapping on the media controls when we are in empty state should not
  // notify delegate.
  EXPECT_CALL(*delegate(), OnMediaControlsViewClicked).Times(0);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      media_controls_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // Media controls should get back to normal state.
  SimulateNewMediaSessionWithData(request_id);
  EXPECT_FALSE(IsMediaControlsInEmptyState());

  EXPECT_FALSE(artwork_view()->GetVisible());
  for (views::View* button : button_row()->children())
    EXPECT_TRUE(button->GetEnabled());

  // User should be able to tap the controls for detailed view again.
  EXPECT_CALL(*delegate(), OnMediaControlsViewClicked).Times(1);
  generator->ClickLeftButton();
}

TEST_F(UnifiedMediaControlsControllerTest, MediaControlsEmptyStateWithArtwork) {
  auto request_id = base::UnguessableToken::Create();

  EXPECT_FALSE(delegate()->IsControlsVisible());
  SimulateNewMediaSessionWithData(request_id);
  EXPECT_TRUE(delegate()->IsControlsVisible());
  EXPECT_FALSE(IsMediaControlsInEmptyState());

  // Artwork changed, and artwork view should have an empty background in normal
  // state.
  SkBitmap artwork;
  artwork.allocN32Pixels(40, 40);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  EXPECT_TRUE(artwork_view()->GetVisible());
  EXPECT_EQ(artwork_view()->background(), nullptr);

  controller()->MediaSessionChanged(absl::nullopt);
  task_environment()->FastForwardBy(base::Milliseconds(kFreezeControlsTime));

  // Artwork view should still be visible and have an background in empty state.
  EXPECT_TRUE(IsMediaControlsInEmptyState());
  EXPECT_TRUE(artwork_view()->GetVisible());
  EXPECT_NE(artwork_view()->background(), nullptr);

  // Session and artwork updated, artwotk view should be back in normal state.
  SimulateNewMediaSessionWithData(request_id);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  EXPECT_TRUE(artwork_view()->GetVisible());
  EXPECT_EQ(artwork_view()->background(), nullptr);
}

TEST_F(UnifiedMediaControlsControllerTest, FreezeControlsWhenUpdateSession) {
  auto request_id = base::UnguessableToken::Create();
  SimulateNewMediaSessionWithData(request_id);
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  media_session::MediaMetadata init_metadata;
  init_metadata.title = u"init_title";
  init_metadata.artist = u"init_artist";
  controller()->MediaSessionMetadataChanged(init_metadata);

  // Initial state of media controls.
  EXPECT_TRUE(GetActionButton(MediaSessionAction::kPause)->GetVisible());
  EXPECT_FALSE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());
  EXPECT_EQ(title_label()->GetText(), init_metadata.title);
  EXPECT_EQ(artist_label()->GetText(), init_metadata.artist);
  EXPECT_FALSE(artwork_view()->GetVisible());

  controller()->MediaSessionChanged(absl::nullopt);

  // Test that metadata update is ignored when we waiting for new session.
  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  controller()->MediaSessionMetadataChanged(metadata);

  EXPECT_EQ(title_label()->GetText(), init_metadata.title);
  EXPECT_EQ(artist_label()->GetText(), init_metadata.artist);

  // Test that media seeesion info update is ignored when we waiting for new
  // session.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  EXPECT_TRUE(GetActionButton(MediaSessionAction::kPause)->GetVisible());

  // Test that artwork update is ignored when we waiting for new session.
  SkBitmap artwork;
  artwork.allocN32Pixels(40, 40);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  EXPECT_FALSE(artwork_view()->GetVisible());

  // Test that enabled action update is ignored when we waiting for new session.
  EnableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_FALSE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());

  // Resume the session, now we should start updating controls.
  controller()->MediaSessionChanged(request_id);

  // Test that metadata is updated.
  controller()->MediaSessionMetadataChanged(metadata);
  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.artist, artist_label()->GetText());

  // Test that media session info is updated.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  EXPECT_EQ(GetActionButton(MediaSessionAction::kPause), nullptr);
  EXPECT_NE(GetActionButton(MediaSessionAction::kPlay), nullptr);

  // Test that artwork is updated.
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);
  EXPECT_TRUE(artwork_view()->GetVisible());

  // Test that enabled actions are updated.
  EnableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_TRUE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());
}

TEST_F(UnifiedMediaControlsControllerTest, FreezeControlsBetweenSessions) {
  auto request_id = base::UnguessableToken::Create();
  controller()->MediaSessionChanged(request_id);

  EnableAction(MediaSessionAction::kPreviousTrack);
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);

  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  metadata.artist = u"artist";
  controller()->MediaSessionMetadataChanged(metadata);

  // Verify initial state
  EXPECT_TRUE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());
  EXPECT_NE(GetActionButton(MediaSessionAction::kPause), nullptr);
  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.artist, artist_label()->GetText());
  EXPECT_FALSE(artwork_view()->GetVisible());

  // Receive a new session with new data.
  auto new_request_id = base::UnguessableToken::Create();
  controller()->MediaSessionChanged(new_request_id);

  DisableAction(MediaSessionAction::kPreviousTrack);
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);

  media_session::MediaMetadata new_metadata;
  new_metadata.title = u"different title";
  new_metadata.artist = u"different artist";
  controller()->MediaSessionMetadataChanged(new_metadata);

  SkBitmap artwork;
  artwork.allocN32Pixels(40, 40);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  // Session resumes within freezing timeout.
  task_environment()->FastForwardBy(
      base::Milliseconds(kFreezeControlsTime - 1));
  controller()->MediaSessionChanged(request_id);

  // Media controls should not be updated.
  EXPECT_TRUE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());
  EXPECT_NE(GetActionButton(MediaSessionAction::kPause), nullptr);
  EXPECT_EQ(metadata.title, title_label()->GetText());
  EXPECT_EQ(metadata.artist, artist_label()->GetText());
  EXPECT_FALSE(artwork_view()->GetVisible());

  // Receive new session and data.
  controller()->MediaSessionChanged(new_request_id);
  DisableAction(MediaSessionAction::kPreviousTrack);
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  controller()->MediaSessionMetadataChanged(new_metadata);
  controller()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, artwork);

  // Controls should be updated after freeze timeout.
  task_environment()->FastForwardBy(base::Milliseconds(kFreezeControlsTime));
  EXPECT_FALSE(
      GetActionButton(MediaSessionAction::kPreviousTrack)->GetVisible());
  EXPECT_EQ(GetActionButton(MediaSessionAction::kPause), nullptr);
  EXPECT_EQ(new_metadata.title, title_label()->GetText());
  EXPECT_EQ(new_metadata.artist, artist_label()->GetText());
  EXPECT_TRUE(artwork_view()->GetVisible());
}

TEST_F(UnifiedMediaControlsControllerTest,
       NotifyDelegateWhenMediaControlsViewClicked) {
  CreateWidget();

  EXPECT_CALL(*delegate(), OnMediaControlsViewClicked);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      media_controls_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
}

TEST_F(UnifiedMediaControlsControllerTest, ArtistVisibility) {
  auto request_id = base::UnguessableToken::Create();
  controller()->MediaSessionChanged(request_id);

  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  controller()->MediaSessionMetadataChanged(metadata);

  // Artist label should be hidden if empty.
  EXPECT_FALSE(artist_label()->GetVisible());

  metadata.artist = u"artist";
  controller()->MediaSessionMetadataChanged(metadata);
  EXPECT_TRUE(artist_label()->GetVisible());
}

TEST_F(UnifiedMediaControlsControllerTest,
       FallbackToSourceTitleWhenTitleIsEmpty) {
  auto request_id = base::UnguessableToken::Create();
  SimulateNewMediaSessionWithData(request_id);

  // Simulate metadata update with empty title.
  media_session::MediaMetadata metadata;
  metadata.source_title = u"source title";
  metadata.title = u"";
  controller()->MediaSessionMetadataChanged(metadata);

  // Title label should display source title instead.
  EXPECT_EQ(title_label()->GetText(), metadata.source_title);
}

}  // namespace ash
