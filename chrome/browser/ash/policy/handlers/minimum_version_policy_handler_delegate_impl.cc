// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler_delegate_impl.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/update_required_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace policy {

MinimumVersionPolicyHandlerDelegateImpl::
    MinimumVersionPolicyHandlerDelegateImpl() = default;

bool MinimumVersionPolicyHandlerDelegateImpl::IsKioskMode() const {
  return user_manager::UserManager::IsInitialized() &&
         (ash::ShouldAutoLaunchKioskApp(
              CHECK_DEREF(base::CommandLine::ForCurrentProcess()),
              CHECK_DEREF(g_browser_process->local_state())) ||
          user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());
}

bool MinimumVersionPolicyHandlerDelegateImpl::IsDeviceEnterpriseManaged()
    const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->IsDeviceEnterpriseManaged();
}

bool MinimumVersionPolicyHandlerDelegateImpl::IsUserLoggedIn() const {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}

bool MinimumVersionPolicyHandlerDelegateImpl::IsUserEnterpriseManaged() const {
  if (!IsUserLoggedIn()) {
    return false;
  }
  Profile* const profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    return false;
  }
  // TODO(https://crbug.com/1048607): Handle the case when |IsUserLoggedIn|
  // returns true after Auth success but |IsManaged| returns false before user
  // policy fetched.
  return profile->GetProfilePolicyConnector()->IsManaged() &&
         !profile->IsChild();
}

bool MinimumVersionPolicyHandlerDelegateImpl::IsLoginSessionState() const {
  using session_manager::SessionState;
  SessionState state = session_manager::SessionManager::Get()->session_state();
  return state == SessionState::LOGIN_PRIMARY;
}

bool MinimumVersionPolicyHandlerDelegateImpl::IsLoginInProgress() const {
  const auto* existing_user_controller =
      ash::ExistingUserController::current_controller();
  return existing_user_controller &&
         existing_user_controller->IsSigninInProgress();
}

void MinimumVersionPolicyHandlerDelegateImpl::ShowUpdateRequiredScreen() {
  if (ash::LoginDisplayHost::default_host()) {
    ash::LoginDisplayHost::default_host()->StartWizard(
        ash::UpdateRequiredView::kScreenId);
  }
}

void MinimumVersionPolicyHandlerDelegateImpl::RestartToLoginScreen() {
  chrome::AttemptUserExit();
}

void MinimumVersionPolicyHandlerDelegateImpl::
    HideUpdateRequiredScreenIfShown() {
  auto* const wizard_controller = ash::WizardController::default_controller();
  if (!wizard_controller) {
    return;
  }
  auto* screen = wizard_controller->GetScreen<ash::UpdateRequiredScreen>();
  if (screen->is_hidden()) {
    return;
  }
  screen->Exit();
}

base::Version MinimumVersionPolicyHandlerDelegateImpl::GetCurrentVersion()
    const {
  return base::Version(base::SysInfo::OperatingSystemVersion());
}

}  // namespace policy
