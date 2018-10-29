// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"

#include <utility>

#include "apps/app_lifetime_monitor.h"
#include "apps/app_lifetime_monitor_factory.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/easy_unlock/chrome_proximity_auth_client.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_app_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_observer.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_local_state_pref_manager.h"
#include "chromeos/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/components/proximity_auth/switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/cryptauth/cryptauth_client_impl.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/version_info/version_info.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

using proximity_auth::ScreenlockState;

namespace chromeos {

namespace {

PrefService* GetLocalState() {
  return g_browser_process ? g_browser_process->local_state() : NULL;
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

class EasyUnlockService::BluetoothDetector
    : public device::BluetoothAdapter::Observer,
      public apps::AppLifetimeMonitor::Observer {
 public:
  explicit BluetoothDetector(EasyUnlockService* service)
      : service_(service), weak_ptr_factory_(this) {
    apps::AppLifetimeMonitorFactory::GetForBrowserContext(service_->profile())
        ->AddObserver(this);
  }

  ~BluetoothDetector() override {
    if (adapter_.get())
      adapter_->RemoveObserver(this);
    apps::AppLifetimeMonitorFactory::GetForBrowserContext(service_->profile())
        ->RemoveObserver(this);
  }

  void Initialize() {
    if (!device::BluetoothAdapterFactory::IsBluetoothSupported())
      return;

    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothDetector::OnAdapterInitialized,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  bool IsPresent() const { return adapter_.get() && adapter_->IsPresent(); }

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override {
    service_->OnBluetoothAdapterPresentChanged();
  }

 private:
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter) {
    adapter_ = adapter;
    adapter_->AddObserver(this);
    service_->OnBluetoothAdapterPresentChanged();

    // TODO(tengs): At the moment, there is no way for Bluetooth discoverability
    // to be turned on except through the Easy Unlock setup. If we step on any
    // toes in the future then we need to revisit this guard.
    if (adapter_->IsDiscoverable())
      TurnOffBluetoothDiscoverability();
  }

  // apps::AppLifetimeMonitor::Observer:
  void OnAppDeactivated(content::BrowserContext* context,
                        const std::string& app_id) override {
    // TODO(tengs): Refactor the lifetime management to EasyUnlockAppManager.
    if (app_id == extension_misc::kEasyUnlockAppId)
      TurnOffBluetoothDiscoverability();
  }

  void OnAppStop(content::BrowserContext* context,
                 const std::string& app_id) override {
    // TODO(tengs): Refactor the lifetime management to EasyUnlockAppManager.
    if (app_id == extension_misc::kEasyUnlockAppId)
      TurnOffBluetoothDiscoverability();
  }

  void TurnOffBluetoothDiscoverability() {
    if (adapter_) {
      adapter_->SetDiscoverable(false, base::DoNothing(), base::DoNothing());
    }
  }

  // Owner of this class and should out-live this class.
  EasyUnlockService* service_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::WeakPtrFactory<BluetoothDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetector);
};

class EasyUnlockService::PowerMonitor : public PowerManagerClient::Observer {
 public:
  explicit PowerMonitor(EasyUnlockService* service)
      : service_(service), waking_up_(false), weak_ptr_factory_(this) {
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  }

  ~PowerMonitor() override {
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  }

  // Called when the remote device has been authenticated to record the time
  // delta from waking up. No time will be recorded if the start-up time has
  // already been recorded or if the system never went to sleep previously.
  void RecordStartUpTime() {
    if (wake_up_time_.is_null())
      return;
    UMA_HISTOGRAM_MEDIUM_TIMES("EasyUnlock.StartupTimeFromSuspend",
                               base::Time::Now() - wake_up_time_);
    wake_up_time_ = base::Time();
  }

  bool waking_up() const { return waking_up_; }

 private:
  // PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override {
    service_->PrepareForSuspend();
  }

  void SuspendDone(const base::TimeDelta& sleep_duration) override {
    waking_up_ = true;
    wake_up_time_ = base::Time::Now();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerMonitor::ResetWakingUp,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(5));
    service_->OnSuspendDone();
    service_->UpdateAppState();
    // Note that |this| may get deleted after |UpdateAppState| is called.
  }

  void ResetWakingUp() {
    waking_up_ = false;
    service_->UpdateAppState();
  }

