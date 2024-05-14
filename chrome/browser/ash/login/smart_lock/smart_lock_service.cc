// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/smart_lock/smart_lock_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/smartlock_state.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/smart_lock/chrome_proximity_auth_client.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_feature_usage_metrics.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_notification_controller.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

enum class SmartLockEnabledState {
  ENABLED = 0,
  DISABLED = 1,
  UNSET = 2,
  COUNT
};

void LogSmartLockEnabledState(SmartLockEnabledState state) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.EnabledState", state,
                            SmartLockEnabledState::COUNT);
}

void SetAuthTypeIfChanged(
    proximity_auth::ScreenlockBridge::LockHandler* lock_handler,
    const AccountId& account_id,
    proximity_auth::mojom::AuthType auth_type,
    const std::u16string& auth_value) {
  CHECK(lock_handler);
  const proximity_auth::mojom::AuthType existing_auth_type =
      lock_handler->GetAuthType(account_id);
  if (auth_type == existing_auth_type) {
    return;
  }

  lock_handler->SetAuthType(account_id, auth_type, auth_value);
}

}  // namespace

// static
SmartLockService* SmartLockService::Get(Profile* profile) {
  return SmartLockServiceFactory::GetForBrowserContext(profile);
}

// static
SmartLockService* SmartLockService::GetForUser(const user_manager::User& user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(&user);
  if (!profile) {
    return nullptr;
  }
  return SmartLockService::Get(profile);
}

// static
void SmartLockService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kEasyUnlockPairing);
  proximity_auth::ProximityAuthProfilePrefManager::RegisterPrefs(registry);
}

