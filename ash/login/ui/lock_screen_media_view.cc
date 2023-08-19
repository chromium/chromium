// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_view.h"

#include "ash/media/media_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/media/media_color_theme.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "components/global_media_controls/public/constants.h"
#include "components/vector_icons/vector_icons.h"
#include "services/media_session/public/cpp/util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

// Constants for the dismiss button.
constexpr gfx::Size kDismissButtonSize = gfx::Size(20, 20);
constexpr int kDismissButtonIconSize = 15;

// The time to delay before considering a new media session has started to
// replace the current one.
constexpr base::TimeDelta kSwitchMediaDelay = base::Seconds(2);

class DismissButton : public views::ImageButton {
 public:
  DismissButton(PressedCallback callback,
                ui::ColorId foreground_color_id,
                ui::ColorId foreground_disabled_color_id,
                ui::ColorId focus_ring_color_id)
      : ImageButton(std::move(callback)) {
    views::ConfigureVectorImageButton(this);
    SetFlipCanvasOnPaintForRTLUI(false);
    views::InstallRoundRectHighlightPathGenerator(
        this, gfx::Insets(), kDismissButtonSize.height() / 2);
    SetPreferredSize(kDismissButtonSize);

    SetInstallFocusRingOnFocus(true);
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    views::FocusRing::Get(this)->SetColorId(focus_ring_color_id);

    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_CLOSE));
    views::SetImageFromVectorIconWithColorId(
        this, vector_icons::kCloseRoundedIcon, foreground_color_id,
        foreground_disabled_color_id, kDismissButtonIconSize);
  }
};

}  // namespace

LockScreenMediaView::LockScreenMediaView(
    MediaControlsEnabledCallback media_controls_enabled_callback,
    const base::RepeatingClosure& show_media_view_callback,
    const base::RepeatingClosure& hide_media_view_callback)
    : media_controls_enabled_callback_(media_controls_enabled_callback),
      show_media_view_callback_(show_media_view_callback),
      hide_media_view_callback_(hide_media_view_callback) {
  // Observe power events and if created in power suspended state, post
  // OnSuspend() call to run after LockContentsView is initialized.
  if (base::PowerMonitor::AddPowerSuspendObserverAndReturnSuspendedState(
          this)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LockScreenMediaView::OnSuspend,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  // Media controls have not been dismissed initially.
  Shell::Get()->media_controller()->SetMediaControlsDismissed(false);

  // |service| can be null in tests.
  media_session::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();
  if (!service) {
    return;
  }

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes.
  media_controller_remote_->AddObserver(
      media_observer_receiver_.BindNewPipeAndPassRemote());

  // Observe the active media controller for image changes.
  media_controller_remote_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork,
      global_media_controls::kMediaItemArtworkMinSize,
      global_media_controls::kMediaItemArtworkDesiredSize,
      artwork_observer_receiver_.BindNewPipeAndPassRemote());

  SetLayoutManager(std::make_unique<views::FillLayout>());
  const auto media_color_theme = GetCrosMediaColorTheme();

  auto dismiss_button = std::make_unique<DismissButton>(
      base::BindRepeating(&LockScreenMediaView::Hide, base::Unretained(this)),
      media_color_theme.primary_foreground_color_id,
      media_color_theme.secondary_foreground_color_id,
      media_color_theme.focus_ring_color_id);

  view_ = AddChildView(
      std::make_unique<global_media_controls::MediaNotificationViewAshImpl>(
          this, /*item=*/nullptr, /*footer_view=*/nullptr,
          /*device_selector_view=*/nullptr, std::move(dismiss_button),
          media_color_theme,
          global_media_controls::MediaDisplayPage::kLockScreenMediaView));
}

LockScreenMediaView::~LockScreenMediaView() {
  base::PowerMonitor::RemovePowerSuspendObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

gfx::Size LockScreenMediaView::CalculatePreferredSize() const {
  return global_media_controls::kCrOSMediaItemUpdatedUISize;
}

void LockScreenMediaView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kListItem;
  node_data->SetNameChecked(l10n_util::GetStringUTF8(
      IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACCESSIBLE_NAME));
}

