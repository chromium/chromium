// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/smartlock_state.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
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
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_local_state_pref_manager.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

PrefService* GetLocalState() {
  return g_browser_process ? g_browser_process->local_state() : nullptr;
}

void RecordAuthResultFailure(
    EasyUnlockAuthAttempt::Type auth_attempt_type,
    SmartLockMetricsRecorder::SmartLockAuthResultFailureReason failure_reason) {
  if (auth_attempt_type == EasyUnlockAuthAttempt::TYPE_UNLOCK)
    SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(failure_reason);
  else if (auth_attempt_type == EasyUnlockAuthAttempt::TYPE_SIGNIN)
    SmartLockMetricsRecorder::RecordAuthResultSignInFailure(failure_reason);
}

void SetAuthTypeIfChanged(
    proximity_auth::ScreenlockBridge::LockHandler* lock_handler,
    const AccountId& account_id,
    proximity_auth::mojom::AuthType auth_type,
    const std::u16string& auth_value) {
  DCHECK(lock_handler);
  const proximity_auth::mojom::AuthType existing_auth_type =
      lock_handler->GetAuthType(account_id);
  if (auth_type == existing_auth_type)
    return;

  lock_handler->SetAuthType(account_id, auth_type, auth_value);
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
    return nullptr;
  return EasyUnlockService::Get(profile);
}

class EasyUnlockService::PowerMonitor
    : public chromeos::PowerManagerClient::Observer {
 public:
  explicit PowerMonitor(EasyUnlockService* service) : service_(service) {
    chromeos::PowerManagerClient::Get()->AddObserver(this);
  }

  PowerMonitor(const PowerMonitor&) = delete;
  PowerMonitor& operator=(const PowerMonitor&) = delete;

  ~PowerMonitor() override {
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  }

 private:
  // PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override {
    service_->PrepareForSuspend();
  }

  void SuspendDone(base::TimeDelta sleep_duration) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerMonitor::ResetWakingUp,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(5));
    service_->OnSuspendDone();
    service_->UpdateAppState();
    // Note that `this` may get deleted after `UpdateAppState` is called.
  }

  void ResetWakingUp() { service_->UpdateAppState(); }

  EasyUnlockService* service_;
  base::WeakPtrFactory<PowerMonitor> weak_ptr_factory_{this};
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

  ScopedDictPrefUpdate update(local_state, prefs::kEasyUnlockHardlockState);
  update->Remove(account_id.GetUserEmail());

  EasyUnlockTpmKeyManager::ResetLocalStateForUser(account_id);
}

void EasyUnlockService::Initialize() {
  proximity_auth::ScreenlockBridge::Get()->AddObserver(this);

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

SmartLockState EasyUnlockService::GetInitialSmartLockState() const {
  if (IsAllowed() && IsEnabled() && proximity_auth_system_ != nullptr)
    return SmartLockState::kConnectingToPhone;

  return SmartLockState::kDisabled;
}

void EasyUnlockService::SetHardlockState(
    SmartLockStateHandler::HardlockState state) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
    return;

  const AccountId& account_id = GetAccountId();
  if (!account_id.is_valid())
    return;

  if (state == GetHardlockState())
    return;

  SetHardlockStateForUser(account_id, state);
}

SmartLockStateHandler::HardlockState EasyUnlockService::GetHardlockState()
    const {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
    return SmartLockStateHandler::NO_HARDLOCK;

  SmartLockStateHandler::HardlockState state;
  if (GetPersistedHardlockState(&state))
    return state;

  return SmartLockStateHandler::NO_HARDLOCK;
}

bool EasyUnlockService::GetPersistedHardlockState(
    SmartLockStateHandler::HardlockState* state) const {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
    return false;

  const AccountId& account_id = GetAccountId();
  if (!account_id.is_valid())
    return false;

  PrefService* local_state = GetLocalState();
  if (!local_state)
    return false;

  const base::Value::Dict& dict =
      local_state->GetDict(prefs::kEasyUnlockHardlockState);

  absl::optional<int> state_int = dict.FindInt(account_id.GetUserEmail());
  if (!state_int.has_value())
    return false;

  *state = static_cast<SmartLockStateHandler::HardlockState>(state_int.value());
  return true;
}

