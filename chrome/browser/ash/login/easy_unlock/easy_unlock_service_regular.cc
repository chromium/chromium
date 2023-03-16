// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_regular.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/easy_unlock/chrome_proximity_auth_client.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_notification_controller.h"
#include "chrome/browser/ash/login/easy_unlock/smartlock_feature_usage_metrics.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

namespace {

enum class SmartLockToggleFeature { DISABLE = false, ENABLE = true };

// The result of a SmartLock operation.
enum class SmartLockResult { FAILURE = false, SUCCESS = true };

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

}  // namespace

EasyUnlockServiceRegular::EasyUnlockServiceRegular(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : EasyUnlockServiceRegular(
          profile,
          secure_channel_client,
          std::make_unique<EasyUnlockNotificationController>(profile),
          device_sync_client,
          multidevice_setup_client) {}

EasyUnlockServiceRegular::EasyUnlockServiceRegular(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<EasyUnlockNotificationController> notification_controller,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : EasyUnlockService(profile, secure_channel_client),
      lock_screen_last_shown_timestamp_(base::TimeTicks::Now()),
      notification_controller_(std::move(notification_controller)),
      device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client) {}

EasyUnlockServiceRegular::~EasyUnlockServiceRegular() = default;

// TODO(jhawkins): This method with `has_unlock_keys` == true is the only signal
// that SmartLock setup has completed successfully. Make this signal more
// explicit.
void EasyUnlockServiceRegular::LoadRemoteDevices() {
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
                            absl::nullopt /* local_device */);
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
                            absl::nullopt /* local_device */);

    if (pref_manager_->IsEasyUnlockEnabledStateSet()) {
      LogSmartLockEnabledState(SmartLockEnabledState::DISABLED);
    } else {
      LogSmartLockEnabledState(SmartLockEnabledState::UNSET);
    }
    return;
  }

  // This code path may be hit by:
  //   1. New devices were synced on the lock screen.
  //   2. The service was initialized while the login screen is still up.
  if (proximity_auth::ScreenlockBridge::Get()->IsLocked()) {
    PA_LOG(VERBOSE) << "Deferring device load until screen is unlocked.";
    deferring_device_load_ = true;
    return;
  }

  UseLoadedRemoteDevices(GetUnlockKeys());
}

void EasyUnlockServiceRegular::UseLoadedRemoteDevices(
    const multidevice::RemoteDeviceRefList& remote_devices) {
  // When EasyUnlock is enabled, only one EasyUnlock host should exist.
  if (remote_devices.size() != 1u) {
    PA_LOG(ERROR) << "There should only be 1 Smart Lock host, but there are: "
                  << remote_devices.size();
    SetProximityAuthDevices(GetAccountId(), multidevice::RemoteDeviceRefList(),
                            absl::nullopt);
    NOTREACHED();
    return;
  }

  absl::optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();
  if (!local_device) {
    PA_LOG(ERROR) << "EasyUnlockServiceRegular::" << __func__
                  << ": Local device unexpectedly null.";
    SetProximityAuthDevices(GetAccountId(), multidevice::RemoteDeviceRefList(),
                            absl::nullopt);
    return;
  }

  SetProximityAuthDevices(GetAccountId(), remote_devices, local_device);
}

proximity_auth::ProximityAuthPrefManager*
EasyUnlockServiceRegular::GetProximityAuthPrefManager() {
  return pref_manager_.get();
}

AccountId EasyUnlockServiceRegular::GetAccountId() const {
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  return primary_user->GetAccountId();
}

void EasyUnlockServiceRegular::InitializeInternal() {
  pref_manager_ =
      std::make_unique<proximity_auth::ProximityAuthProfilePrefManager>(
          profile()->GetPrefs(), multidevice_setup_client_);

  // If `device_sync_client_` is not ready yet, wait for it to call back on
  // OnReady().
  if (device_sync_client_->is_ready())
    OnReady();
  device_sync_client_->AddObserver(this);

  OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());
  multidevice_setup_client_->AddObserver(this);
  StartFeatureUsageMetrics();

  LoadRemoteDevices();
}

