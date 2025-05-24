// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_VIEW_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/timer/timer.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/media_message_center/media_notification_container.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

namespace {
class DismissButton;
}

// View for media controls that appear on the lock screen if it is enabled. This
// replaces the old LockScreenMediaControlsView if the flag
// media::kGlobalMediaControlsCrOSUpdatedUI is enabled. It registers for media
// updates and reuses MediaItemUIDetailedView to display the media controls.
class ASH_EXPORT LockScreenMediaView
    : public views::View,
      public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver,
      public media_message_center::MediaNotificationContainer,
      public base::PowerSuspendObserver {
  METADATA_HEADER(LockScreenMediaView, views::View)

 public:
  using MediaControlsEnabledCallback = const base::RepeatingCallback<bool()>&;

  explicit LockScreenMediaView(
      MediaControlsEnabledCallback media_controls_enabled_callback,
      const base::RepeatingClosure& show_media_view_callback,
      const base::RepeatingClosure& hide_media_view_callback);
  LockScreenMediaView(const LockScreenMediaView&) = delete;
  LockScreenMediaView& operator=(const LockScreenMediaView&) = delete;
  ~LockScreenMediaView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

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
  void MediaControllerChapterImageChanged(int chapter_index,
                                          const SkBitmap& bitmap) override;

  // media_message_center::MediaNotificationContainer:
  void OnExpanded(bool expanded) override {}
  void OnMediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override {}
  void OnMediaSessionMetadataChanged(
      const media_session::MediaMetadata& metadata) override {}
  void OnVisibleActionsChanged(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void OnMediaArtworkChanged(const gfx::ImageSkia& image) override {}
  void OnColorsChanged(SkColor foreground,
                       SkColor foreground_disabled,
                       SkColor background) override {}
  void OnHeaderClicked(bool activate_original_media) override {}
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) override;
  void SeekTo(base::TimeDelta time) override;

  // base::PowerSuspendObserver:
  void OnSuspend() override;

  // Helper functions for testing:
  void FlushForTesting();

  void SetMediaControllerForTesting(
      mojo::Remote<media_session::mojom::MediaController> media_controller);

  void SetSwitchMediaDelayTimerForTesting(
      std::unique_ptr<base::OneShotTimer> test_timer);

  views::Button* GetDismissButtonForTesting();

  global_media_controls::MediaItemUIDetailedView* GetDetailedViewForTesting();

 private:
  friend class LockScreenMediaViewTest;

  // Called to show the media view.
  void Show();

  // Called to hide the media view.
  void Hide();

  // Used to control the active media session.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_observer_receiver_{this};

  // Used to receive updates to the active media's artwork.
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      artwork_observer_receiver_{this};

  // The id of the current media session. It will be null if there is no current
  // session.
  std::optional<base::UnguessableToken> media_session_id_;

  // A timer that delays for some time before considering a new media session
  // has started to replace the current one. If a switch has occurred, the media
  // view will be hidden.
  std::unique_ptr<base::OneShotTimer> switch_media_delay_timer_ =
      std::make_unique<base::OneShotTimer>();

  const base::RepeatingCallback<bool()> media_controls_enabled_callback_;
  const base::RepeatingClosure show_media_view_callback_;
  const base::RepeatingClosure hide_media_view_callback_;

  raw_ptr<DismissButton> dismiss_button_;
  raw_ptr<global_media_controls::MediaItemUIDetailedView> view_;

  base::WeakPtrFactory<LockScreenMediaView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_VIEW_H_