///////////////////////////////////////////////////////////////////////////////
// media_session::mojom::MediaControllerObserver implementations:

void LockScreenMediaView::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (switch_media_delay_timer_->IsRunning()) {
    return;
  }

  // If the session is marked as sensitive, or it is not controllable, or it
  // already has a presentation of another cast media session, do not show the
  // media view.
  if (!media_controls_enabled_callback_.Run() || !session_info ||
      session_info->is_sensitive || !session_info->is_controllable ||
      session_info->has_presentation) {
    Hide();
    return;
  }

  // If the media is paused when the screen is locked, do not show the media
  // view.
  if (!IsDrawn() && session_info->playback_state ==
                        media_session::mojom::MediaPlaybackState::kPaused) {
    Hide();
    return;
  }

  if (!IsDrawn()) {
    Show();
  }

  view_->UpdateWithMediaSessionInfo(std::move(session_info));
}

void LockScreenMediaView::MediaSessionMetadataChanged(
    const absl::optional<media_session::MediaMetadata>& metadata) {
  if (switch_media_delay_timer_->IsRunning()) {
    return;
  }

  view_->UpdateWithMediaMetadata(
      metadata.value_or(media_session::MediaMetadata()));
}

void LockScreenMediaView::MediaSessionActionsChanged(
    const std::vector<MediaSessionAction>& actions) {
  if (switch_media_delay_timer_->IsRunning()) {
    return;
  }

  base::flat_set<MediaSessionAction> actions_without_pip(actions);
  actions_without_pip.erase(MediaSessionAction::kEnterPictureInPicture);
  actions_without_pip.erase(MediaSessionAction::kExitPictureInPicture);
  view_->UpdateWithMediaActions(actions_without_pip);
}

void LockScreenMediaView::MediaSessionChanged(
    const absl::optional<base::UnguessableToken>& request_id) {
  if (!media_session_id_.has_value()) {
    media_session_id_ = request_id;
    return;
  }

  // If |media_session_id_| resumed while waiting, stop the timer.
  if (switch_media_delay_timer_->IsRunning() &&
      request_id == media_session_id_) {
    switch_media_delay_timer_->Stop();
  }

  // If this session is different than the previous one, wait to see if the
  // previous one resumes before hiding the media view.
  if (request_id == media_session_id_) {
    return;
  }

  switch_media_delay_timer_->Start(
      FROM_HERE, kSwitchMediaDelay,
      base::BindOnce(&LockScreenMediaView::Hide, base::Unretained(this)));
}

void LockScreenMediaView::MediaSessionPositionChanged(
    const absl::optional<media_session::MediaPosition>& position) {
  if (switch_media_delay_timer_->IsRunning() || !position.has_value()) {
    return;
  }

  view_->UpdateWithMediaPosition(*position);
}

///////////////////////////////////////////////////////////////////////////////
// media_session::mojom::MediaControllerImageObserver implementations:

void LockScreenMediaView::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  if (switch_media_delay_timer_->IsRunning()) {
    return;
  }

  CHECK_EQ(media_session::mojom::MediaSessionImageType::kArtwork, type);
  view_->UpdateWithMediaArtwork(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

///////////////////////////////////////////////////////////////////////////////
// media_message_center::MediaNotificationContainer implementations:

void LockScreenMediaView::OnMediaSessionActionButtonPressed(
    MediaSessionAction action) {
  if (media_session_id_.has_value()) {
    media_session::PerformMediaSessionAction(action, media_controller_remote_);
  }
}

void LockScreenMediaView::SeekTo(base::TimeDelta time) {
  if (media_session_id_.has_value()) {
    media_controller_remote_->SeekTo(time);
  }
}

///////////////////////////////////////////////////////////////////////////////
// base::PowerSuspendObserver implementations:

void LockScreenMediaView::OnSuspend() {
  Hide();
}

///////////////////////////////////////////////////////////////////////////////
// LockScreenMediaView implementations:

void LockScreenMediaView::Show() {
  show_media_view_callback_.Run();
}

void LockScreenMediaView::Hide() {
  media_controller_remote_->Stop();
  hide_media_view_callback_.Run();
}

}  // namespace ash
