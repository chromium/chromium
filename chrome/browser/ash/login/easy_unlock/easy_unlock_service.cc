// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/easy_unlock/chrome_proximity_auth_client.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_local_state_pref_manager.h"
#include "chromeos/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/version_info/version_info.h"

using proximity_auth::ScreenlockState;

namespace chromeos {

namespace {

PrefService* GetLocalState() {
  return g_browser_process ? g_browser_process->local_state() : NULL;
}

void RecordAuthResultFailure(
    EasyUnlockAuthAttempt::Type auth_attempt_type,
    SmartLockMetricsRecorder::SmartLockAuthResultFailureReason failure_reason) {
  if (auth_attempt_type == EasyUnlockAuthAttempt::TYPE_UNLOCK)
    SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(failure_reason);
  else if (auth_attempt_type == EasyUnlockAuthAttempt::TYPE_SIGNIN)
    SmartLockMetricsRecorder::RecordAuthResultSignInFailure(failure_reason);
}

}  // namespace

// static
EasyUnlockService* EasyUnlockService::Get(Profile* profile) {
  return EasyUnlockServiceFactory::GetForBrowserContext(profile);
}

// static
EasyUnlockService* EasyUnlockService::GetForUser(
    const user_manager::User& user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(&user);
  if (!profile)
    return NULL;
  return EasyUnlockService::Get(profile);
}

class EasyUnlockService::PowerMonitor : public PowerManagerClient::Observer {
 public:
  explicit PowerMonitor(EasyUnlockService* service) : service_(service) {
    PowerManagerClient::Get()->AddObserver(this);
  }

  ~PowerMonitor() override { PowerManagerClient::Get()->RemoveObserver(this); }

 private:
  // PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override {
    service_->PrepareForSuspend();
  }

  void SuspendDone(base::TimeDelta sleep_duration) override {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerMonitor::ResetWakingUp,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(5));
    service_->OnSuspendDone();
    service_->UpdateAppState();
    // Note that `this` may get deleted after `UpdateAppState` is called.
  }

  void ResetWakingUp() { service_->UpdateAppState(); }

  EasyUnlockService* service_;
  base::WeakPtrFactory<PowerMonitor> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PowerMonitor);
};

EasyUnlockService::EasyUnlockService(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client)
    : profile_(profile),
      secure_channel_client_(secure_channel_client),
      proximity_auth_client_(profile),
      shut_down_(false),
      tpm_key_checked_(false) {}

EasyUnlockService::~EasyUnlockService() = default;

// static
void EasyUnlockService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kEasyUnlockPairing);
  proximity_auth::ProximityAuthProfilePrefManager::RegisterPrefs(registry);
}

// static
void EasyUnlockService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kEasyUnlockHardlockState);
  EasyUnlockTpmKeyManager::RegisterLocalStatePrefs(registry);
  proximity_auth::ProximityAuthLocalStatePrefManager::RegisterPrefs(registry);
}

// static
void EasyUnlockService::ResetLocalStateForUser(const AccountId& account_id) {
  DCHECK(account_id.is_valid());

  PrefService* local_state = GetLocalState();
  if (!local_state)
    return;

  DictionaryPrefUpdate update(local_state, prefs::kEasyUnlockHardlockState);
  update->RemoveKey(account_id.GetUserEmail());

  EasyUnlockTpmKeyManager::ResetLocalStateForUser(account_id);
}

void EasyUnlockService::Initialize() {
  InitializeInternal();
}

proximity_auth::ProximityAuthPrefManager*
EasyUnlockService::GetProximityAuthPrefManager() {
  NOTREACHED();
  return nullptr;
}

bool EasyUnlockService::IsAllowed() const {
  if (shut_down_)
    return false;

  if (!IsAllowedInternal())
    return false;

  return true;
}

bool EasyUnlockService::IsEnabled() const {
  return false;
}

bool EasyUnlockService::IsChromeOSLoginEnabled() const {
  return false;
}

void EasyUnlockService::SetHardlockState(
    EasyUnlockScreenlockStateHandler::HardlockState state) {
  const AccountId& account_id = GetAccountId();
  if (!account_id.is_valid())
    return;

  if (state == GetHardlockState())
    return;

  SetHardlockStateForUser(account_id, state);
}

EasyUnlockScreenlockStateHandler::HardlockState
EasyUnlockService::GetHardlockState() const {
  EasyUnlockScreenlockStateHandler::HardlockState state;
  if (GetPersistedHardlockState(&state))
    return state;

  return EasyUnlockScreenlockStateHandler::NO_HARDLOCK;
}