SmartLockStateHandler* EasyUnlockService::GetSmartLockStateHandler() {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
    return nullptr;

  if (!IsAllowed())
    return nullptr;
  if (!smartlock_state_handler_) {
    smartlock_state_handler_ = std::make_unique<SmartLockStateHandler>(
        GetAccountId(), GetHardlockState(),
        proximity_auth::ScreenlockBridge::Get(), GetProximityAuthPrefManager());
  }
  return smartlock_state_handler_.get();
}

void EasyUnlockService::UpdateSmartLockState(SmartLockState state) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    if (smart_lock_state_ && state == smart_lock_state_.value())
      return;

    smart_lock_state_ = state;

    if (proximity_auth::ScreenlockBridge::Get()->IsLocked()) {
      auto* lock_handler =
          proximity_auth::ScreenlockBridge::Get()->lock_handler();
      DCHECK(lock_handler);

      lock_handler->SetSmartLockState(GetAccountId(), state);

      // TODO(https://crbug.com/1233614): Eventually we would like to remove
      // auth_type.mojom where AuthType lives, but this will require further
      // investigation. This logic was copied from
      // SmartLockStateHandler::UpdateScreenlockAuthType.
      // Do not override online signin.
      if (lock_handler->GetAuthType(GetAccountId()) !=
          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
        if (smart_lock_state_ == SmartLockState::kPhoneAuthenticated) {
          SetAuthTypeIfChanged(
              lock_handler, GetAccountId(),
              proximity_auth::mojom::AuthType::USER_CLICK,
              l10n_util::GetStringUTF16(
                  IDS_EASY_UNLOCK_SCREENLOCK_USER_POD_AUTH_VALUE));
        } else {
          SetAuthTypeIfChanged(
              lock_handler, GetAccountId(),
              proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
              std::u16string());
        }
      }
    }

    if (state != SmartLockState::kPhoneAuthenticated && auth_attempt_) {
      // Clean up existing auth attempt if we can no longer authenticate the
      // remote device.
      auth_attempt_.reset();

      if (!IsSmartLockStateValidOnRemoteAuthFailure())
        HandleAuthFailure(GetAccountId());
    }

    return;
  }

  SmartLockStateHandler* handler = GetSmartLockStateHandler();
  if (!handler)
    return;

  handler->ChangeState(state);

  if (state != SmartLockState::kPhoneAuthenticated && auth_attempt_) {
    auth_attempt_.reset();

    if (!handler->InStateValidOnRemoteAuthFailure())
      HandleAuthFailure(GetAccountId());
  }
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

  auth_attempt_ =
      std::make_unique<EasyUnlockAuthAttempt>(account_id, auth_attempt_type);
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
  // UpdateSmartLockState() is called (indicating screen unlock).

  // Make sure that the lock screen is updated on failure.
  if (!success) {
    auth_attempt_.reset();
    RecordEasyUnlockScreenUnlockEvent(EASY_UNLOCK_FAILURE);
    if (!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
      HandleAuthFailure(GetAccountId());
    }
  }

  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    NotifySmartLockAuthResult(success);
  }
}

void EasyUnlockService::FinalizeSignin(const std::string& key) {
  if (!auth_attempt_)
    return;

  std::string wrapped_secret = GetWrappedSecret();
  if (!wrapped_secret.empty())
    auth_attempt_->FinalizeSignin(GetAccountId(), wrapped_secret, key);

  // If successful, allow |auth_attempt_| to continue until
  // UpdateSmartLockState() is called (indicating sign in).

  // Processing empty key is equivalent to auth cancellation. In this case the
  // signin request will not actually be processed by login stack, so the lock
  // screen state should be set from here.
  bool success = !key.empty();

  if (success) {
    set_will_authenticate_using_easy_unlock(true);
  } else {
    auth_attempt_.reset();
    if (!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
      HandleAuthFailure(GetAccountId());
    }
  }

  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    NotifySmartLockAuthResult(success);
  }
}

void EasyUnlockService::HandleAuthFailure(const AccountId& account_id) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    NotifySmartLockAuthResult(/*success=*/false);
    return;
  }

  if (account_id != GetAccountId())
    return;

  if (!smartlock_state_handler_.get())
    return;

  smartlock_state_handler_->SetHardlockState(
      SmartLockStateHandler::LOGIN_FAILED);
}