class SmartLockService::PowerMonitor
    : public chromeos::PowerManagerClient::Observer {
 public:
  explicit PowerMonitor(SmartLockService* service) : service_(service) {
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

  raw_ptr<SmartLockService> service_;
  base::WeakPtrFactory<PowerMonitor> weak_ptr_factory_{this};
};

SmartLockService::SmartLockService(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : SmartLockService(
          profile,
          secure_channel_client,
          std::make_unique<SmartLockNotificationController>(profile),
          device_sync_client,
          multidevice_setup_client) {}

SmartLockService::SmartLockService(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<SmartLockNotificationController> notification_controller,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : profile_(profile),
      secure_channel_client_(secure_channel_client),
      proximity_auth_client_(profile),
      shut_down_(false),
      lock_screen_last_shown_timestamp_(base::TimeTicks::Now()),
      notification_controller_(std::move(notification_controller)),
      device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client) {}

SmartLockService::~SmartLockService() = default;

bool SmartLockService::AttemptAuth(const AccountId& account_id) {
  PA_LOG(VERBOSE) << "User began unlock auth attempt.";

  if (auth_attempt_) {
    PA_LOG(VERBOSE) << "Already attempting auth, skipping this request.";
    return false;
  }

  if (!GetAccountId().is_valid()) {
    PA_LOG(ERROR) << "Empty user account. Auth attempt failed.";
    RecordAuthResult(SmartLockMetricsRecorder::
                         SmartLockAuthResultFailureReason::kEmptyUserAccount);
    return false;
  }

  if (GetAccountId() != account_id) {
    RecordAuthResult(SmartLockMetricsRecorder::
                         SmartLockAuthResultFailureReason::kInvalidAccoundId);

    PA_LOG(ERROR) << "Check failed: " << GetAccountId().Serialize() << " vs "
                  << account_id.Serialize();
    return false;
  }

  auth_attempt_ = std::make_unique<SmartLockAuthAttempt>(account_id);
  if (!auth_attempt_->Start()) {
    RecordAuthResult(
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

void SmartLockService::FinalizeUnlock(bool success) {
  if (!auth_attempt_) {
    return;
  }

  set_will_authenticate_using_smart_lock(true);
  auth_attempt_->FinalizeUnlock(GetAccountId(), success);

  // If successful, allow |auth_attempt_| to continue until
  // UpdateSmartLockState() is called (indicating screen unlock).

  // Make sure that the lock screen is updated on failure.
  if (!success) {
    auth_attempt_.reset();
    RecordSmartLockScreenUnlockEvent(SMART_LOCK_FAILURE);
  }

  NotifySmartLockAuthResult(success);
}

AccountId SmartLockService::GetAccountId() const {
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(primary_user);
  return primary_user->GetAccountId();
}

SmartLockState SmartLockService::GetInitialSmartLockState() const {
  if (IsAllowed() && IsEnabled() && proximity_auth_system_ != nullptr) {
    return SmartLockState::kConnectingToPhone;
  }

  return SmartLockState::kDisabled;
}

std::string SmartLockService::GetLastRemoteStatusUnlockForLogging() {
  if (proximity_auth_system_) {
    return proximity_auth_system_->GetLastRemoteStatusUnlockForLogging();
  }
  return std::string();
}

const multidevice::RemoteDeviceRefList
SmartLockService::GetRemoteDevicesForTesting() const {
  if (!proximity_auth_system_) {
    return multidevice::RemoteDeviceRefList();
  }

  return proximity_auth_system_->GetRemoteDevicesForUser(GetAccountId());
}

void SmartLockService::Initialize() {
  proximity_auth::ScreenlockBridge::Get()->AddObserver(this);

  pref_manager_ =
      std::make_unique<proximity_auth::ProximityAuthProfilePrefManager>(
          profile()->GetPrefs(), multidevice_setup_client_);

  // If `device_sync_client_` is not ready yet, wait for it to call back on
  // OnReady().
  if (device_sync_client_->is_ready()) {
    OnReady();
  }
  device_sync_client_->AddObserver(this);

  OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());
  multidevice_setup_client_->AddObserver(this);
  StartFeatureUsageMetrics();

  LoadRemoteDevices();
}

bool SmartLockService::IsAllowed() const {
  if (shut_down_) {
    return false;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  if (!user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    return false;
  }

  // TODO(tengs): Ephemeral accounts generate a new enrollment every time they
  // are added, so disable Smart Lock to reduce enrollments on server. However,
  // ephemeral accounts can be locked, so we should revisit this use case.
  if (user_manager->IsCurrentUserNonCryptohomeDataEphemeral()) {
    return false;
  }

  if (!ProfileHelper::IsPrimaryProfile(profile())) {
    return false;
  }

  if (multidevice_setup_client_->GetFeatureState(
          multidevice_setup::mojom::Feature::kSmartLock) ==
      multidevice_setup::mojom::FeatureState::kProhibitedByPolicy) {
    return false;
  }

  return true;
}

bool SmartLockService::IsEnabled() const {
  return multidevice_setup_client_->GetFeatureState(
             multidevice_setup::mojom::Feature::kSmartLock) ==
         multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

void SmartLockService::UpdateSmartLockState(SmartLockState state) {
  if (smart_lock_state_ && state == smart_lock_state_.value()) {
    return;
  }

  smart_lock_state_ = state;

  if (proximity_auth::ScreenlockBridge::Get()->IsLocked()) {
    auto* lock_handler =
        proximity_auth::ScreenlockBridge::Get()->lock_handler();
    CHECK(lock_handler);

    lock_handler->SetSmartLockState(GetAccountId(), state);

    // TODO(https://crbug.com/1233614): Eventually we would like to remove
    // auth_type.mojom where AuthType lives, but this will require further
    // investigation. This logic was copied from legacy
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
        SetAuthTypeIfChanged(lock_handler, GetAccountId(),
                             proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
                             std::u16string());
      }
    }
  }

  if (state != SmartLockState::kPhoneAuthenticated && auth_attempt_) {
    // Clean up existing auth attempt if we can no longer authenticate the
    // remote device.
    auth_attempt_.reset();

    if (!IsSmartLockStateValidOnRemoteAuthFailure()) {
      NotifySmartLockAuthResult(/*success=*/false);
    }
  }
}

void SmartLockService::Shutdown() {
  if (shut_down_) {
    return;
  }
  shut_down_ = true;

  pref_manager_.reset();
  notification_controller_.reset();

  device_sync_client_->RemoveObserver(this);

  multidevice_setup_client_->RemoveObserver(this);

  StopFeatureUsageMetrics();

  weak_ptr_factory_.InvalidateWeakPtrs();

  proximity_auth::ScreenlockBridge::Get()->RemoveObserver(this);

  ResetSmartLockState();
  proximity_auth_system_.reset();
  power_monitor_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SmartLockService::OnScreenDidLock() {
  ShowInitialSmartLockState();

  set_will_authenticate_using_smart_lock(false);
  lock_screen_last_shown_timestamp_ = base::TimeTicks::Now();
}

void SmartLockService::OnScreenDidUnlock() {
  // If we tried to load remote devices (e.g. after a sync or the
  // service was initialized) while the screen was locked, we can now
  // load the new remote devices.
  if (deferring_device_load_) {
    PA_LOG(VERBOSE) << "Loading deferred devices after screen unlock.";
    deferring_device_load_ = false;
    LoadRemoteDevices();
  }

  if (shown_pairing_changed_notification_) {
    shown_pairing_changed_notification_ = false;

    if (!GetUnlockKeys().empty()) {
      notification_controller_->ShowPairingChangeAppliedNotification(
          GetUnlockKeys()[0].name());
    }
  }

  // Only record metrics for users who have enabled the feature.
  if (IsEnabled()) {
    SmartLockAuthEvent event = will_authenticate_using_smart_lock()
                                   ? SMART_LOCK_SUCCESS
                                   : GetPasswordAuthEvent();
    RecordSmartLockScreenUnlockEvent(event);

    if (will_authenticate_using_smart_lock()) {
      SmartLockMetricsRecorder::RecordSmartLockUnlockAuthMethodChoice(
          SmartLockMetricsRecorder::SmartLockAuthMethodChoice::kSmartLock);
      RecordAuthResult(/*failure_reason=*/std::nullopt);
      RecordSmartLockScreenUnlockDuration(base::TimeTicks::Now() -
                                          lock_screen_last_shown_timestamp_);
    } else {
      SmartLockMetricsRecorder::RecordSmartLockUnlockAuthMethodChoice(
          SmartLockMetricsRecorder::SmartLockAuthMethodChoice::kOther);
      SmartLockMetricsRecorder::RecordAuthMethodChoiceUnlockPasswordState(
          GetSmartUnlockPasswordAuthEvent());
      OnUserEnteredPassword();
    }
  }

  set_will_authenticate_using_smart_lock(false);
}

void SmartLockService::OnReady() {
  // If the local device and synced devices are ready for the first time,
  // establish what the unlock keys were before the next sync. This is necessary
  // in order for OnNewDevicesSynced() to determine if new devices were added
  // since the last sync.
  remote_device_unlock_keys_before_sync_ = GetUnlockKeys();
}

void SmartLockService::OnEnrollmentFinished() {
  // The local device may be ready for the first time, or it may have been
  // updated, so reload devices.
  LoadRemoteDevices();
}

void SmartLockService::OnNewDevicesSynced() {
  std::set<std::string> public_keys_before_sync;
  for (const auto& remote_device : remote_device_unlock_keys_before_sync_) {
    public_keys_before_sync.insert(remote_device.public_key());
  }

  multidevice::RemoteDeviceRefList remote_device_unlock_keys_after_sync =
      GetUnlockKeys();
  std::set<std::string> public_keys_after_sync;
  for (const auto& remote_device : remote_device_unlock_keys_after_sync) {
    public_keys_after_sync.insert(remote_device.public_key());
  }

  ShowNotificationIfNewDevicePresent(public_keys_before_sync,
                                     public_keys_after_sync);

  LoadRemoteDevices();

  remote_device_unlock_keys_before_sync_ = remote_device_unlock_keys_after_sync;
}

void SmartLockService::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  LoadRemoteDevices();
  UpdateAppState();
}

SmartLockAuthEvent SmartLockService::GetPasswordAuthEvent() const {
  CHECK(IsEnabled());

  if (!smart_lock_state_) {
    return PASSWORD_ENTRY_NO_SMARTLOCK_STATE_HANDLER;
  }

  SmartLockState state = smart_lock_state_.value();

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
    case SmartLockState::kPrimaryUserAbsent:
      return PASSWORD_ENTRY_PRIMARY_USER_ABSENT;
  }

  NOTREACHED_IN_MIGRATION();
  return SMART_LOCK_AUTH_EVENT_COUNT;
}

SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
SmartLockService::GetSmartUnlockPasswordAuthEvent() const {
  CHECK(IsEnabled());

  if (!smart_lock_state_) {
    return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
        kUnknownState;
  }

  SmartLockState state = smart_lock_state_.value();

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
    case SmartLockState::kPhoneNotLockable:
      return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
          kPhoneNotLockable;
    case SmartLockState::kPrimaryUserAbsent:
      return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
          kPrimaryUserAbsent;
    default:
      NOTREACHED_IN_MIGRATION();
      return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
          kUnknownState;
  }

  NOTREACHED_IN_MIGRATION();
  return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
      kUnknownState;
}

multidevice::RemoteDeviceRefList SmartLockService::GetUnlockKeys() {
  multidevice::RemoteDeviceRefList unlock_keys;
  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    bool unlock_key = remote_device.GetSoftwareFeatureState(
                          multidevice::SoftwareFeature::kSmartLockHost) ==
                      multidevice::SoftwareFeatureState::kEnabled;
    if (unlock_key) {
      unlock_keys.push_back(remote_device);
    }
  }
  return unlock_keys;
}

bool SmartLockService::IsSmartLockStateValidOnRemoteAuthFailure() const {
  // Note that NO_PHONE is not valid in this case because the phone may close
  // the connection if the auth challenge sent to it is invalid. This case
  // should be handled as authentication failure.
  return smart_lock_state_ == SmartLockState::kInactive ||
         smart_lock_state_ == SmartLockState::kDisabled ||
         smart_lock_state_ == SmartLockState::kBluetoothDisabled ||
         smart_lock_state_ == SmartLockState::kPhoneFoundLockedAndProximate;
}

// TODO(jhawkins): This method with `has_unlock_keys` == true is the only signal
// that SmartLock setup has completed successfully. Make this signal more
// explicit.
void SmartLockService::LoadRemoteDevices() {
  if (!device_sync_client_->is_ready()) {
    // OnEnrollmentFinished() or OnNewDevicesSynced() will call back on this
    // method once `device_sync_client_` is ready.
    PA_LOG(VERBOSE) << "DeviceSyncClient is not ready yet, delaying "
                       "UseLoadedRemoteDevices().";
    return;
  }

  if (!IsEnabled()) {
    // OnFeatureStatesChanged() will call back on this method when feature state
    // changes.
    PA_LOG(VERBOSE) << "Smart Lock is not enabled by user; aborting.";
    SetProximityAuthDevices(GetAccountId(), multidevice::RemoteDeviceRefList(),
                            std::nullopt /* local_device */);
    return;
  }

  bool has_unlock_keys = !GetUnlockKeys().empty();

  // TODO(jhawkins): The enabled pref should not be tied to whether unlock keys
  // exist; instead, both of these variables should be used to determine
  // IsEnabled().
  pref_manager_->SetIsEasyUnlockEnabled(has_unlock_keys);
  if (has_unlock_keys) {
    // If `has_unlock_keys` is true, then the user must have successfully
    // completed setup. Track that the IsEasyUnlockEnabled pref is actively set
    // by the user, as opposed to passively being set to disabled (the default
    // state).
    pref_manager_->SetEasyUnlockEnabledStateSet();
    LogSmartLockEnabledState(SmartLockEnabledState::ENABLED);
  } else {
    PA_LOG(ERROR) << "Smart Lock is enabled by user, but no unlock key is "
                     "present; aborting.";
    SetProximityAuthDevices(GetAccountId(), multidevice::RemoteDeviceRefList(),
                            std::nullopt /* local_device */);

    if (pref_manager_->IsEasyUnlockEnabledStateSet()) {
      LogSmartLockEnabledState(SmartLockEnabledState::DISABLED);
    } else {
      LogSmartLockEnabledState(SmartLockEnabledState::UNSET);
    }
    return;
  }

  // This code path may be hit if new devices were synced on the lock screen.
  if (proximity_auth::ScreenlockBridge::Get()->IsLocked()) {
    PA_LOG(VERBOSE) << "Deferring device load until screen is unlocked.";
    deferring_device_load_ = true;
    return;
  }

  UseLoadedRemoteDevices(GetUnlockKeys());
}

void SmartLockService::NotifySmartLockAuthResult(bool success) {
  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked()) {
    return;
  }

  proximity_auth::ScreenlockBridge::Get()
      ->lock_handler()
      ->NotifySmartLockAuthResult(GetAccountId(), success);
}