  EasyUnlockService* service_;
  bool waking_up_;
  base::Time wake_up_time_;
  base::WeakPtrFactory<PowerMonitor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitor);
};

EasyUnlockService::EasyUnlockService(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client)
    : profile_(profile),
      secure_channel_client_(secure_channel_client),
      proximity_auth_client_(profile),
      bluetooth_detector_(new BluetoothDetector(this)),
      shut_down_(false),
      tpm_key_checked_(false),
      weak_ptr_factory_(this) {}

EasyUnlockService::~EasyUnlockService() {}

// static
void EasyUnlockService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kEasyUnlockPairing,
                                   std::make_unique<base::DictionaryValue>());

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
  update->RemoveWithoutPathExpansion(account_id.GetUserEmail(), NULL);

  EasyUnlockTpmKeyManager::ResetLocalStateForUser(account_id);
}

void EasyUnlockService::Initialize(
    std::unique_ptr<EasyUnlockAppManager> app_manager) {
  app_manager_ = std::move(app_manager);
  app_manager_->EnsureReady(
      base::Bind(&EasyUnlockService::InitializeOnAppManagerReady,
                 weak_ptr_factory_.GetWeakPtr()));
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

  if (!bluetooth_detector_->IsPresent())
    return false;

  return true;
}

bool EasyUnlockService::IsEnabled() const {
  return false;
}

bool EasyUnlockService::IsChromeOSLoginEnabled() const {
  return false;
}

bool EasyUnlockService::IsInLegacyHostMode() const {
  return false;
}

void EasyUnlockService::OpenSetupApp() {
  app_manager_->LaunchSetup();
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
        proximity_auth::ScreenlockBridge::Get()));
  }
  return screenlock_state_handler_.get();
}

bool EasyUnlockService::UpdateScreenlockState(ScreenlockState state) {
  EasyUnlockScreenlockStateHandler* handler = GetScreenlockStateHandler();
  if (!handler)
    return false;

  handler->ChangeState(state);

  if (state == ScreenlockState::AUTHENTICATED) {
    if (power_monitor_)
      power_monitor_->RecordStartUpTime();
  } else if (auth_attempt_) {
    // Clean up existing auth attempt if we can no longer authenticate the
    // remote device.
    auth_attempt_.reset();

    if (!handler->InStateValidOnRemoteAuthFailure())
      HandleAuthFailure(GetAccountId());
  }

  for (EasyUnlockServiceObserver& observer : observers_)
    observer.OnScreenlockStateChanged(state);
  return true;
}

void EasyUnlockService::AttemptAuth(const AccountId& account_id) {
  const EasyUnlockAuthAttempt::Type auth_attempt_type =
      GetType() == TYPE_REGULAR ? EasyUnlockAuthAttempt::TYPE_UNLOCK
                                : EasyUnlockAuthAttempt::TYPE_SIGNIN;
  if (auth_attempt_) {
    PA_LOG(INFO) << "Already attempting auth, skipping this request.";
    return;
  }

  if (!GetAccountId().is_valid()) {
    PA_LOG(ERROR) << "Empty user account. Auth attempt failed.";
    return;
  }

  CHECK(GetAccountId() == account_id)
      << "Check failed: " << GetAccountId().Serialize() << " vs "
      << account_id.Serialize();

  auth_attempt_.reset(new EasyUnlockAuthAttempt(app_manager_.get(), account_id,
                                                auth_attempt_type));
  if (!auth_attempt_->Start())
    auth_attempt_.reset();

  // TODO(tengs): We notify ProximityAuthSystem whenever unlock attempts are
  // attempted. However, we ideally should refactor the auth attempt logic to
  // the proximity_auth component.
  if (proximity_auth_system_)
    proximity_auth_system_->OnAuthAttempted(account_id);
}

void EasyUnlockService::FinalizeUnlock(bool success) {
  if (!auth_attempt_)
    return;

  this->OnWillFinalizeUnlock(success);
  auth_attempt_->FinalizeUnlock(GetAccountId(), success);
  auth_attempt_.reset();
  // TODO(isherman): If observing screen unlock events, is there a race
  // condition in terms of reading the service's state vs. the app setting the
  // state?

  // Make sure that the lock screen is updated on failure.
  if (!success) {
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
  auth_attempt_.reset();

  // Processing empty key is equivalent to auth cancellation. In this case the
  // signin request will not actually be processed by login stack, so the lock
  // screen state should be set from here.
  if (key.empty())
    HandleAuthFailure(GetAccountId());
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
      base::Bind(&EasyUnlockService::OnCryptohomeKeysFetchedForChecking,
                 weak_ptr_factory_.GetWeakPtr(), account_id, paired_devices));
}

