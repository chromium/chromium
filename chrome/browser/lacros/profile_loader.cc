// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/profile_loader.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/startup/first_run_service.h"

namespace {

void MaybeProceedWithProfile(base::OnceCallback<void(Profile*)> callback,
                             Profile* profile,
                             bool proceed) {
  LOG_IF(ERROR, !proceed) << "Not proceeding after LacrosFirstRun";
  std::move(callback).Run(proceed ? profile : nullptr);
}

// Helper function to handle profile initialization.
void OnMainProfileInitialized(base::OnceCallback<void(Profile*)> callback,
                              bool can_trigger_fre,
                              Profile* profile) {
  DCHECK(callback);
  if (!profile) {
    LOG(ERROR) << "Profile creation failed.";
    // Profile creation failed, show the profile picker instead.
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kNewSessionOnExistingProcess));
    std::move(callback).Run(nullptr);
    return;
  }

  auto* fre_service = FirstRunServiceFactory::GetForBrowserContext(profile);
  if (fre_service && can_trigger_fre && fre_service->ShouldOpenFirstRun()) {
    // TODO(https://crbug.com/1313848): Consider taking a
    // `ScopedProfileKeepAlive`.
    fre_service->OpenFirstRunIfNeeded(
        FirstRunService::EntryPoint::kOther,
        base::BindOnce(&MaybeProceedWithProfile, std::move(callback),
                       base::Unretained(profile)));
  } else {
    std::move(callback).Run(profile);
  }
}

}  // namespace

void LoadMainProfile(base::OnceCallback<void(Profile*)> callback,
                     bool can_trigger_fre) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetPrimaryUserProfilePath(),
      base::BindOnce(&OnMainProfileInitialized, std::move(callback),
                     can_trigger_fre));
}