void SmartLockService::OnSuspendDone() {
  if (proximity_auth_system_) {
    proximity_auth_system_->OnSuspendDone();
  }
}

void SmartLockService::OnUserEnteredPassword() {
  if (proximity_auth_system_) {
    proximity_auth_system_->CancelConnectionAttempt();
  }
}

void SmartLockService::PrepareForSuspend() {
  if (smart_lock_state_ && *smart_lock_state_ != SmartLockState::kInactive) {
    ShowInitialSmartLockState();
  }

  if (proximity_auth_system_) {
    proximity_auth_system_->OnSuspend();
  }
}

void SmartLockService::RecordAuthResult(
    std::optional<SmartLockMetricsRecorder::SmartLockAuthResultFailureReason>
        failure_reason) {
  if (failure_reason.has_value()) {
    SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(
        failure_reason.value());
    feature_usage_metrics_->RecordUsage(/*success=*/false);
  } else {
    SmartLockMetricsRecorder::RecordAuthResultUnlockSuccess();
    feature_usage_metrics_->RecordUsage(/*success=*/true);
  }
}

void SmartLockService::ResetSmartLockState() {
  smart_lock_state_.reset();
  auth_attempt_.reset();
}

void SmartLockService::SetProximityAuthDevices(
    const AccountId& account_id,
    const multidevice::RemoteDeviceRefList& remote_devices,
    std::optional<multidevice::RemoteDeviceRef> local_device) {
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
            proximity_auth_client(), secure_channel_client_);
  }

  proximity_auth_system_->SetRemoteDevicesForUser(account_id, remote_devices,
                                                  local_device);
  proximity_auth_system_->Start();
}

