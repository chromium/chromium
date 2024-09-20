// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "base/check.h"
#include "base/uuid.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace {

// Returns the active account ID. If the system has not been ready, an empty ID
// will be returned.
AccountId GetActiveAccountId() {
  auto* shell = ash::Shell::Get();
  CHECK(shell);
  const auto* session_controller = shell->session_controller();
  CHECK(session_controller);
  return session_controller ? session_controller->GetActiveAccountId()
                            : AccountId();
}

}  // namespace

namespace ash::youtube_music {

YouTubeMusicController::YouTubeMusicController() {
  SessionController* session_controller = SessionController::Get();
  CHECK(session_controller);
  session_controller->AddObserver(this);
}

YouTubeMusicController::~YouTubeMusicController() {
  SessionController* session_controller = SessionController::Get();
  CHECK(session_controller);
  session_controller->RemoveObserver(this);
}

void YouTubeMusicController::OnActiveUserSessionChanged(
    const AccountId& active_id) {
  // Do not create client for guest profile.
  auto* session_controller = Shell::Get()->session_controller();
  if (!session_controller || session_controller->IsUserGuest()) {
    return;
  }

  // Do not create client if it already exists.
  if (clients_.find(active_id) != clients_.end()) {
    return;
  }

  clients_[active_id] =
      FocusModeController::Get()->delegate()->CreateYouTubeMusicClient(
          active_id, GetDeviceId());
}

youtube_music::YouTubeMusicClient* YouTubeMusicController::GetActiveClient()
    const {
  const auto it = clients_.find(GetActiveAccountId());
  return it != clients_.end() ? it->second.get() : nullptr;
}

bool YouTubeMusicController::GetMusicSection(
    youtube_music::GetMusicSectionCallback callback) {
  CHECK(callback);
  auto* client = GetActiveClient();
  if (!client) {
    return false;
  }
  client->GetMusicSection(std::move(callback));
  return true;
}

bool YouTubeMusicController::GetPlaylist(
    const std::string& playlist_id,
    youtube_music::GetPlaylistCallback callback) {
  CHECK(callback);
  auto* client = GetActiveClient();
  if (!client) {
    return false;
  }
  client->GetPlaylist(playlist_id, std::move(callback));
  return true;
}

bool YouTubeMusicController::PlaybackQueuePrepare(
    const std::string& playlist_id,
    youtube_music::GetPlaybackContextCallback callback) {
  CHECK(callback);
  auto* client = GetActiveClient();
  if (!client) {
    return false;
  }
  client->PlaybackQueuePrepare(playlist_id, std::move(callback));
  return true;
}

bool YouTubeMusicController::PlaybackQueueNext(
    const std::string& playback_queue_id,
    youtube_music::GetPlaybackContextCallback callback) {
  CHECK(callback);
  auto* client = GetActiveClient();
  if (!client) {
    return false;
  }
  client->PlaybackQueueNext(playback_queue_id, std::move(callback));
  return true;
}

bool YouTubeMusicController::ReportPlayback(
    const std::string& playback_reporting_token,
    const PlaybackData& playback_data,
    ReportPlaybackCallback callback) {
  auto* client = GetActiveClient();
  if (!client) {
    return false;
  }
  client->ReportPlayback(playback_reporting_token, playback_data,
                         std::move(callback));
  return true;
}

std::string YouTubeMusicController::GetDeviceId() {
  // Device ids are unique to the device + user and stable across reboot. So,
  // they're generated per user and stored in prefs.
  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  const std::string& device_id =
      pref_service->GetString(prefs::kFocusModeDeviceId);
  if (!device_id.empty()) {
    return device_id;
  }

  // A new UUID needs to be generated.
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::string uuid_string = uuid.AsLowercaseString();
  pref_service->SetString(prefs::kFocusModeDeviceId, uuid_string);
  return uuid_string;
}

}  // namespace ash::youtube_music