void EasyUnlockService::SetTrialRun() {
  DCHECK_EQ(GetType(), TYPE_REGULAR);

  EasyUnlockScreenlockStateHandler* handler = GetScreenlockStateHandler();
  if (handler)
    handler->SetTrialRun();
}

void EasyUnlockService::AddObserver(EasyUnlockServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void EasyUnlockService::RemoveObserver(EasyUnlockServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void EasyUnlockService::Shutdown() {
  if (shut_down_)
    return;
  shut_down_ = true;

  ShutdownInternal();

  ResetScreenlockState();
  bluetooth_detector_.reset();
  proximity_auth_system_.reset();
  power_monitor_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EasyUnlockService::UpdateAppState() {
  if (IsAllowed()) {
    EnsureTpmKeyPresentIfNeeded();
    app_manager_->LoadApp();

    if (proximity_auth_system_)
      proximity_auth_system_->Start();

    if (!power_monitor_)
      power_monitor_.reset(new PowerMonitor(this));
  } else {
    bool bluetooth_waking_up = false;

    // If the service is not allowed due to bluetooth not being detected just
    // after system suspend is done, give bluetooth more time to be detected
    // before disabling the app (and resetting screenlock state).
    bluetooth_waking_up = power_monitor_.get() && power_monitor_->waking_up() &&
                          !bluetooth_detector_->IsPresent();

    if (!bluetooth_waking_up) {
      app_manager_->DisableAppIfLoaded();

      if (proximity_auth_system_)
        proximity_auth_system_->Stop();

      power_monitor_.reset();
    }
  }
}

void EasyUnlockService::DisableAppWithoutResettingScreenlockState() {
  app_manager_->DisableAppIfLoaded();
}

void EasyUnlockService::NotifyTurnOffOperationStatusChanged() {
  for (EasyUnlockServiceObserver& observer : observers_)
    observer.OnTurnOffOperationStatusChanged();
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

void EasyUnlockService::InitializeOnAppManagerReady() {
  CHECK(app_manager_.get());

  InitializeInternal();

  bluetooth_detector_->Initialize();
}

void EasyUnlockService::OnBluetoothAdapterPresentChanged() {
  UpdateAppState();
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
      case EasyUnlockScreenlockStateHandler::PASSWORD_REQUIRED_FOR_LOGIN:
        return PASSWORD_ENTRY_REQUIRED_FOR_LOGIN;
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
      case ScreenlockState::PHONE_UNSUPPORTED:
        return PASSWORD_ENTRY_PHONE_UNSUPPORTED;
      case ScreenlockState::RSSI_TOO_LOW:
        return PASSWORD_ENTRY_RSSI_TOO_LOW;
      case ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW:
        return PASSWORD_ENTRY_PHONE_LOCKED_AND_RSSI_TOO_LOW;
      case ScreenlockState::AUTHENTICATED:
        return PASSWORD_ENTRY_WITH_AUTHENTICATED_PHONE;
      case ScreenlockState::PASSWORD_REAUTH:
        return PASSWORD_ENTRY_FORCED_REAUTH;
    }
  }

  NOTREACHED();
  return EASY_UNLOCK_AUTH_EVENT_COUNT;
}

void EasyUnlockService::SetProximityAuthDevices(
    const AccountId& account_id,
    const cryptauth::RemoteDeviceRefList& remote_devices,
    base::Optional<cryptauth::RemoteDeviceRef> local_device) {
  UMA_HISTOGRAM_COUNTS_100("SmartLock.EnabledDevicesCount",
                           remote_devices.size());

  if (remote_devices.size() == 0) {
    proximity_auth_system_.reset();
    return;
  }

  if (!proximity_auth_system_) {
    PA_LOG(INFO) << "Creating ProximityAuthSystem.";
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

void EasyUnlockService::HandleUserReauth(const UserContext& user_context) {}

void EasyUnlockService::PrepareForSuspend() {
  app_manager_->DisableAppIfLoaded();
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
      true /* check_private_key */, base::Closure());

  tpm_key_checked_ = true;
}

}  // namespace chromeos