void EasyUnlockServiceRegular::ShutdownInternal() {
  pref_manager_.reset();
  notification_controller_.reset();

  device_sync_client_->RemoveObserver(this);

  multidevice_setup_client_->RemoveObserver(this);

  StopFeatureUsageMetrics();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool EasyUnlockServiceRegular::IsAllowedInternal() const {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsLoggedInAsUserWithGaiaAccount())
    return false;

  // TODO(tengs): Ephemeral accounts generate a new enrollment every time they
  // are added, so disable Smart Lock to reduce enrollments on server. However,
  // ephemeral accounts can be locked, so we should revisit this use case.
  if (user_manager->IsCurrentUserNonCryptohomeDataEphemeral())
    return false;

  if (!ProfileHelper::IsPrimaryProfile(profile()))
    return false;

  if (multidevice_setup_client_->GetFeatureState(
          multidevice_setup::mojom::Feature::kSmartLock) ==
      multidevice_setup::mojom::FeatureState::kProhibitedByPolicy) {
    return false;
  }

  return true;
}

bool EasyUnlockServiceRegular::IsEnabled() const {
  return multidevice_setup_client_->GetFeatureState(
             multidevice_setup::mojom::Feature::kSmartLock) ==
         multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

bool EasyUnlockServiceRegular::IsChromeOSLoginEnabled() const {
  return pref_manager_ && pref_manager_->IsChromeOSLoginEnabled();
}

void EasyUnlockServiceRegular::OnSuspendDoneInternal() {
  lock_screen_last_shown_timestamp_ = base::TimeTicks::Now();
}

void EasyUnlockServiceRegular::OnReady() {
  // If the local device and synced devices are ready for the first time,
  // establish what the unlock keys were before the next sync. This is necessary
  // in order for OnNewDevicesSynced() to determine if new devices were added
  // since the last sync.
  remote_device_unlock_keys_before_sync_ = GetUnlockKeys();
}

void EasyUnlockServiceRegular::OnEnrollmentFinished() {
  // The local device may be ready for the first time, or it may have been
  // updated, so reload devices.
  LoadRemoteDevices();
}

void EasyUnlockServiceRegular::OnNewDevicesSynced() {
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

void EasyUnlockServiceRegular::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  LoadRemoteDevices();
  UpdateAppState();
}

void EasyUnlockServiceRegular::ShowChromebookAddedNotification() {
  // The user may have decided to disable Smart Lock or the whole multidevice
  // suite immediately after completing setup, so ensure that Smart Lock is
  // enabled.
  if (IsEnabled())
    notification_controller_->ShowChromebookAddedNotification();
}

void EasyUnlockServiceRegular::ShowNotificationIfNewDevicePresent(
    const std::set<std::string>& public_keys_before_sync,
    const std::set<std::string>& public_keys_after_sync) {
  if (public_keys_before_sync == public_keys_after_sync)
    return;

  // Show the appropriate notification if an unlock key is first synced or if it
  // changes an existing key.
  // Note: We do not show a notification when EasyUnlock is disabled by sync nor
  // if EasyUnlock was enabled through the setup app.
  if (!public_keys_after_sync.empty()) {
    if (public_keys_before_sync.empty()) {
      multidevice_setup::MultiDeviceSetupDialog* multidevice_setup_dialog =
          multidevice_setup::MultiDeviceSetupDialog::Get();
      if (multidevice_setup_dialog) {
        // Delay showing the "Chromebook added" notification until the
        // MultiDeviceSetupDialog is closed.
        multidevice_setup_dialog->AddOnCloseCallback(base::BindOnce(
            &EasyUnlockServiceRegular::ShowChromebookAddedNotification,
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

void EasyUnlockServiceRegular::StartFeatureUsageMetrics() {
  feature_usage_metrics_ =
      std::make_unique<SmartLockFeatureUsageMetrics>(multidevice_setup_client_);

  SmartLockMetricsRecorder::SetUsageRecorderInstance(
      feature_usage_metrics_.get());
}

void EasyUnlockServiceRegular::StopFeatureUsageMetrics() {
  feature_usage_metrics_.reset();
  SmartLockMetricsRecorder::SetUsageRecorderInstance(nullptr);
}

void EasyUnlockServiceRegular::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  EasyUnlockService::OnScreenDidLock(screen_type);

  set_will_authenticate_using_easy_unlock(false);
  lock_screen_last_shown_timestamp_ = base::TimeTicks::Now();
}

void EasyUnlockServiceRegular::OnScreenDidUnlock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  // If we tried to load remote devices (e.g. after a sync or the
  // service was initialized) while the screen was locked, we can now
  // load the new remote devices.
  //
  // It's important to go through this code path even if unlocking the
  // login screen. Because when the service is initialized while the
  // user is signing in we need to load the remotes. Otherwise, the
  // first time the user locks the screen the feature won't work.
  if (deferring_device_load_) {
    PA_LOG(VERBOSE) << "Loading deferred devices after screen unlock.";
    deferring_device_load_ = false;
    LoadRemoteDevices();
  }

  // Do not process events for the login screen.
  if (screen_type != proximity_auth::ScreenlockBridge::LockHandler::LOCK_SCREEN)
    return;

  if (shown_pairing_changed_notification_) {
    shown_pairing_changed_notification_ = false;

    if (!GetUnlockKeys().empty()) {
      notification_controller_->ShowPairingChangeAppliedNotification(
          GetUnlockKeys()[0].name());
    }
  }

  // Only record metrics for users who have enabled the feature.
  if (IsEnabled()) {
    EasyUnlockAuthEvent event = will_authenticate_using_easy_unlock()
                                    ? EASY_UNLOCK_SUCCESS
                                    : GetPasswordAuthEvent();
    RecordEasyUnlockScreenUnlockEvent(event);

    if (will_authenticate_using_easy_unlock()) {
      // TODO(crbug.com/1171972): Deprecate the AuthMethodChoice metric.
      SmartLockMetricsRecorder::RecordSmartLockUnlockAuthMethodChoice(
          SmartLockMetricsRecorder::SmartLockAuthMethodChoice::kSmartLock);
      SmartLockMetricsRecorder::RecordAuthResultUnlockSuccess();
      RecordEasyUnlockScreenUnlockDuration(base::TimeTicks::Now() -
                                           lock_screen_last_shown_timestamp_);
    } else {
      SmartLockMetricsRecorder::RecordAuthMethodChoiceUnlockPasswordState(
          GetSmartUnlockPasswordAuthEvent());
      // TODO(crbug.com/1171972): Deprecate the AuthMethodChoice metric.
      SmartLockMetricsRecorder::RecordSmartLockUnlockAuthMethodChoice(
          SmartLockMetricsRecorder::SmartLockAuthMethodChoice::kOther);
      OnUserEnteredPassword();
    }
  }

  set_will_authenticate_using_easy_unlock(false);
}

void EasyUnlockServiceRegular::OnFocusedUserChanged(
    const AccountId& account_id) {
  // Nothing to do.
}

multidevice::RemoteDeviceRefList EasyUnlockServiceRegular::GetUnlockKeys() {
  multidevice::RemoteDeviceRefList unlock_keys;
  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    bool unlock_key = remote_device.GetSoftwareFeatureState(
                          multidevice::SoftwareFeature::kSmartLockHost) ==
                      multidevice::SoftwareFeatureState::kEnabled;
    if (unlock_key)
      unlock_keys.push_back(remote_device);
  }
  return unlock_keys;
}

}  // namespace ash
