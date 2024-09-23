// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/full_restore_client_lacros.h"

#include "base/barrier_callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/full_restore/full_restore_util.h"
#include "chrome/browser/lacros/profile_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chromeos/lacros/lacros_service.h"

FullRestoreClientLacros::FullRestoreClientLacros() {
  auto* const lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::FullRestore>()) {
    lacros_service->GetRemote<crosapi::mojom::FullRestore>()
        ->AddFullRestoreClient(receiver_.BindNewPipeAndPassRemote());
  }
}

FullRestoreClientLacros::~FullRestoreClientLacros() = default;

void FullRestoreClientLacros::GetSessionInformation(
    GetSessionInformationCallback callback) {
  // TODO(sammiequon): Once multi profile is complete, consider factoring this
  // code with the block in `ash::full_restore::FullRestoreService`.
  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
  if (profiles.empty()) {
    std::move(callback).Run({});
    return;
  }

  // Retrieves session service data from browser and app browsers, which
  // will be used to display favicons and tab titles.
  auto barrier = base::BarrierCallback<SessionWindowsPair>(
      /*num_callbacks=*/2u * profiles.size(), /*done_callback=*/base::BindOnce(
          &FullRestoreClientLacros::OnGotAllSessions,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  for (Profile* profile : profiles) {
    SessionServiceBase* service =
        SessionServiceFactory::GetForProfileForSessionRestore(profile);
    SessionServiceBase* app_service =
        AppSessionServiceFactory::GetForProfileForSessionRestore(profile);
    if (!service || !app_service) {
      std::move(callback).Run({});
      return;
    }

    const uint64_t profile_id = HashProfilePathToProfileId(profile->GetPath());
    service->GetLastSession(
        base::BindOnce(&FullRestoreClientLacros::OnGotSession,
                       weak_ptr_factory_.GetWeakPtr(), barrier, profile_id));
    app_service->GetLastSession(
        base::BindOnce(&FullRestoreClientLacros::OnGotSession,
                       weak_ptr_factory_.GetWeakPtr(), barrier, profile_id));
  }
}

void FullRestoreClientLacros::OnGotSession(
    base::OnceCallback<void(SessionWindowsPair)> barrier,
    uint64_t profile_id,
    SessionWindows session_windows,
    SessionID active_window_id,
    bool read_error) {
  std::move(barrier).Run({std::move(session_windows), profile_id});
}

void FullRestoreClientLacros::OnGotAllSessions(
    GetSessionInformationCallback callback,
    const std::vector<SessionWindowsPair>& all_session_windows) {
  std::vector<crosapi::mojom::SessionWindowPtr> session_window_ptrs;
  for (const SessionWindowsPair& session_windows : all_session_windows) {
    const uint64_t lacros_profile_id = session_windows.second;
    for (const std::unique_ptr<sessions::SessionWindow>& session_window :
         session_windows.first) {
      session_window_ptrs.emplace_back(
          full_restore::ToSessionWindowPtr(*session_window, lacros_profile_id));
    }
  }
  std::move(callback).Run(std::move(session_window_ptrs));
}
