// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_view.h"

#include "ash/media/media_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/media/media_color_theme.h"
#include "base/metrics/histogram_functions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "components/global_media_controls/public/constants.h"
#include "components/vector_icons/vector_icons.h"
#include "services/media_session/public/cpp/util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
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

// Constants for histograms.
constexpr char kMediaDisplayPageHistogram[] = "Media.Notification.DisplayPage";
constexpr char kMediaUserActionHistogram[] = "Media.Notification.UserAction";

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
  if (base::PowerMonitor::GetInstance()
          ->AddPowerSuspendObserverAndReturnSuspendedState(this)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LockScreenMediaView::OnSuspend,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  // Media controls have not been dismissed initially.
  Shell::Get()->media_controller()->SetMediaControlsDismissed(false);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  const auto media_color_theme = GetCrosMediaColorTheme();

  auto dismiss_button = std::make_unique<DismissButton>(
      base::BindRepeating(&LockScreenMediaView::Hide, base::Unretained(this)),
      media_color_theme.primary_foreground_color_id,
      media_color_theme.secondary_foreground_color_id,
      media_color_theme.focus_ring_color_id);
  dismiss_button_ = dismiss_button.get();

  // Create the media view to receive media info updates, but the view may not
  // be visible to users yet and its visibility is set in LockContentsView.
  view_ = AddChildView(
      std::make_unique<global_media_controls::MediaItemUIDetailedView>(
          this, /*item=*/nullptr, /*footer_view=*/nullptr,
          /*device_selector_view=*/nullptr, std::move(dismiss_button),
          media_color_theme,
          global_media_controls::MediaDisplayPage::kLockScreenMediaView));

  GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF8(
      IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACCESSIBLE_NAME));

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
}

LockScreenMediaView::~LockScreenMediaView() {
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

gfx::Size LockScreenMediaView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return global_media_controls::kCrOSMediaItemUpdatedUISize;
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
    const std::optional<media_session::MediaMetadata>& metadata) {
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
    const std::optional<base::UnguessableToken>& request_id) {
  // Record to metric when the media view is visible to users and a non-empty
  // media session starts. This usually means the screen is locked and a playing
  // media is switching to the next media in a playlist. We need to check the
  // media view is visible to record the metric because MediaSessionChanged()
  // can also be called if there is a paused media.
  if (IsDrawn() && request_id.has_value()) {
    base::UmaHistogramEnumeration(
        kMediaDisplayPageHistogram,
        global_media_controls::MediaDisplayPage::kLockScreenMediaView);
  }

  // Record the active media session ID and future IDs will either be the same
  // as this one or be null.
  if (!media_session_id_.has_value()) {
    media_session_id_ = request_id;
    return;
  }

  // If |media_session_id_| resumed while waiting, stop the timer so that we do
  // not hide the media view.
  if (switch_media_delay_timer_->IsRunning() &&
      request_id == media_session_id_) {
    switch_media_delay_timer_->Stop();
  }

  // If this session is different than the previous one (which means it becomes
  // null), wait to see if the previous one resumes before hiding the media
  // view.
  if (request_id != media_session_id_) {
    switch_media_delay_timer_->Start(
        FROM_HERE, kSwitchMediaDelay,
        base::BindOnce(&LockScreenMediaView::Hide, base::Unretained(this)));
  }
}

void LockScreenMediaView::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
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

void LockScreenMediaView::MediaControllerChapterImageChanged(
    int chapter_index,
    const SkBitmap& bitmap) {
  if (switch_media_delay_timer_->IsRunning()) {
    return;
  }

  view_->UpdateWithChapterArtwork(chapter_index,
                                  gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

///////////////////////////////////////////////////////////////////////////////
// media_message_center::MediaNotificationContainer implementations:

void LockScreenMediaView::OnMediaSessionActionButtonPressed(
    MediaSessionAction action) {
  if (media_session_id_.has_value()) {
    base::UmaHistogramEnumeration(kMediaUserActionHistogram, action);
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
// Helper functions for testing:

void LockScreenMediaView::FlushForTesting() {
  media_controller_remote_.FlushForTesting();  // IN-TEST
}

void LockScreenMediaView::SetMediaControllerForTesting(
    mojo::Remote<media_session::mojom::MediaController> media_controller) {
  media_controller_remote_ = std::move(media_controller);
}

void LockScreenMediaView::SetSwitchMediaDelayTimerForTesting(
    std::unique_ptr<base::OneShotTimer> test_timer) {
  switch_media_delay_timer_ = std::move(test_timer);
}

views::Button* LockScreenMediaView::GetDismissButtonForTesting() {
  return dismiss_button_;
}

global_media_controls::MediaItemUIDetailedView*
LockScreenMediaView::GetDetailedViewForTesting() {
  return view_;
}

///////////////////////////////////////////////////////////////////////////////
// LockScreenMediaView implementations:

void LockScreenMediaView::Show() {
  // Show() is called to make the media view become visible at most once every
  // time the user locks the screen. There must be a playing media if Show() is
  // called, but the first MediaSessionChanged() call for that media happens
  // before this and MediaSessionChanged() skips recording to metric because the
  // media view is not visible. Therefore we need to record to metric here.
  base::UmaHistogramEnumeration(
      kMediaDisplayPageHistogram,
      global_media_controls::MediaDisplayPage::kLockScreenMediaView);
  show_media_view_callback_.Run();
}

void LockScreenMediaView::Hide() {
  // |media_controller_remote_| can be null in tests.
  if (media_controller_remote_.is_bound()) {
    media_controller_remote_->Stop();
  }
  hide_media_view_callback_.Run();
}

BEGIN_METADATA(LockScreenMediaView)
END_METADATA

}  // namespace ash