void SmartLockService::ShowChromebookAddedNotification() {
  // The user may have decided to disable Smart Lock or the whole multidevice
  // suite immediately after completing setup, so ensure that Smart Lock is
  // enabled.
  if (IsEnabled()) {
    notification_controller_->ShowChromebookAddedNotification();
  }
}

void SmartLockService::ShowInitialSmartLockState() {
  // Only proceed if the screen is locked to prevent the UI event from not
  // persisting within UpdateSmartLockState().
  //
  // Note: ScreenlockBridge::IsLocked() may return a false positive if the
  // system is "warming up" (for example, on screen lock or after suspend). To
  // work around this race, ShowInitialSmartLockState() is also called from
  // OnScreenDidLock() (which triggers when ScreenlockBridge::IsLocked() becomes
  // true) to ensure that an initial state is displayed in the UI.
  // TODO(b/227674947) Investigate whether a false positive is still possible
  // now that Sign in with Smart Lock is deprecated.
  auto* screenlock_bridge = proximity_auth::ScreenlockBridge::Get();
  if (screenlock_bridge && screenlock_bridge->IsLocked()) {
    UpdateSmartLockState(GetInitialSmartLockState());
  }
}

void SmartLockService::ShowNotificationIfNewDevicePresent(
    const std::set<std::string>& public_keys_before_sync,
    const std::set<std::string>& public_keys_after_sync) {
  if (public_keys_before_sync == public_keys_after_sync) {
    return;
  }

  // Show the appropriate notification if an unlock key is first synced or if it
  // changes an existing key.
  // Note: We do not show a notification when Smart lock is disabled by sync nor
  // if Smart Lock was enabled through the setup app.
  if (!public_keys_after_sync.empty()) {
    if (public_keys_before_sync.empty()) {
      multidevice_setup::MultiDeviceSetupDialog* multidevice_setup_dialog =
          multidevice_setup::MultiDeviceSetupDialog::Get();
      if (multidevice_setup_dialog) {
        // Delay showing the "Chromebook added" notification until the
        // MultiDeviceSetupDialog is closed.
        multidevice_setup_dialog->AddOnCloseCallback(
            base::BindOnce(&SmartLockService::ShowChromebookAddedNotification,
                           weak_ptr_factory_.GetWeakPtr()));
        return;
      }

      notification_controller_->ShowChromebookAddedNotification();
    } else {
      shown_pairing_changed_notification_ = true;
      notification_controller_->ShowPairingChangeNotification();
    }
  }
}

