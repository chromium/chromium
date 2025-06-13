// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_login/enterprise_login_api.h"

#include <optional>
#include <string>

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/common/extensions/api/enterprise_login.h"
#include "components/user_manager/user_manager.h"

namespace extensions {

EnterpriseLoginExitCurrentManagedGuestSessionFunction::
    EnterpriseLoginExitCurrentManagedGuestSessionFunction() = default;
EnterpriseLoginExitCurrentManagedGuestSessionFunction::
    ~EnterpriseLoginExitCurrentManagedGuestSessionFunction() = default;

void EnterpriseLoginExitCurrentManagedGuestSessionFunction::OnResult(
    const std::optional<std::string>& error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  return Respond(NoArguments());
}

ExtensionFunction::ResponseAction
EnterpriseLoginExitCurrentManagedGuestSessionFunction::Run() {
  if (!user_manager::UserManager::Get() ||
      !user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession()) {
    return RespondNow(Error("Not a managed guest session."));
  }
  auto callback = base::BindOnce(
      &EnterpriseLoginExitCurrentManagedGuestSessionFunction::OnResult, this);

  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->login_ash()
      ->ExitCurrentSession(/*data_for_next_login_attempt=*/std::nullopt,
                           std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

}  // namespace extensions
