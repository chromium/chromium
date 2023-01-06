// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_controller.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/media/unified_media_controls_view.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr int kMinimumArtworkSize = 30;
constexpr int kDisiredArtworkSize = 48;

// Time to wait for new media session.
constexpr base::TimeDelta kFreezeControlsTime = base::Milliseconds(2000);

// Time to wait for new artwork.
constexpr base::TimeDelta kHideArtworkDelay = base::Milliseconds(2000);

}  // namespace

UnifiedMediaControlsController::UnifiedMediaControlsController(
    Delegate* delegate)
    : delegate_(delegate) {
  media_session::MediaSessionService* service =
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
  if (freeze_session_timer_->IsRunning()) {
    if (session_info)
      pending_session_info_ = std::move(session_info);
    return;
  }

  session_info_ = std::move(session_info);
  MaybeShowMediaControlsOrEmptyState();

  if (!session_info_)
    return;

  media_controls_->SetIsPlaying(
      session_info_->playback_state ==
      media_session::mojom::MediaPlaybackState::kPlaying);
  media_controls_->UpdateActionButtonAvailability(enabled_actions_);
}

void UnifiedMediaControlsController::MediaSessionMetadataChanged(
    const absl::optional<media_session::MediaMetadata>& metadata) {
  pending_metadata_ = metadata.value_or(media_session::MediaMetadata());
  if (freeze_session_timer_->IsRunning())
    return;

  session_metadata_ = *pending_metadata_;
  media_controls_->SetTitle(pending_metadata_->title.empty()
                                ? pending_metadata_->source_title
                                : pending_metadata_->title);
  media_controls_->SetArtist(pending_metadata_->artist);
  pending_metadata_.reset();
  MaybeShowMediaControlsOrEmptyState();
}

void UnifiedMediaControlsController::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  if (freeze_session_timer_->IsRunning()) {
    pending_enabled_actions_ =
        base::flat_set<media_session::mojom::MediaSessionAction>(
            actions.begin(), actions.end());
    return;
  }

  enabled_actions_ = base::flat_set<media_session::mojom::MediaSessionAction>(
      actions.begin(), actions.end());
  media_controls_->UpdateActionButtonAvailability(enabled_actions_);
}

void UnifiedMediaControlsController::MediaSessionChanged(
    const absl::optional<base::UnguessableToken>& request_id) {
  // If previous session resumes, stop freeze timer if necessary and discard
  // any pending data.
  if (request_id == media_session_id_) {
    if (!freeze_session_timer_->IsRunning())
      return;

    freeze_session_timer_->Stop();
    ResetPendingData();
    return;
  }

  // If we don't currently have a session, update session id.
  if (!media_session_id_.has_value()) {
    DCHECK(!freeze_session_timer_->IsRunning());
    media_session_id_ = request_id;
    return;
  }

  // If we do currently have a session and received a new session request,
  // wait to see if the session will resumes. If it is not resumed after
  // timeout, update session with all pending data.
  pending_session_id_ = request_id;
  if (!freeze_session_timer_->IsRunning()) {
    if (hide_artwork_timer_->IsRunning())
      hide_artwork_timer_->Stop();

    freeze_session_timer_->Start(
        FROM_HERE, kFreezeControlsTime,
        base::BindOnce(&UnifiedMediaControlsController::UpdateSession,
                       base::Unretained(this)));
  }
}

void UnifiedMediaControlsController::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  if (type != media_session::mojom::MediaSessionImageType::kArtwork)
    return;

  if (freeze_session_timer_->IsRunning()) {
    pending_artwork_ = bitmap;
    return;
  }

  UpdateArtwork(bitmap, true /* should_start_hide_timer */);
}

void UnifiedMediaControlsController::UpdateSession() {
  media_session_id_ = pending_session_id_;

  if (media_session_id_ == absl::nullopt)
    ResetPendingData();

  if (pending_session_info_.has_value()) {
    media_controls_->SetIsPlaying(
        (*pending_session_info_)->playback_state ==
        media_session::mojom::MediaPlaybackState::kPlaying);
    session_info_ = std::move(*pending_session_info_);
  } else {
    session_info_ = nullptr;
  }

  if (pending_metadata_.has_value()) {
    media_controls_->SetTitle(pending_metadata_->title.empty()
                                  ? pending_metadata_->source_title
                                  : pending_metadata_->title);
    media_controls_->SetArtist(pending_metadata_->artist);
  }
  session_metadata_ =
      pending_metadata_.value_or(media_session::MediaMetadata());

  if (pending_enabled_actions_.has_value()) {
    media_controls_->UpdateActionButtonAvailability(*pending_enabled_actions_);
    enabled_actions_ = base::flat_set<media_session::mojom::MediaSessionAction>(
        pending_enabled_actions_->begin(), pending_enabled_actions_->end());
  }

  if (pending_artwork_.has_value())
    UpdateArtwork(*pending_artwork_, false /* should_start_hide_timer */);

  MaybeShowMediaControlsOrEmptyState();
  ResetPendingData();
}

void UnifiedMediaControlsController::UpdateArtwork(
    const SkBitmap& bitmap,
    bool should_start_hide_timer) {
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

  if (media_controls_->artwork_view()->GetImageModel().IsEmpty())
    return;

  if (!should_start_hide_timer) {
    media_controls_->SetArtwork(absl::nullopt);
    return;
  }

  // Start |hide_artork_timer_| if not already started and wait for
  // artwork update.
  if (!hide_artwork_timer_->IsRunning()) {
    hide_artwork_timer_->Start(
        FROM_HERE, kHideArtworkDelay,
        base::BindOnce(&UnifiedMediaControlsView::SetArtwork,
                       base::Unretained(media_controls_), absl::nullopt));
  }
}

void UnifiedMediaControlsController::OnMediaControlsViewClicked() {
  delegate_->OnMediaControlsViewClicked();
}

void UnifiedMediaControlsController::PerformAction(
    media_session::mojom::MediaSessionAction action) {
  if (freeze_session_timer_->IsRunning())
    return;

  base::UmaHistogramEnumeration(
      "Media.CrosGlobalMediaControls.QuickSettingUserAction", action);
  media_session::PerformMediaSessionAction(action, media_controller_remote_);
}

void UnifiedMediaControlsController::ResetPendingData() {
  pending_session_id_.reset();
  pending_session_info_.reset();
  pending_metadata_.reset();
  pending_enabled_actions_.reset();
  pending_artwork_.reset();
}

bool UnifiedMediaControlsController::ShouldShowMediaControls() const {
  return session_info_ && session_info_->is_controllable;
}

void UnifiedMediaControlsController::MaybeShowMediaControlsOrEmptyState() {
  if (ShouldShowMediaControls()) {
    delegate_->ShowMediaControls();
    media_controls_->OnNewMediaSession();
  } else {
    media_controls_->ShowEmptyState();
  }
}

void UnifiedMediaControlsController::FlushForTesting() {
  media_controller_remote_.FlushForTesting();  // IN-TEST
}

}  // namespace ash
