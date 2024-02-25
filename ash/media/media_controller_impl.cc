// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/media_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/accelerators/media_keys_util.h"

namespace ash {

namespace {

constexpr base::TimeDelta kDefaultSeekTime =
    base::Seconds(media_session::mojom::kDefaultSeekTimeSeconds);

bool IsMediaSessionActionEligibleForKeyControl(
    media_session::mojom::MediaSessionAction action) {
  return action == media_session::mojom::MediaSessionAction::kPlay ||
         action == media_session::mojom::MediaSessionAction::kPause ||
         action == media_session::mojom::MediaSessionAction::kPreviousTrack ||
         action == media_session::mojom::MediaSessionAction::kNextTrack;
}

}  // namespace

MediaControllerImpl::MediaControllerImpl() {
  // If media session media key handling is enabled this will setup a connection
  // and bind an observer to the media session service.
  if (base::FeatureList::IsEnabled(media::kHardwareMediaKeyHandling))
    GetMediaSessionController();
}

MediaControllerImpl::~MediaControllerImpl() = default;

// static
void MediaControllerImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kLockScreenMediaControlsEnabled, true);
}

bool MediaControllerImpl::AreLockScreenMediaKeysEnabled() const {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  CHECK(prefs);

  return prefs->GetBoolean(prefs::kLockScreenMediaControlsEnabled) &&
         !media_controls_dismissed_;
}

void MediaControllerImpl::SetMediaControlsDismissed(
    bool media_controls_dismissed) {
  media_controls_dismissed_ = media_controls_dismissed;
}

void MediaControllerImpl::AddObserver(MediaCaptureObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaControllerImpl::RemoveObserver(MediaCaptureObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaControllerImpl::SetClient(MediaClient* client) {
  client_ = client;

  // When |client_| is changed or encounters an error we should reset the
  // |force_media_client_key_handling_| bit.
  ResetForceMediaClientKeyHandling();
}

void MediaControllerImpl::SetForceMediaClientKeyHandling(bool enabled) {
  force_media_client_key_handling_ = enabled;
}

void MediaControllerImpl::NotifyCaptureState(
    const base::flat_map<AccountId, MediaCaptureState>& capture_states) {
  for (auto& observer : observers_)
    observer.OnMediaCaptureChanged(capture_states);
}

void MediaControllerImpl::NotifyVmMediaNotificationState(bool camera,
                                                         bool mic,
                                                         bool camera_and_mic) {
  for (auto& observer : observers_)
    observer.OnVmMediaNotificationChanged(camera, mic, camera_and_mic);
}

void MediaControllerImpl::HandleMediaPlayPause() {
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !AreLockScreenMediaKeysEnabled()) {
    return;
  }

  // If the |client_| is force handling the keys then we should forward them.
  if (client_ && force_media_client_key_handling_) {
    ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPlayPause);
    client_->HandleMediaPlayPause();
    return;
  }

  // If media session media key handling is enabled. Toggle play pause using the
  // media session service.
  if (ShouldUseMediaSession()) {
    switch (media_session_info_->playback_state) {
      case media_session::mojom::MediaPlaybackState::kPaused:
        GetMediaSessionController()->Resume();
        ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPlay);
        break;
      case media_session::mojom::MediaPlaybackState::kPlaying:
        GetMediaSessionController()->Suspend();
        ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPause);
        break;
    }

    return;
  }

  // If media session does not handle the key then we don't know whether the
  // action will play or pause so we should record a generic "play/pause".
  ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPlayPause);

  if (client_)
    client_->HandleMediaPlayPause();
}

void MediaControllerImpl::HandleMediaPlay() {
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !AreLockScreenMediaKeysEnabled()) {
    return;
  }

  ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPlay);

  // If the |client_| is force handling the keys then we should forward them.
  if (client_ && force_media_client_key_handling_) {
    client_->HandleMediaPlay();
    return;
  }

  // If media session media key handling is enabled. Fire play using the media
  // session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->Resume();
    return;
  }

  if (client_)
    client_->HandleMediaPlay();
}

void MediaControllerImpl::HandleMediaPause() {
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !AreLockScreenMediaKeysEnabled()) {
    return;
  }

  ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPause);

  // If the |client_| is force handling the keys then we should forward them.
  if (client_ && force_media_client_key_handling_) {
    client_->HandleMediaPause();
    return;
  }

  // If media session media key handling is enabled. Fire pause using the media
  // session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->Suspend();
    return;
  }

  if (client_)
    client_->HandleMediaPause();
}

void MediaControllerImpl::HandleMediaStop() {
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !AreLockScreenMediaKeysEnabled()) {
    return;
  }

  ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kStop);

  // If the |client_| is force handling the keys then we should forward them.
  if (client_ && force_media_client_key_handling_) {
    client_->HandleMediaStop();
    return;
  }

  // If media session media key handling is enabled. Fire stop using the media
  // session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->Stop();
    return;
  }

  if (client_)
    client_->HandleMediaStop();
}

