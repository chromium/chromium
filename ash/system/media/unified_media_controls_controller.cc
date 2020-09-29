// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_controller.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/media/unified_media_controls_view.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/media_session/public/mojom/media_session_service.mojom.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr int kMinimumArtworkSize = 30;
constexpr int kDisiredArtworkSize = 48;

// Time to wait for new media session.
constexpr base::TimeDelta kHideControlsDelay =
    base::TimeDelta::FromMilliseconds(2000);

// Time to wait for new artwork.
constexpr base::TimeDelta kHideArtworkDelay =
    base::TimeDelta::FromMilliseconds(2000);

}  // namespace

UnifiedMediaControlsController::UnifiedMediaControlsController(
    Delegate* delegate)
    : delegate_(delegate) {
  media_session::mojom::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();
  // Happens in test.
  if (!service)
    return;

  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  media_controller_remote_->AddObserver(
      observer_receiver_.BindNewPipeAndPassRemote());

  media_controller_remote_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork,
      kMinimumArtworkSize, kDisiredArtworkSize,
      artwork_observer_receiver_.BindNewPipeAndPassRemote());
}

UnifiedMediaControlsController::~UnifiedMediaControlsController() = default;

views::View* UnifiedMediaControlsController::CreateView() {
  media_controls_ = new UnifiedMediaControlsView(this);
  return media_controls_;
}

void UnifiedMediaControlsController::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (hide_controls_timer_->IsRunning())
    return;

  if (!session_info)
    return;

  media_controls_->SetIsPlaying(
      session_info->playback_state ==
      media_session::mojom::MediaPlaybackState::kPlaying);
  media_controls_->UpdateActionButtonAvailability(enabled_actions_);
}

void UnifiedMediaControlsController::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  if (hide_controls_timer_->IsRunning())
    return;

  media_session::MediaMetadata session_metadata =
      metadata.value_or(media_session::MediaMetadata());
  media_controls_->SetTitle(session_metadata.title);
  media_controls_->SetArtist(session_metadata.artist);
}

void UnifiedMediaControlsController::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  if (hide_controls_timer_->IsRunning())
    return;

  enabled_actions_ = base::flat_set<media_session::mojom::MediaSessionAction>(
      actions.begin(), actions.end());
  media_controls_->UpdateActionButtonAvailability(enabled_actions_);
}

void UnifiedMediaControlsController::MediaSessionChanged(
    const base::Optional<base::UnguessableToken>& request_id) {
  // Stop the timer if we receive a new active sessoin.
  if (hide_controls_timer_->IsRunning() && request_id.has_value())
    hide_controls_timer_->Stop();

  if (request_id == media_session_id_)
    return;

  // Start hide controls timer if there is no active session, wait to
  // see if we will receive a new session.
  if (!request_id.has_value()) {
    hide_controls_timer_->Start(
        FROM_HERE, kHideControlsDelay,
        base::BindOnce(&UnifiedMediaControlsController::HideControls,
                       base::Unretained(this)));
    return;
  }

  if (!media_session_id_.has_value())
    delegate_->ShowMediaControls();
  media_session_id_ = request_id;
}

void UnifiedMediaControlsController::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  if (hide_controls_timer_->IsRunning())
    return;

  if (type != media_session::mojom::MediaSessionImageType::kArtwork)
    return;

  // Convert the bitmap to kN32_SkColorType if necessary.
  SkBitmap converted_bitmap;
  if (bitmap.colorType() == kN32_SkColorType) {
    converted_bitmap = bitmap;
  } else {
    SkImageInfo info = bitmap.info().makeColorType(kN32_SkColorType);
    if (converted_bitmap.tryAllocPixels(info)) {
      bitmap.readPixels(info, converted_bitmap.getPixels(),
                        converted_bitmap.rowBytes(), 0, 0);
    }
  }

  // If we do get an artwork, set the artwork immediately and stop
  // |hide_artwork_timer_| if necessary.
  if (!converted_bitmap.empty()) {
    if (hide_artwork_timer_->IsRunning())
      hide_artwork_timer_->Stop();

    media_controls_->SetArtwork(
        gfx::ImageSkia::CreateFrom1xBitmap(converted_bitmap));
    return;
  }

  if (media_controls_->artwork_view()->GetImage().isNull())
    return;

  // Start |hide_artork_timer_} if not already started and wait for
  // artwork update.
  if (!hide_artwork_timer_->IsRunning()) {
    hide_artwork_timer_->Start(
        FROM_HERE, kHideArtworkDelay,
        base::BindOnce(&UnifiedMediaControlsView::SetArtwork,
                       base::Unretained(media_controls_), base::nullopt));
  }
}

void UnifiedMediaControlsController::OnMediaControlsViewClicked() {
  delegate_->OnMediaControlsViewClicked();
}

void UnifiedMediaControlsController::PerformAction(
    media_session::mojom::MediaSessionAction action) {
  media_session::PerformMediaSessionAction(action, media_controller_remote_);
}

void UnifiedMediaControlsController::HideControls() {
  media_session_id_ = base::nullopt;
  delegate_->HideMediaControls();
}

void UnifiedMediaControlsController::FlushForTesting() {
  media_controller_remote_.FlushForTesting();  // IN-TEST
}

}  // namespace ash