void SmartLockService::StartFeatureUsageMetrics() {
  feature_usage_metrics_ =
      std::make_unique<SmartLockFeatureUsageMetrics>(multidevice_setup_client_);
}

void SmartLockService::StopFeatureUsageMetrics() {
  feature_usage_metrics_.reset();
}

void SmartLockService::UpdateAppState() {
  if (IsAllowed()) {
    if (proximity_auth_system_) {
      proximity_auth_system_->Start();
    }

    if (!power_monitor_) {
      power_monitor_ = std::make_unique<PowerMonitor>(this);
    }
  }
}

void SmartLockService::UseLoadedRemoteDevices(
    const multidevice::RemoteDeviceRefList& remote_devices) {
  // When Smart Lock is enabled, only one Smart Lock host should exist.
  if (remote_devices.size() != 1u) {
    PA_LOG(ERROR) << "There should only be 1 Smart Lock host, but there are: "
                  << remote_devices.size();
    SetProximityAuthDevices(GetAccountId(), multidevice::RemoteDeviceRefList(),
                            std::nullopt);
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();
  if (!local_device) {
    PA_LOG(ERROR) << "SmartLockService::" << __func__
                  << ": Local device unexpectedly null.";
    SetProximityAuthDevices(GetAccountId(), multidevice::RemoteDeviceRefList(),
                            std::nullopt);
    return;
  }

  SetProximityAuthDevices(GetAccountId(), remote_devices, local_device);
}

}  // namespace ash
