// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_login/enterprise_login_api.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/extensions/api/enterprise_login.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace extensions {

EnterpriseLoginExitCurrentManagedGuestSessionFunction::
    EnterpriseLoginExitCurrentManagedGuestSessionFunction() = default;
EnterpriseLoginExitCurrentManagedGuestSessionFunction::
    ~EnterpriseLoginExitCurrentManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseLoginExitCurrentManagedGuestSessionFunction::Run() {
  if (!user_manager::UserManager::Get() ||
      !user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession()) {
    return RespondNow(Error("Not a managed guest session."));
  }

  g_browser_process->local_state()->ClearPref(
      prefs::kLoginExtensionApiDataForNextLoginAttempt);

  chrome::AttemptUserExit();
  return RespondNow(NoArguments());
}

}  // namespace extensions