bool EasyUnlockService::GetPersistedHardlockState(
    EasyUnlockScreenlockStateHandler::HardlockState* state) const {
  const AccountId& account_id = GetAccountId();
  if (!account_id.is_valid())
    return false;

  PrefService* local_state = GetLocalState();
  if (!local_state)
    return false;

  const base::DictionaryValue* dict =
      local_state->GetDictionary(prefs::kEasyUnlockHardlockState);
  int state_int;
  if (dict && dict->GetIntegerWithoutPathExpansion(account_id.GetUserEmail(),
                                                   &state_int)) {
    *state =
        static_cast<EasyUnlockScreenlockStateHandler::HardlockState>(state_int);
    return true;
  }

  return false;
}

EasyUnlockScreenlockStateHandler*
EasyUnlockService::GetScreenlockStateHandler() {
  if (!IsAllowed())
    return NULL;
  if (!screenlock_state_handler_) {
    screenlock_state_handler_.reset(new EasyUnlockScreenlockStateHandler(
        GetAccountId(), GetHardlockState(),
        proximity_auth::ScreenlockBridge::Get(),
        GetProximityAuthPrefManager()));
  }
  return screenlock_state_handler_.get();
}

bool EasyUnlockService::UpdateScreenlockState(ScreenlockState state) {
  EasyUnlockScreenlockStateHandler* handler = GetScreenlockStateHandler();
  if (!handler)
    return false;

  handler->ChangeState(state);

  if (state != ScreenlockState::AUTHENTICATED && auth_attempt_) {
    // Clean up existing auth attempt if we can no longer authenticate the
    // remote device.
    auth_attempt_.reset();

    if (!handler->InStateValidOnRemoteAuthFailure())
      HandleAuthFailure(GetAccountId());
  }

  return true;
}

void EasyUnlockService::OnUserEnteredPassword() {
  if (proximity_auth_system_)
    proximity_auth_system_->CancelConnectionAttempt();
}

bool EasyUnlockService::AttemptAuth(const AccountId& account_id) {
  const EasyUnlockAuthAttempt::Type auth_attempt_type =
      GetType() == TYPE_REGULAR ? EasyUnlockAuthAttempt::TYPE_UNLOCK
                                : EasyUnlockAuthAttempt::TYPE_SIGNIN;
  PA_LOG(VERBOSE) << "User began auth attempt (unlock or sign in attempt).";

  if (auth_attempt_) {
    PA_LOG(VERBOSE) << "Already attempting auth, skipping this request.";
    return false;
  }

  if (!GetAccountId().is_valid()) {
    PA_LOG(ERROR) << "Empty user account. Auth attempt failed.";
    RecordAuthResultFailure(
        auth_attempt_type,
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kEmptyUserAccount);
    return false;
  }

  if (GetAccountId() != account_id) {
    RecordAuthResultFailure(
        auth_attempt_type,
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kInvalidAccoundId);

    PA_LOG(ERROR) << "Check failed: " << GetAccountId().Serialize() << " vs "
                  << account_id.Serialize();
    return false;
  }

  auth_attempt_.reset(new EasyUnlockAuthAttempt(account_id, auth_attempt_type));
  if (!auth_attempt_->Start()) {
    RecordAuthResultFailure(
        auth_attempt_type,
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kAuthAttemptCannotStart);
    auth_attempt_.reset();
    return false;
  }

  // TODO(tengs): We notify ProximityAuthSystem whenever unlock attempts are
  // attempted. However, we ideally should refactor the auth attempt logic to
  // the proximity_auth component.
  if (!proximity_auth_system_) {
    PA_LOG(ERROR) << "No ProximityAuthSystem present.";
    return false;
  }

  proximity_auth_system_->OnAuthAttempted();
  return true;
}

void EasyUnlockService::FinalizeUnlock(bool success) {
  if (!auth_attempt_)
    return;

  set_will_authenticate_using_easy_unlock(true);
  auth_attempt_->FinalizeUnlock(GetAccountId(), success);

  // If successful, allow |auth_attempt_| to continue until
  // UpdateScreenlockState() is called (indicating screen unlock).

  // Make sure that the lock screen is updated on failure.
  if (!success) {
    auth_attempt_.reset();
    RecordEasyUnlockScreenUnlockEvent(EASY_UNLOCK_FAILURE);
    HandleAuthFailure(GetAccountId());
  }
}

