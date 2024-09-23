// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/lock_to_single_user_manager.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

using RebootOnSignOutPolicy =
    enterprise_management::DeviceRebootOnUserSignoutProto;
using RebootOnSignOutRequest =
    user_data_auth::LockToSingleUserMountUntilRebootRequest;
using RebootOnSignOutReply =
    user_data_auth::LockToSingleUserMountUntilRebootReply;

namespace policy {

namespace {

ash::ConciergeClient* GetConciergeClient() {
  return ash::ConciergeClient::Get();
}

LockToSingleUserManager* g_lock_to_single_user_manager_instance;
}  // namespace

// static
LockToSingleUserManager*
LockToSingleUserManager::GetLockToSingleUserManagerInstance() {
  return g_lock_to_single_user_manager_instance;
}

LockToSingleUserManager::LockToSingleUserManager() {
  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  DCHECK(!g_lock_to_single_user_manager_instance);
  g_lock_to_single_user_manager_instance = this;
}

LockToSingleUserManager::~LockToSingleUserManager() {
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);

  g_lock_to_single_user_manager_instance = nullptr;
}

void LockToSingleUserManager::DbusNotifyVmStarting() {
  if (lock_to_single_user_on_dbus_call_)
    LockToSingleUser();
}

void LockToSingleUserManager::ActiveUserChanged(user_manager::User* user) {
  user->IsAffiliatedAsync(
      base::BindOnce(&LockToSingleUserManager::OnUserAffiliationEstablished,
                     weak_factory_.GetWeakPtr(), user));
}

void LockToSingleUserManager::OnUserAffiliationEstablished(
    user_manager::User* user,
    bool is_affiliated) {
  if (is_affiliated)
    return;

  int policy_value = -1;
  if (!ash::CrosSettings::Get()->GetInteger(ash::kDeviceRebootOnUserSignout,
                                            &policy_value)) {
    return;
  }

  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  switch (policy_value) {
    default:
    case RebootOnSignOutPolicy::ALWAYS:
      user->AddProfileCreatedObserver(
          base::BindOnce(&LockToSingleUserManager::LockToSingleUser,
                         weak_factory_.GetWeakPtr()));
      break;
    case RebootOnSignOutPolicy::VM_STARTED_OR_ARC_SESSION:
      user->AddProfileCreatedObserver(
          base::BindOnce(&LockToSingleUserManager::AddVmStartingObservers,
                         weak_factory_.GetWeakPtr(), user));
      arc_session_observation_.Observe(arc::ArcSessionManager::Get());
      break;
    case RebootOnSignOutPolicy::ARC_SESSION:
      arc_session_observation_.Observe(arc::ArcSessionManager::Get());
      break;
    case RebootOnSignOutPolicy::NEVER:
      break;
  }
}

void LockToSingleUserManager::OnArcStarted() {
  arc_session_observation_.Reset();
  LockToSingleUser();
}

void LockToSingleUserManager::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  // When we receive the VmStarted signal we expect to already be locked to a
  // single user.

  GetConciergeClient()->RemoveVmObserver(this);
  LockToSingleUser();
  expect_to_be_locked_ = true;
}

void LockToSingleUserManager::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {}

void LockToSingleUserManager::OnVmStarting() {
  LockToSingleUser();
}

void LockToSingleUserManager::AddVmStartingObservers(user_manager::User* user) {
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);

  crostini::CrostiniManager::GetForProfile(profile)->AddVmStartingObserver(
      this);
  plugin_vm::PluginVmManagerFactory::GetForProfile(profile)
      ->AddVmStartingObserver(this);

  GetConciergeClient()->AddVmObserver(this);

  lock_to_single_user_on_dbus_call_ = true;
}

void LockToSingleUserManager::LockToSingleUser() {
  cryptohome::AccountIdentifier account_id =
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  RebootOnSignOutRequest request;
  request.mutable_account_id()->CopyFrom(account_id);
  ash::CryptohomeMiscClient::Get()->LockToSingleUserMountUntilReboot(
      request,
      base::BindOnce(
          &LockToSingleUserManager::OnLockToSingleUserMountUntilRebootDone,
          weak_factory_.GetWeakPtr()));
}

void LockToSingleUserManager::OnLockToSingleUserMountUntilRebootDone(
    std::optional<RebootOnSignOutReply> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Signing out user: no reply from "
                  "LockToSingleUserMountUntilReboot D-Bus call.";
    chrome::AttemptUserExit();
    return;
  }

  // Force user logout if failed to lock the device to single user mount.
  if (reply->error() ==
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET ||
      reply->error() == user_data_auth::CRYPTOHOME_ERROR_PCR_ALREADY_EXTENDED) {
    // The device is locked to single user on TPM level. Update the cache in
    // SessionTerminationManager, so that it triggers reboot on sign out.
    ash::SessionTerminationManager::Get()->SetDeviceLockedToSingleUser();
  } else {
    LOG(ERROR) << "Signing out user: failed to lock device to single user: "
               << reply->error();
    chrome::AttemptUserExit();
  }
}

}  // namespace policy