void EasyUnlockService::CheckCryptohomeKeysAndMaybeHardlock() {
  const AccountId& account_id = GetAccountId();
  if (!account_id.is_valid() || !IsChromeOSLoginEnabled())
    return;

  const base::Value::List* device_list = GetRemoteDevices();
  std::set<std::string> paired_devices;
  if (device_list) {
    EasyUnlockDeviceKeyDataList parsed_paired;
    EasyUnlockKeyManager::RemoteDeviceRefListToDeviceDataList(*device_list,
                                                              &parsed_paired);
    for (const auto& device_key_data : parsed_paired)
      paired_devices.insert(device_key_data.psk);
  }
  if (paired_devices.empty()) {
    SetHardlockState(SmartLockStateHandler::NO_PAIRING);
    return;
  }

  // No need to compare if a change is already recorded.
  if (GetHardlockState() == SmartLockStateHandler::PAIRING_CHANGED ||
      GetHardlockState() == SmartLockStateHandler::PAIRING_ADDED) {
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

  proximity_auth::ScreenlockBridge::Get()->RemoveObserver(this);

  ResetSmartLockState();
  proximity_auth_system_.reset();
  power_monitor_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EasyUnlockService::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    ShowInitialSmartLockState();
  }
}

void EasyUnlockService::UpdateAppState() {
  if (IsAllowed()) {
    EnsureTpmKeyPresentIfNeeded();

    if (proximity_auth_system_)
      proximity_auth_system_->Start();

    if (!power_monitor_)
      power_monitor_ = std::make_unique<PowerMonitor>(this);
  }
}

void EasyUnlockService::ShowInitialSmartLockState() {
  // Only proceed if the screen is locked to prevent the UI event from not
  // persisting within UpdateSmartLockState().
  //
  // Note: ScreenlockBridge::IsLocked() may return a false positive if the
  // system is "warming up" (for example, ScreenlockBridge::IsLocked() will
  // return false when EasyUnlockServiceSignin is first instantiated because of
  // initialization timing in UserSelectionScreen). To work around this race,
  // ShowInitialSmartLockState() is also called from OnScreenDidLock() (which
  // triggers when ScreenlockBridge::IsLocked() becomes true) to ensure that
  // an initial state is displayed in the UI.
  auto* screenlock_bridge = proximity_auth::ScreenlockBridge::Get();
  if (screenlock_bridge && screenlock_bridge->IsLocked()) {
    UpdateSmartLockState(GetInitialSmartLockState());
  }
}

void EasyUnlockService::ResetSmartLockState() {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    smart_lock_state_.reset();
  } else {
    smartlock_state_handler_.reset();
  }

  auth_attempt_.reset();
}

void EasyUnlockService::SetSmartLockHardlockedState(
    SmartLockStateHandler::HardlockState state) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
    return;

  if (GetSmartLockStateHandler()) {
    smartlock_state_handler_->SetHardlockState(state);
    smartlock_state_handler_->MaybeShowHardlockUI();
  }
  if (state != SmartLockStateHandler::NO_HARDLOCK)
    auth_attempt_.reset();
}

void EasyUnlockService::SetHardlockStateForUser(
    const AccountId& account_id,
    SmartLockStateHandler::HardlockState state) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
    return;

  DCHECK(account_id.is_valid());

  PrefService* local_state = GetLocalState();
  if (!local_state)
    return;

  // Disallow setting the hardlock state if the password is currently being
  // forced.
  if (GetSmartLockStateHandler() &&
      GetSmartLockStateHandler()->state() ==
          SmartLockState::kPasswordReentryRequired) {
    return;
  }

  ScopedDictPrefUpdate update(local_state, prefs::kEasyUnlockHardlockState);
  update->Set(account_id.GetUserEmail(), static_cast<int>(state));

  if (GetAccountId() == account_id)
    SetSmartLockHardlockedState(state);
}

SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
EasyUnlockService::GetSmartUnlockPasswordAuthEvent() const {
  DCHECK(IsEnabled());

  if (GetHardlockState() != SmartLockStateHandler::NO_HARDLOCK) {
    switch (GetHardlockState()) {
      case SmartLockStateHandler::NO_PAIRING:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kNoPairing;
      case SmartLockStateHandler::USER_HARDLOCK:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kUserHardlock;
      case SmartLockStateHandler::PAIRING_CHANGED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPairingChanged;
      case SmartLockStateHandler::LOGIN_FAILED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kLoginFailed;
      case SmartLockStateHandler::PAIRING_ADDED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPairingAdded;
      case SmartLockStateHandler::LOGIN_DISABLED:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kLoginWithSmartLockDisabled;
      default:
        NOTREACHED();
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kUnknownState;
    }
  } else if (!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp) &&
             !smartlock_state_handler()) {
    return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
        kUnknownState;
  } else if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp) &&
             !smart_lock_state_) {
    return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
        kUnknownState;
  } else {
    SmartLockState state =
        (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
            ? smart_lock_state_.value()
            : smartlock_state_handler()->state();
    switch (state) {
      case SmartLockState::kInactive:
      case SmartLockState::kDisabled:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kServiceNotActive;
      case SmartLockState::kBluetoothDisabled:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kNoBluetooth;
      case SmartLockState::kConnectingToPhone:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kBluetoothConnecting;
      case SmartLockState::kPhoneNotFound:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kCouldNotConnectToPhone;
      case SmartLockState::kPhoneNotAuthenticated:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kNotAuthenticated;
      case SmartLockState::kPhoneFoundLockedAndProximate:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneLocked;
      case SmartLockState::kPhoneFoundUnlockedAndDistant:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kRssiTooLow;
      case SmartLockState::kPhoneFoundLockedAndDistant:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneLockedAndRssiTooLow;
      case SmartLockState::kPhoneAuthenticated:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kAuthenticatedPhone;
      case SmartLockState::kPasswordReentryRequired:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kForcedReauth;
      case SmartLockState::kPhoneNotLockable:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneNotLockable;
      case SmartLockState::kPrimaryUserAbsent:
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

  if (GetHardlockState() != SmartLockStateHandler::NO_HARDLOCK) {
    switch (GetHardlockState()) {
      case SmartLockStateHandler::NO_HARDLOCK:
        NOTREACHED();
        return EASY_UNLOCK_AUTH_EVENT_COUNT;
      case SmartLockStateHandler::NO_PAIRING:
        return PASSWORD_ENTRY_NO_PAIRING;
      case SmartLockStateHandler::USER_HARDLOCK:
        return PASSWORD_ENTRY_USER_HARDLOCK;
      case SmartLockStateHandler::PAIRING_CHANGED:
        return PASSWORD_ENTRY_PAIRING_CHANGED;
      case SmartLockStateHandler::LOGIN_FAILED:
        return PASSWORD_ENTRY_LOGIN_FAILED;
      case SmartLockStateHandler::PAIRING_ADDED:
        return PASSWORD_ENTRY_PAIRING_ADDED;
      case SmartLockStateHandler::LOGIN_DISABLED:
        return PASSWORD_ENTRY_LOGIN_DISABLED;
    }
  } else if (!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp) &&
             !smartlock_state_handler()) {
    return PASSWORD_ENTRY_NO_SMARTLOCK_STATE_HANDLER;
  } else if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp) &&
             !smart_lock_state_) {
    return PASSWORD_ENTRY_NO_SMARTLOCK_STATE_HANDLER;
  } else {
    SmartLockState state =
        (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
            ? smart_lock_state_.value()
            : smartlock_state_handler()->state();

    switch (state) {
      case SmartLockState::kInactive:
      case SmartLockState::kDisabled:
        return PASSWORD_ENTRY_SERVICE_NOT_ACTIVE;
      case SmartLockState::kBluetoothDisabled:
        return PASSWORD_ENTRY_NO_BLUETOOTH;
      case SmartLockState::kConnectingToPhone:
        return PASSWORD_ENTRY_BLUETOOTH_CONNECTING;
      case SmartLockState::kPhoneNotFound:
        return PASSWORD_ENTRY_NO_PHONE;
      case SmartLockState::kPhoneNotAuthenticated:
        return PASSWORD_ENTRY_PHONE_NOT_AUTHENTICATED;
      case SmartLockState::kPhoneFoundLockedAndProximate:
        return PASSWORD_ENTRY_PHONE_LOCKED;
      case SmartLockState::kPhoneNotLockable:
        return PASSWORD_ENTRY_PHONE_NOT_LOCKABLE;
      case SmartLockState::kPhoneFoundUnlockedAndDistant:
        return PASSWORD_ENTRY_RSSI_TOO_LOW;
      case SmartLockState::kPhoneFoundLockedAndDistant:
        return PASSWORD_ENTRY_PHONE_LOCKED_AND_RSSI_TOO_LOW;
      case SmartLockState::kPhoneAuthenticated:
        return PASSWORD_ENTRY_WITH_AUTHENTICATED_PHONE;
      case SmartLockState::kPasswordReentryRequired:
        return PASSWORD_ENTRY_FORCED_REAUTH;
      case SmartLockState::kPrimaryUserAbsent:
        return PASSWORD_ENTRY_PRIMARY_USER_ABSENT;
    }
  }

  NOTREACHED();
  return EASY_UNLOCK_AUTH_EVENT_COUNT;
}