void EasyUnlockService::FinalizeSignin(const std::string& key) {
  if (!auth_attempt_)
    return;

  std::string wrapped_secret = GetWrappedSecret();
  if (!wrapped_secret.empty())
    auth_attempt_->FinalizeSignin(GetAccountId(), wrapped_secret, key);

  // If successful, allow |auth_attempt_| to continue until
  // UpdateScreenlockState() is called (indicating sign in).

  // Processing empty key is equivalent to auth cancellation. In this case the
  // signin request will not actually be processed by login stack, so the lock
  // screen state should be set from here.
  if (key.empty()) {
    auth_attempt_.reset();
    HandleAuthFailure(GetAccountId());
    return;
  }

  set_will_authenticate_using_easy_unlock(true);
}

void EasyUnlockService::HandleAuthFailure(const AccountId& account_id) {
  if (account_id != GetAccountId())
    return;

  if (!screenlock_state_handler_.get())
    return;

  screenlock_state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::LOGIN_FAILED);
}

void EasyUnlockService::CheckCryptohomeKeysAndMaybeHardlock() {
  const AccountId& account_id = GetAccountId();
  if (!account_id.is_valid() || !IsChromeOSLoginEnabled())
    return;

  const base::ListValue* device_list = GetRemoteDevices();
  std::set<std::string> paired_devices;
  if (device_list) {
    EasyUnlockDeviceKeyDataList parsed_paired;
    EasyUnlockKeyManager::RemoteDeviceRefListToDeviceDataList(*device_list,
                                                              &parsed_paired);
    for (const auto& device_key_data : parsed_paired)
      paired_devices.insert(device_key_data.psk);
  }
  if (paired_devices.empty()) {
    SetHardlockState(EasyUnlockScreenlockStateHandler::NO_PAIRING);
    return;
  }

  // No need to compare if a change is already recorded.
  if (GetHardlockState() == EasyUnlockScreenlockStateHandler::PAIRING_CHANGED ||
      GetHardlockState() == EasyUnlockScreenlockStateHandler::PAIRING_ADDED) {
    return;
  }

  EasyUnlockKeyManager* key_manager =
      UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
  DCHECK(key_manager);

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  key_manager->GetDeviceDataList(
      UserContext(*user),
      base::BindOnce(&EasyUnlockService::OnCryptohomeKeysFetchedForChecking,
                     weak_ptr_factory_.GetWeakPtr(), account_id,
                     paired_devices));
}

