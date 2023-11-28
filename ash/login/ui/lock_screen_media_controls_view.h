// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace views {
class Button;
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
      public base::PowerSuspendObserver,
      public ui::ImplicitAnimationObserver {
 public:
  METADATA_HEADER(LockScreenMediaControlsView);

  using MediaControlsEnabled = base::RepeatingCallback<bool()>;

  // The reason why the media controls were hidden. This is recorded in
  // metrics and new values should only be added to the end.
  enum class HideReason {
    kSessionChanged,
    kDismissedByUser,
    kUnlocked,
    kDeviceSleep,
    kMaxValue = kDeviceSleep
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

    Callbacks(const Callbacks&) = delete;
    Callbacks& operator=(const Callbacks&) = delete;

    ~Callbacks();

    // Called in |MediaSessionInfoChanged| to determine the visibility of the
    // media controls.
    MediaControlsEnabled media_controls_enabled;

    // Called when the controls should be hidden on the lock screen.
    base::RepeatingClosure hide_media_controls;

    // Called when the controls should be drawn on the lock screen.
    base::RepeatingClosure show_media_controls;
  };

  explicit LockScreenMediaControlsView(const Callbacks& callbacks);

  LockScreenMediaControlsView(const LockScreenMediaControlsView&) = delete;
  LockScreenMediaControlsView& operator=(const LockScreenMediaControlsView&) =
      delete;

  ~LockScreenMediaControlsView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override;
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // base::PowerSuspendObserver:
  void OnSuspend() override;

  void ButtonPressed(media_session::mojom::MediaSessionAction action);

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
  void SetArtwork(std::optional<gfx::ImageSkia> img);

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
  std::optional<base::UnguessableToken> media_session_id_;

  // The MediaPosition associated with the current media session.
  std::optional<media_session::MediaPosition> position_;

  // Automatically hides the controls a few seconds if no media playing.
  std::unique_ptr<base::OneShotTimer> hide_controls_timer_ =
      std::make_unique<base::OneShotTimer>();

  // Make artwork view invisible if there is no artwork update after receiving
  // an empty artwork.
  std::unique_ptr<base::OneShotTimer> hide_artwork_timer_ =
      std::make_unique<base::OneShotTimer>();

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  // Contains the visible and draggable UI of the media controls.
  raw_ptr<views::View, ExperimentalAsh> contents_view_ = nullptr;

  // Whether the controls were shown or not and the reason why.
  std::optional<Shown> shown_;

  // Container views attached to |contents_view_|.
  raw_ptr<MediaControlsHeaderView, ExperimentalAsh> header_row_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> session_artwork_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> title_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> artist_label_ = nullptr;
  raw_ptr<NonAccessibleView, ExperimentalAsh> button_row_ = nullptr;
  raw_ptr<MediaActionButton, ExperimentalAsh> play_pause_button_ = nullptr;
  raw_ptr<media_message_center::MediaControlsProgressView, ExperimentalAsh>
      progress_ = nullptr;

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

  base::WeakPtrFactory<LockScreenMediaControlsView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