void MediaControllerImpl::HandleMediaNextTrack() {
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !AreLockScreenMediaKeysEnabled()) {
    return;
  }

  ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kNextTrack);

  // If the |client_| is force handling the keys then we should forward them.
  if (client_ && force_media_client_key_handling_) {
    client_->HandleMediaNextTrack();
    return;
  }

  // If media session media key handling is enabled. Fire next track using the
  // media session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->NextTrack();
    return;
  }

  if (client_)
    client_->HandleMediaNextTrack();
}

void MediaControllerImpl::HandleMediaPrevTrack() {
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !AreLockScreenMediaKeysEnabled()) {
    return;
  }

  ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kPreviousTrack);

  // If the |client_| is force handling the keys then we should forward them.
  if (client_ && force_media_client_key_handling_) {
    client_->HandleMediaPrevTrack();
    return;
  }

  // If media session media key handling is enabled. Fire previous track using
  // the media session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->PreviousTrack();
    return;
  }

  if (client_)
    client_->HandleMediaPrevTrack();
}

void MediaControllerImpl::HandleMediaSeekBackward() {
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !AreLockScreenMediaKeysEnabled()) {
    return;
  }

  ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kSeekBackward);

  // If the |client_| is force handling the keys then we should forward them.
  if (client_ && force_media_client_key_handling_) {
    client_->HandleMediaSeekBackward();
    return;
  }

  // If media session media key handling is enabled. Seek backward with
  // kDefaultSeekTime using the media session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->Seek(kDefaultSeekTime * -1);
    return;
  }

  if (client_)
    client_->HandleMediaSeekBackward();
}

void MediaControllerImpl::HandleMediaSeekForward() {
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !AreLockScreenMediaKeysEnabled()) {
    return;
  }

  ui::RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction::kSeekForward);

  // If the |client_| is force handling the keys then we should forward them.
  if (client_ && force_media_client_key_handling_) {
    client_->HandleMediaSeekForward();
    return;
  }

  // If media session media key handling is enabled. Seek forward with
  // kDefaultSeekTime using the media session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->Seek(kDefaultSeekTime);
    return;
  }

  if (client_)
    client_->HandleMediaSeekForward();
}

void MediaControllerImpl::RequestCaptureState() {
  if (client_)
    client_->RequestCaptureState();
}

void MediaControllerImpl::SuspendMediaSessions() {
  if (client_)
    client_->SuspendMediaSessions();
}

void MediaControllerImpl::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  media_session_info_ = std::move(session_info);
}

void MediaControllerImpl::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  supported_media_session_action_ = false;

  for (auto action : actions) {
    if (IsMediaSessionActionEligibleForKeyControl(action)) {
      supported_media_session_action_ = true;
      return;
    }
  }
}

void MediaControllerImpl::SetMediaSessionControllerForTest(
    mojo::Remote<media_session::mojom::MediaController> controller) {
  media_session_controller_remote_ = std::move(controller);
  BindMediaControllerObserver();
}

void MediaControllerImpl::FlushForTesting() {
  if (media_session_controller_remote_)
    media_session_controller_remote_.FlushForTesting();
}

media_session::mojom::MediaController*
MediaControllerImpl::GetMediaSessionController() {
  // NOTE: |media_session_controller_remote_| may be overridden by tests and
  // therefore non-null even if the real Media Session Service is unavailable.
  if (media_session_controller_remote_)
    return media_session_controller_remote_.get();

  // The service may be unavailable in some test environments.
  if (!Shell::HasInstance())
    return nullptr;

  media_session::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();
  if (!service)
    return nullptr;

  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_session_controller_remote_.BindNewPipeAndPassReceiver());

  media_session_controller_remote_.set_disconnect_handler(
      base::BindOnce(&MediaControllerImpl::OnMediaSessionControllerError,
                     base::Unretained(this)));

  BindMediaControllerObserver();

  return media_session_controller_remote_.get();
}

void MediaControllerImpl::OnMediaSessionControllerError() {
  media_session_controller_remote_.reset();
  supported_media_session_action_ = false;
}

void MediaControllerImpl::BindMediaControllerObserver() {
  if (!media_session_controller_remote_.is_bound())
    return;

  media_session_controller_remote_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

bool MediaControllerImpl::ShouldUseMediaSession() {
  return base::FeatureList::IsEnabled(media::kHardwareMediaKeyHandling) &&
         GetMediaSessionController() && supported_media_session_action_ &&
         !media_session_info_.is_null();
}

void MediaControllerImpl::ResetForceMediaClientKeyHandling() {
  force_media_client_key_handling_ = false;
}

}  // namespace ash