void EasyUnlockService::Shutdown() {
  if (shut_down_)
    return;
  shut_down_ = true;

  ShutdownInternal();

  ResetScreenlockState();
  proximity_auth_system_.reset();
  power_monitor_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EasyUnlockService::UpdateAppState() {
  if (IsAllowed()) {
    EnsureTpmKeyPresentIfNeeded();

    if (proximity_auth_system_)
      proximity_auth_system_->Start();

    if (!power_monitor_)
      power_monitor_.reset(new PowerMonitor(this));
  }
}

void EasyUnlockService::ResetScreenlockState() {
  screenlock_state_handler_.reset();
  auth_attempt_.reset();
}

void EasyUnlockService::SetScreenlockHardlockedState(
    EasyUnlockScreenlockStateHandler::HardlockState state) {
  if (GetScreenlockStateHandler()) {
    screenlock_state_handler_->SetHardlockState(state);
    screenlock_state_handler_->MaybeShowHardlockUI();
  }
  if (state != EasyUnlockScreenlockStateHandler::NO_HARDLOCK)
    auth_attempt_.reset();
}

void EasyUnlockService::SetHardlockStateForUser(
    const AccountId& account_id,
    EasyUnlockScreenlockStateHandler::HardlockState state) {
  DCHECK(account_id.is_valid());

  PrefService* local_state = GetLocalState();
  if (!local_state)
    return;

  // Disallow setting the hardlock state if the password is currently being
  // forced.
  if (GetScreenlockStateHandler() &&
      GetScreenlockStateHandler()->state() ==
          proximity_auth::ScreenlockState::PASSWORD_REAUTH) {
    return;
  }

  DictionaryPrefUpdate update(local_state, prefs::kEasyUnlockHardlockState);
  update->SetKey(account_id.GetUserEmail(),
                 base::Value(static_cast<int>(state)));

  if (GetAccountId() == account_id)
    SetScreenlockHardlockedState(state);
}

SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
EasyUnlockService::GetSmartUnlockPasswordAuthEvent() const {
  DCHECK(IsEnabled());

  if (GetHardlockState() != EasyUnlockScreenlockStateHandler::NO_HARDLOCK) {
    switch (GetHardlockState()) {
      case EasyUnlockScreenlockStateHandler::NO_PAIRING:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kNoPairing;
      case EasyUnlockScreenlockStateHandler::USER_HARDLOCK:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kUserHardlock;
      case EasyUnlockScreenlockStateHandler::PAIRING_CHANGED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPairingChanged;
      case EasyUnlockScreenlockStateHandler::LOGIN_FAILED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kLoginFailed;
      case EasyUnlockScreenlockStateHandler::PAIRING_ADDED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPairingAdded;
      case EasyUnlockScreenlockStateHandler::LOGIN_DISABLED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kLoginWithSmartLockDisabled;
      default:
        NOTREACHED();
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kUnknownState;
    }
  } else if (!screenlock_state_handler()) {
    return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
        kUnknownState;
  } else {
    switch (screenlock_state_handler()->state()) {
      case ScreenlockState::INACTIVE:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kServiceNotActive;
      case ScreenlockState::NO_BLUETOOTH:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kNoBluetooth;
      case ScreenlockState::BLUETOOTH_CONNECTING:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kBluetoothConnecting;
      case ScreenlockState::NO_PHONE:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kCouldNotConnectToPhone;
      case ScreenlockState::PHONE_NOT_AUTHENTICATED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kNotAuthenticated;
      case ScreenlockState::PHONE_LOCKED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneLocked;
      case ScreenlockState::RSSI_TOO_LOW:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kRssiTooLow;
      case ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneLockedAndRssiTooLow;
      case ScreenlockState::AUTHENTICATED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kAuthenticatedPhone;
      case ScreenlockState::PASSWORD_REAUTH:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kForcedReauth;
      case ScreenlockState::PHONE_NOT_LOCKABLE:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneNotLockable;
      case ScreenlockState::PRIMARY_USER_ABSENT:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPrimaryUserAbsent;
      default:
        NOTREACHED();
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kUnknownState;
    }
  }

  NOTREACHED();
  return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
      kUnknownState;
}

EasyUnlockAuthEvent EasyUnlockService::GetPasswordAuthEvent() const {
  DCHECK(IsEnabled());

  if (GetHardlockState() != EasyUnlockScreenlockStateHandler::NO_HARDLOCK) {
    switch (GetHardlockState()) {
      case EasyUnlockScreenlockStateHandler::NO_HARDLOCK:
        NOTREACHED();
        return EASY_UNLOCK_AUTH_EVENT_COUNT;
      case EasyUnlockScreenlockStateHandler::NO_PAIRING:
        return PASSWORD_ENTRY_NO_PAIRING;
      case EasyUnlockScreenlockStateHandler::USER_HARDLOCK:
        return PASSWORD_ENTRY_USER_HARDLOCK;
      case EasyUnlockScreenlockStateHandler::PAIRING_CHANGED:
        return PASSWORD_ENTRY_PAIRING_CHANGED;
      case EasyUnlockScreenlockStateHandler::LOGIN_FAILED:
        return PASSWORD_ENTRY_LOGIN_FAILED;
      case EasyUnlockScreenlockStateHandler::PAIRING_ADDED:
        return PASSWORD_ENTRY_PAIRING_ADDED;
      case EasyUnlockScreenlockStateHandler::LOGIN_DISABLED:
        return PASSWORD_ENTRY_LOGIN_DISABLED;
    }
  } else if (!screenlock_state_handler()) {
    return PASSWORD_ENTRY_NO_SCREENLOCK_STATE_HANDLER;
  } else {
    switch (screenlock_state_handler()->state()) {
      case ScreenlockState::INACTIVE:
        return PASSWORD_ENTRY_SERVICE_NOT_ACTIVE;
      case ScreenlockState::NO_BLUETOOTH:
        return PASSWORD_ENTRY_NO_BLUETOOTH;
      case ScreenlockState::BLUETOOTH_CONNECTING:
        return PASSWORD_ENTRY_BLUETOOTH_CONNECTING;
      case ScreenlockState::NO_PHONE:
        return PASSWORD_ENTRY_NO_PHONE;
      case ScreenlockState::PHONE_NOT_AUTHENTICATED:
        return PASSWORD_ENTRY_PHONE_NOT_AUTHENTICATED;
      case ScreenlockState::PHONE_LOCKED:
        return PASSWORD_ENTRY_PHONE_LOCKED;
      case ScreenlockState::PHONE_NOT_LOCKABLE:
        return PASSWORD_ENTRY_PHONE_NOT_LOCKABLE;
      case ScreenlockState::RSSI_TOO_LOW:
        return PASSWORD_ENTRY_RSSI_TOO_LOW;
      case ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW:
        return PASSWORD_ENTRY_PHONE_LOCKED_AND_RSSI_TOO_LOW;
      case ScreenlockState::AUTHENTICATED:
        return PASSWORD_ENTRY_WITH_AUTHENTICATED_PHONE;
      case ScreenlockState::PASSWORD_REAUTH:
        return PASSWORD_ENTRY_FORCED_REAUTH;
      case ScreenlockState::PRIMARY_USER_ABSENT:
        return PASSWORD_ENTRY_PRIMARY_USER_ABSENT;
    }
  }

  NOTREACHED();
  return EASY_UNLOCK_AUTH_EVENT_COUNT;
}

void EasyUnlockService::SetProximityAuthDevices(
    const AccountId& account_id,
    const multidevice::RemoteDeviceRefList& remote_devices,
    base::Optional<multidevice::RemoteDeviceRef> local_device) {
  UMA_HISTOGRAM_COUNTS_100("SmartLock.EnabledDevicesCount",
                           remote_devices.size());

  if (remote_devices.size() == 0) {
    proximity_auth_system_.reset();
    return;
  }

  if (!proximity_auth_system_) {
    PA_LOG(VERBOSE) << "Creating ProximityAuthSystem.";
    proximity_auth_system_.reset(new proximity_auth::ProximityAuthSystem(
        GetType() == TYPE_SIGNIN
            ? proximity_auth::ProximityAuthSystem::SIGN_IN
            : proximity_auth::ProximityAuthSystem::SESSION_LOCK,
        proximity_auth_client(), secure_channel_client_));
  }

  proximity_auth_system_->SetRemoteDevicesForUser(account_id, remote_devices,
                                                  local_device);
  proximity_auth_system_->Start();
}

void EasyUnlockService::OnCryptohomeKeysFetchedForChecking(
    const AccountId& account_id,
    const std::set<std::string> paired_devices,
    bool success,
    const EasyUnlockDeviceKeyDataList& key_data_list) {
  DCHECK(account_id.is_valid() && !paired_devices.empty());

  if (!success) {
    SetHardlockStateForUser(account_id,
                            EasyUnlockScreenlockStateHandler::NO_PAIRING);
    return;
  }

  std::set<std::string> devices_in_cryptohome;
  for (const auto& device_key_data : key_data_list)
    devices_in_cryptohome.insert(device_key_data.psk);

  if (paired_devices != devices_in_cryptohome ||
      GetHardlockState() == EasyUnlockScreenlockStateHandler::NO_PAIRING) {
    SetHardlockStateForUser(
        account_id, devices_in_cryptohome.empty()
                        ? EasyUnlockScreenlockStateHandler::PAIRING_ADDED
                        : EasyUnlockScreenlockStateHandler::PAIRING_CHANGED);
  }
}

void EasyUnlockService::PrepareForSuspend() {
  if (screenlock_state_handler_ && screenlock_state_handler_->IsActive())
    UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING);
  if (proximity_auth_system_)
    proximity_auth_system_->OnSuspend();
}

void EasyUnlockService::OnSuspendDone() {
  if (proximity_auth_system_)
    proximity_auth_system_->OnSuspendDone();
}

void EasyUnlockService::EnsureTpmKeyPresentIfNeeded() {
  if (tpm_key_checked_ || GetType() != TYPE_REGULAR || GetAccountId().empty() ||
      GetHardlockState() == EasyUnlockScreenlockStateHandler::NO_PAIRING) {
    return;
  }

  // If this is called before the session is started, the chances are Chrome
  // is restarting in order to apply user flags. Don't check TPM keys in this
  // case.
  if (!session_manager::SessionManager::Get() ||
      !session_manager::SessionManager::Get()->IsSessionStarted())
    return;

  // TODO(tbarzic): Set check_private_key only if previous sign-in attempt
  // failed.
  EasyUnlockTpmKeyManagerFactory::GetInstance()->Get(profile_)->PrepareTpmKey(
      /*check_private_key=*/true, base::OnceClosure());

  tpm_key_checked_ = true;
}

}  // namespace chromeos
