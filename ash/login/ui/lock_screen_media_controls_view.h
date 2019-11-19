// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/button/button.h"

namespace service_manager {
class Connector;
}

namespace views {
class Label;
class ImageView;
}  // namespace views

namespace media_message_center {
class MediaControlsProgressView;
}

namespace ash {

namespace {
class MediaActionButton;
}  // namespace

class MediaControlsHeaderView;
class NonAccessibleView;

class ASH_EXPORT LockScreenMediaControlsView
    : public views::View,
      public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver,
      public views::ButtonListener,
      public ui::ImplicitAnimationObserver {
 public:
  // The name of the histogram that records the reason why the controls were
  // hidden.
  static const char kMediaControlsHideHistogramName[];

  // The name of the histogram that records whether the media controls were
  // shown and the reason why.
  static const char kMediaControlsShownHistogramName[];

  // The name of the histogram that records when a user interacts with the
  // media controls.
  static const char kMediaControlsUserActionHistogramName[];

  using MediaControlsEnabled = base::RepeatingCallback<bool()>;

  // The reason why the media controls were hidden. This is recorded in
  // metrics and new values should only be added to the end.
  enum class HideReason {
    kSessionChanged,
    kDismissedByUser,
    kUnlocked,
    kMaxValue = kUnlocked
  };

  // Whether the controls were shown or not shown and the reason why. This is
  // recorded in metrics and new values should only be added to the end.
  enum class Shown {
    kNotShownControlsDisabled,
    kNotShownNoSession,
    kNotShownSessionPaused,
    kShown,
    kNotShownSessionSensitive,
    kMaxValue = kNotShownSessionSensitive
  };

  struct Callbacks {
    Callbacks();
    ~Callbacks();

    // Called in |MediaSessionInfoChanged| to determine the visibility of the
    // media controls.
    MediaControlsEnabled media_controls_enabled;

    // Called when the controls should be hidden on the lock screen.
    base::RepeatingClosure hide_media_controls;

    // Called when the controls should be drawn on the lock screen.
    base::RepeatingClosure show_media_controls;

    DISALLOW_COPY_AND_ASSIGN(Callbacks);
  };

  LockScreenMediaControlsView(service_manager::Connector* connector,
                              const Callbacks& callbacks);
  ~LockScreenMediaControlsView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  views::View* GetMiddleSpacingView();

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override;
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override;

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  void FlushForTesting();

  void set_media_controller_for_testing(
      mojo::Remote<media_session::mojom::MediaController> controller) {
    media_controller_remote_ = std::move(controller);
  }

  void set_timer_for_testing(std::unique_ptr<base::OneShotTimer> test_timer) {
    hide_controls_timer_ = std::move(test_timer);
  }

 private:
  friend class LockScreenMediaControlsViewTest;

  // Hide the controls because of |reason|.
  void Hide(HideReason reason);

  void HideArtwork();

  // Set whether the controls should be shown and record the reason why.
  void SetShown(Shown shown);

  // Performs "SeekTo" through |media_controller_ptr_|. The seek time is
  // calculated using |seek_progress| and the total duration of the media.
  void SeekTo(double seek_progress);

  // Hides the controls and stops media playback.
  void Dismiss();

  // Sets the media artwork to |img|. If |img| is nullopt, the default artwork
  // is set instead.
  void SetArtwork(base::Optional<gfx::ImageSkia> img);

  // Returns the rounded rectangle clip path for the current artwork.
  SkPath GetArtworkClipPath() const;

  // Updates the visibility of buttons on |button_row_| depending on what is
  // available in the current media session.
  void UpdateActionButtonsVisibility();

  // Toggles the media play/pause button between "play" and "pause" as
  // necessary.
  void SetIsPlaying(bool playing);

  // Updates the y position and opacity of |contents_view_| during dragging.
  void UpdateDrag(const gfx::Point& location_in_screen);

  // If the drag velocity is past the threshold or the drag position is past
  // the height threshold, this calls |HideControlsAnimation()|. Otherwise, this
  // will call |ResetControlsAnimation()|.
  void EndDrag();

  // Updates the opacity of |contents_view_| based on its current position.
  void UpdateOpacity();

  // Animates |contents_view_| up and off the screen.
  void RunHideControlsAnimation();

  // Animates |contents_view_| to its original position.
  void RunResetControlsAnimation();

  // Used to connect to the Media Session service.
  service_manager::Connector* const connector_;

  // Used to control the active session.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      observer_receiver_{this};

  // Used to receive updates to the active media's icon.
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      icon_observer_receiver_{this};

  // Used to receive updates to the active media's artwork.
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      artwork_observer_receiver_{this};

  // The id of the current media session. It will be null if there is not
  // a current session.
  base::Optional<base::UnguessableToken> media_session_id_;

  // The MediaPosition associated with the current media session.
  base::Optional<media_session::MediaPosition> position_;

  // Spacing between controls and user.
  std::unique_ptr<views::View> middle_spacing_;

  // Automatically hides the controls a few seconds if no media playing.
  std::unique_ptr<base::OneShotTimer> hide_controls_timer_;

  // Make artwork view invisible if there is no artwork update after receiving
  // an empty artwork.
  std::unique_ptr<base::OneShotTimer> hide_artwork_timer_;

  // Caches the text to be read by screen readers describing the media controls
  // view.
  base::string16 accessible_name_;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  // Contains the visible and draggable UI of the media controls.
  views::View* contents_view_ = nullptr;

  // The reason we hid the media controls.
  base::Optional<HideReason> hide_reason_;

  // Whether the controls were shown or not and the reason why.
  base::Optional<Shown> shown_;

  // Container views attached to |contents_view_|.
  MediaControlsHeaderView* header_row_ = nullptr;
  views::ImageView* session_artwork_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::Label* artist_label_ = nullptr;
  NonAccessibleView* button_row_ = nullptr;
  MediaActionButton* play_pause_button_ = nullptr;
  media_message_center::MediaControlsProgressView* progress_ = nullptr;

  std::vector<views::Button*> media_action_buttons_;

  // Callbacks.
  const MediaControlsEnabled media_controls_enabled_;
  const base::RepeatingClosure hide_media_controls_;
  const base::RepeatingClosure show_media_controls_;

  // The location of the initial gesture event in screen coordinates.
  gfx::Point initial_drag_point_;

  // The velocity of the gesture event.
  float last_fling_velocity_ = 0;

  // True if the user is in the process of gesture-dragging |contents_view_|.
  bool is_in_drag_ = false;

  DISALLOW_COPY_AND_ASSIGN(LockScreenMediaControlsView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