void EasyUnlockService::SetProximityAuthDevices(
    const AccountId& account_id,
    const multidevice::RemoteDeviceRefList& remote_devices,
    absl::optional<multidevice::RemoteDeviceRef> local_device) {
  UMA_HISTOGRAM_COUNTS_100("SmartLock.EnabledDevicesCount",
                           remote_devices.size());

  if (remote_devices.size() == 0) {
    proximity_auth_system_.reset();
    return;
  }

  if (!proximity_auth_system_) {
    PA_LOG(VERBOSE) << "Creating ProximityAuthSystem.";
    proximity_auth_system_ =
        std::make_unique<proximity_auth::ProximityAuthSystem>(
            GetType() == TYPE_SIGNIN
                ? proximity_auth::ProximityAuthSystem::SIGN_IN
                : proximity_auth::ProximityAuthSystem::SESSION_LOCK,
            proximity_auth_client(), secure_channel_client_);
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
    SetHardlockStateForUser(account_id, SmartLockStateHandler::NO_PAIRING);
    return;
  }

  std::set<std::string> devices_in_cryptohome;
  for (const auto& device_key_data : key_data_list)
    devices_in_cryptohome.insert(device_key_data.psk);

  if (paired_devices != devices_in_cryptohome ||
      GetHardlockState() == SmartLockStateHandler::NO_PAIRING) {
    SetHardlockStateForUser(account_id,
                            devices_in_cryptohome.empty()
                                ? SmartLockStateHandler::PAIRING_ADDED
                                : SmartLockStateHandler::PAIRING_CHANGED);
  }
}

void EasyUnlockService::PrepareForSuspend() {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    if (smart_lock_state_ && *smart_lock_state_ != SmartLockState::kInactive) {
      ShowInitialSmartLockState();
    }
  } else {
    if (smartlock_state_handler_ && smartlock_state_handler_->IsActive()) {
      UpdateSmartLockState(SmartLockState::kConnectingToPhone);
    }
  }

  if (proximity_auth_system_)
    proximity_auth_system_->OnSuspend();
}

void EasyUnlockService::OnSuspendDone() {
  if (proximity_auth_system_)
    proximity_auth_system_->OnSuspendDone();
}

void EasyUnlockService::EnsureTpmKeyPresentIfNeeded() {
  if (tpm_key_checked_ || GetType() != TYPE_REGULAR || GetAccountId().empty() ||
      GetHardlockState() == SmartLockStateHandler::NO_PAIRING) {
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

bool EasyUnlockService::IsSmartLockStateValidOnRemoteAuthFailure() const {
  // Note that NO_PHONE is not valid in this case because the phone may close
  // the connection if the auth challenge sent to it is invalid. This case
  // should be handled as authentication failure.
  return smart_lock_state_ == SmartLockState::kInactive ||
         smart_lock_state_ == SmartLockState::kDisabled ||
         smart_lock_state_ == SmartLockState::kBluetoothDisabled ||
         smart_lock_state_ == SmartLockState::kPhoneFoundLockedAndProximate;
}

void EasyUnlockService::NotifySmartLockAuthResult(bool success) {
  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked())
    return;

  proximity_auth::ScreenlockBridge::Get()
      ->lock_handler()
      ->NotifySmartLockAuthResult(GetAccountId(), success);
}

std::string EasyUnlockService::GetLastRemoteStatusUnlockForLogging() {
  if (proximity_auth_system_) {
    return proximity_auth_system_->GetLastRemoteStatusUnlockForLogging();
  }
  return std::string();
}

}  // namespace ash
