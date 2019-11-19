// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/lock_to_single_user_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

using RebootOnSignOutPolicy =
    enterprise_management::DeviceRebootOnUserSignoutProto;
using RebootOnSignOutRequest =
    cryptohome::LockToSingleUserMountUntilRebootRequest;
using RebootOnSignOutReply = cryptohome::LockToSingleUserMountUntilRebootReply;
using RebootOnSignOutResult =
    cryptohome::LockToSingleUserMountUntilRebootResult;

namespace policy {

namespace {

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

// The result of locking the device to single user.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "LockToSingleUserResult" in src/tools/metrics/histograms/enums.xml.
enum class LockToSingleUserResult {
  // Successfully locked to single user.
  kSuccess = 0,
  // No response from DBus call.
  kNoResponse = 1,
  // Request failed on Chrome OS side.
  kFailedToLock = 2,
  // Expected device to already be locked to a single user
  kUnexpectedLockState = 3,
  kMaxValue = kUnexpectedLockState,
};

void RecordDBusResult(LockToSingleUserResult result) {
  base::UmaHistogramEnumeration("Enterprise.LockToSingleUserResult", result);
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
  if (user->IsAffiliated())
    return;

  int policy_value = -1;
  if (!chromeos::CrosSettings::Get()->GetInteger(
          chromeos::kDeviceRebootOnUserSignout, &policy_value)) {
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
      arc_session_observer_.Add(arc::ArcSessionManager::Get());
      break;
    case RebootOnSignOutPolicy::ARC_SESSION:
      arc_session_observer_.Add(arc::ArcSessionManager::Get());
      break;
    case RebootOnSignOutPolicy::NEVER:
      break;
  }
}

void LockToSingleUserManager::OnArcStarted() {
  arc_session_observer_.RemoveAll();
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
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);

  crostini::CrostiniManager::GetForProfile(profile)->AddVmStartingObserver(
      this);
  plugin_vm::PluginVmManager::GetForProfile(profile)->AddVmStartingObserver(
      this);

  GetConciergeClient()->AddVmObserver(this);

  lock_to_single_user_on_dbus_call_ = true;
}

void LockToSingleUserManager::LockToSingleUser() {
  cryptohome::AccountIdentifier account_id =
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  RebootOnSignOutRequest request;
  request.mutable_account_id()->CopyFrom(account_id);
  chromeos::CryptohomeClient::Get()->LockToSingleUserMountUntilReboot(
      request,
      base::BindOnce(
          &LockToSingleUserManager::OnLockToSingleUserMountUntilRebootDone,
          weak_factory_.GetWeakPtr()));
}

void LockToSingleUserManager::OnLockToSingleUserMountUntilRebootDone(
    base::Optional<cryptohome::BaseReply> reply) {
  if (!reply || !reply->HasExtension(RebootOnSignOutReply::reply)) {
    LOG(ERROR) << "Signing out user: no reply from "
                  "LockToSingleUserMountUntilReboot D-Bus call.";
    RecordDBusResult(LockToSingleUserResult::kNoResponse);
    chrome::AttemptUserExit();
    return;
  }

  // Force user logout if failed to lock the device to single user mount.
  const RebootOnSignOutReply extension =
      reply->GetExtension(RebootOnSignOutReply::reply);

  if (extension.result() == RebootOnSignOutResult::SUCCESS ||
      extension.result() == RebootOnSignOutResult::PCR_ALREADY_EXTENDED) {
    // The device is locked to single user on TPM level. Update the cache in
    // SessionTerminationManager, so that it triggers reboot on sign out.
    chromeos::SessionTerminationManager::Get()->SetDeviceLockedToSingleUser();

    if (expect_to_be_locked_ &&
        extension.result() != RebootOnSignOutResult::PCR_ALREADY_EXTENDED) {
      RecordDBusResult(LockToSingleUserResult::kUnexpectedLockState);
    } else {
      RecordDBusResult(LockToSingleUserResult::kSuccess);
    }
  } else {
    LOG(ERROR) << "Signing out user: failed to lock device to single user: "
               << extension.result();
    RecordDBusResult(LockToSingleUserResult::kFailedToLock);
    chrome::AttemptUserExit();
  }
}

}  // namespace policy
