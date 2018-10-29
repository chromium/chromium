// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_regular.h"

#include <stdint.h>

#include <utility>

#include "apps/app_lifetime_monitor_factory.h"
#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cryptauth/chrome_cryptauth_service_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/chrome_proximity_auth_client.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_names.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_reauth.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/common/apps/platform_apps/api/easy_unlock_private.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/components/proximity_auth/switches.h"
#include "components/cryptauth/cryptauth_client_impl.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/cryptauth_enrollment_utils.h"
#include "components/cryptauth/cryptauth_gcm_manager_impl.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/remote_device_loader.h"
#include "components/cryptauth/secure_message_delegate_impl.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace chromeos {

namespace {

// Key name of the local device permit record dictonary in kEasyUnlockPairing.
const char kKeyPermitAccess[] = "permitAccess";

// Key name of the remote device list in kEasyUnlockPairing.
const char kKeyDevices[] = "devices";

enum class SmartLockToggleFeature { DISABLE = false, ENABLE = true };

// The result of a SmartLock operation.
enum class SmartLockResult { FAILURE = false, SUCCESS = true };

enum class SmartLockEnabledState {
  ENABLED = 0,
  DISABLED = 1,
  UNSET = 2,
  COUNT
};

void LogToggleFeature(SmartLockToggleFeature toggle) {
  UMA_HISTOGRAM_BOOLEAN("SmartLock.ToggleFeature", static_cast<bool>(toggle));
}

void LogToggleFeatureDisableResult(SmartLockResult result) {
  UMA_HISTOGRAM_BOOLEAN("SmartLock.ToggleFeature.Disable.Result",
                        static_cast<bool>(result));
}

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
      turn_off_flow_status_(EasyUnlockService::IDLE),
      scoped_crypt_auth_device_manager_observer_(this),
      will_unlock_using_easy_unlock_(false),
      lock_screen_last_shown_timestamp_(base::TimeTicks::Now()),
      deferring_device_load_(false),
      notification_controller_(std::move(notification_controller)),
      device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client),
      shown_pairing_changed_notification_(false),
      weak_ptr_factory_(this) {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    // If |device_sync_client_| is not ready yet, wait for it to call back on
    // OnReady().
    if (device_sync_client_->is_ready())
      OnReady();

    device_sync_client_->AddObserver(this);

    if (base::FeatureList::IsEnabled(
            chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
      OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());

      multidevice_setup_client_->AddObserver(this);
    }
  }
}

EasyUnlockServiceRegular::~EasyUnlockServiceRegular() = default;

// TODO(jhawkins): This method with |has_unlock_keys| == true is the only signal
// that SmartLock setup has completed successfully. Make this signal more
// explicit.
void EasyUnlockServiceRegular::LoadRemoteDevices() {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
      !device_sync_client_->is_ready()) {
    // OnEnrollmentFinished() or OnNewDevicesSynced() will call back on this
    // method once |device_sync_client_| is ready.
    PA_LOG(INFO) << "DeviceSyncClient is not ready yet, delaying "
                    "UseLoadedRemoteDevices().";
    return;
  }

  // TODO(crbug.com/894585): Remove this legacy special case after M71.
  bool is_in_legacy_host_mode = IsInLegacyHostMode();
  pref_manager_->SetIsInLegacyHostMode(is_in_legacy_host_mode);

  bool is_in_valid_legacy_host_state =
      is_in_legacy_host_mode &&
      feature_state_ ==
          multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost;
  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup) &&
      feature_state_ !=
          multidevice_setup::mojom::FeatureState::kEnabledByUser &&
      !is_in_valid_legacy_host_state) {
    // OnFeatureStatesChanged() will call back on this method when feature state
    // changes.
    PA_LOG(INFO) << "Smart Lock is disabled; aborting.";
    SetProximityAuthDevices(GetAccountId(), cryptauth::RemoteDeviceRefList(),
                            base::nullopt /* local_device */);
    return;
  }

  bool has_unlock_keys;
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    has_unlock_keys = !GetUnlockKeys().empty();
  } else {
    has_unlock_keys = !GetCryptAuthDeviceManager()->GetUnlockKeys().empty();
  }

  // TODO(jhawkins): The enabled pref should not be tied to whether unlock keys
  // exist; instead, both of these variables should be used to determine
  // IsEnabled().
  pref_manager_->SetIsEasyUnlockEnabled(has_unlock_keys);
  if (has_unlock_keys) {
    // If |has_unlock_keys| is true, then the user must have successfully
    // completed setup. Track that the IsEasyUnlockEnabled pref is actively set
    // by the user, as opposed to passively being set to disabled (the default
    // state).
    pref_manager_->SetEasyUnlockEnabledStateSet();
    LogSmartLockEnabledState(SmartLockEnabledState::ENABLED);
  } else {
    SetProximityAuthDevices(GetAccountId(), cryptauth::RemoteDeviceRefList(),
                            base::nullopt /* local_device */);

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
    PA_LOG(INFO) << "Deferring device load until screen is unlocked.";
    deferring_device_load_ = true;
    return;
  }

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    UseLoadedRemoteDevices(GetUnlockKeys());
  } else {
    remote_device_loader_.reset(new cryptauth::RemoteDeviceLoader(
        GetCryptAuthDeviceManager()->GetUnlockKeys(),
        proximity_auth_client()->GetAccountId(),
        GetCryptAuthEnrollmentManager()->GetUserPrivateKey(),
        cryptauth::SecureMessageDelegateImpl::Factory::NewInstance()));

    // OnRemoteDevicesLoaded() will call on UseLoadedRemoteDevices() with the
    // devices it receives.
    remote_device_loader_->Load(
        base::Bind(&EasyUnlockServiceRegular::OnRemoteDevicesLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void EasyUnlockServiceRegular::OnRemoteDevicesLoaded(
    const cryptauth::RemoteDeviceList& remote_devices) {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  cryptauth::RemoteDeviceRefList remote_device_refs;
  for (auto& remote_device : remote_devices) {
    remote_device_refs.push_back(cryptauth::RemoteDeviceRef(
        std::make_shared<cryptauth::RemoteDevice>(remote_device)));
  }

  UseLoadedRemoteDevices(remote_device_refs);
}

void EasyUnlockServiceRegular::UseLoadedRemoteDevices(
    const cryptauth::RemoteDeviceRefList& remote_devices) {
  // When EasyUnlock is enabled, only one EasyUnlock host should exist.
  DCHECK(remote_devices.size() == 1u);

  SetProximityAuthDevices(
      GetAccountId(), remote_devices,
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
          ? device_sync_client_->GetLocalDeviceMetadata()
          : base::nullopt);

  // We need to store a copy of |local_and_remote_devices| in the TPM, so it can
  // be retrieved on the sign-in screen when a user session has not been started
  // yet.
  // If |chromeos::features::kMultiDeviceApi| is enabled, it will expect a final
  // size of 2 (the one remote device, and the local device).
  // If |chromeos::features::kMultiDeviceApi| is disabled, it will expect a
  // final size of 1 (just the remote device).
  // TODO(crbug.com/856380): For historical reasons, the local and remote device
  // are persisted together in a list. This is awkward and hacky; they should
  // be persisted in a dictionary.
  cryptauth::RemoteDeviceRefList local_and_remote_devices;
  local_and_remote_devices.push_back(remote_devices[0]);
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    local_and_remote_devices.push_back(
        *device_sync_client_->GetLocalDeviceMetadata());
  }

  std::unique_ptr<base::ListValue> device_list(new base::ListValue());
  for (const auto& device : local_and_remote_devices) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    std::string b64_public_key, b64_psk;
    base::Base64UrlEncode(device.public_key(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &b64_public_key);
    base::Base64UrlEncode(device.persistent_symmetric_key(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &b64_psk);

    dict->SetString(key_names::kKeyPsk, b64_psk);

    // TODO(jhawkins): Remove the bluetoothAddress field from this proto.
    dict->SetString(key_names::kKeyBluetoothAddress, std::string());

    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      dict->SetString(
          key_names::kKeyPermitPermitId,
          base::StringPrintf(
              key_names::kPermitPermitIdFormat,
              gaia::CanonicalizeEmail(GetAccountId().GetUserEmail()).c_str()));
    } else {
      dict->SetString(
          key_names::kKeyPermitPermitId,
          base::StringPrintf(key_names::kPermitPermitIdFormat,
                             proximity_auth_client()->GetAccountId().c_str()));
    }

    dict->SetString(key_names::kKeyPermitId, b64_public_key);
    dict->SetString(key_names::kKeyPermitType, key_names::kPermitTypeLicence);
    dict->SetString(key_names::kKeyPermitData, b64_public_key);

    std::unique_ptr<base::ListValue> beacon_seed_list(new base::ListValue());
    for (const auto& beacon_seed : device.beacon_seeds()) {
      std::string b64_beacon_seed;
      base::Base64UrlEncode(beacon_seed.SerializeAsString(),
                            base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                            &b64_beacon_seed);
      beacon_seed_list->AppendString(b64_beacon_seed);
    }

    std::string serialized_beacon_seeds;
    JSONStringValueSerializer serializer(&serialized_beacon_seeds);
    serializer.Serialize(*beacon_seed_list);
    dict->SetString(key_names::kKeySerializedBeaconSeeds,
                    serialized_beacon_seeds);

    // This differentiates the local device from the remote device.
    bool unlock_key = device.GetSoftwareFeatureState(
                          cryptauth::SoftwareFeature::EASY_UNLOCK_HOST) ==
                      cryptauth::SoftwareFeatureState::kEnabled;
    dict->SetBoolean(key_names::kKeyUnlockKey, unlock_key);

    device_list->Append(std::move(dict));
  }

  // TODO(tengs): Rename this function after the easy_unlock app is replaced.
  SetRemoteDevices(*device_list);
}

proximity_auth::ProximityAuthPrefManager*
EasyUnlockServiceRegular::GetProximityAuthPrefManager() {
  return pref_manager_.get();
}

EasyUnlockService::Type EasyUnlockServiceRegular::GetType() const {
  return EasyUnlockService::TYPE_REGULAR;
}

AccountId EasyUnlockServiceRegular::GetAccountId() const {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  // |profile| has to be a signed-in profile with IdentityManager already
  // created. Otherwise, just crash to collect stack.
  DCHECK(identity_manager);
  const AccountInfo account_info = identity_manager->GetPrimaryAccountInfo();
  // A regular signed-in (i.e., non-login) profile should always have an email.
  // TODO(crbug.com/857494): Enable this DCHECK once all browser tests create
  // correctly signed in profiles.
  // DCHECK(!account_info.email.empty());
  return account_info.email.empty()
             ? EmptyAccountId()
             : AccountId::FromUserEmailGaiaId(
                   gaia::CanonicalizeEmail(account_info.email),
                   account_info.gaia);
}

void EasyUnlockServiceRegular::LaunchSetup() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LogToggleFeature(SmartLockToggleFeature::ENABLE);

  // Force the user to reauthenticate by showing a modal overlay (similar to the
  // lock screen). The password obtained from the reauth is cached for a short
  // period of time and used to create the cryptohome keys for sign-in.
  if (short_lived_user_context_ && short_lived_user_context_->user_context()) {
    OpenSetupApp();
  } else {
    bool reauth_success = EasyUnlockReauth::ReauthForUserContext(
        base::Bind(&EasyUnlockServiceRegular::OpenSetupAppAfterReauth,
                   weak_ptr_factory_.GetWeakPtr()));
    if (!reauth_success)
      OpenSetupApp();
  }
}

void EasyUnlockServiceRegular::HandleUserReauth(
    const UserContext& user_context) {
  // Cache the user context for the next X minutes, so the user doesn't have to
  // reauth again.
  short_lived_user_context_.reset(new ShortLivedUserContext(
      user_context,
      apps::AppLifetimeMonitorFactory::GetForBrowserContext(profile()),
      base::ThreadTaskRunnerHandle::Get().get()));
}

void EasyUnlockServiceRegular::OpenSetupAppAfterReauth(
    const UserContext& user_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HandleUserReauth(user_context);

  OpenSetupApp();

  // Use this opportunity to clear the crytohome keys if it was not already
  // cleared earlier.
  const base::ListValue* devices = GetRemoteDevices();
  if (!devices || devices->empty()) {
    EasyUnlockKeyManager* key_manager =
        UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
    key_manager->RefreshKeys(
        user_context, base::ListValue(),
        base::Bind(&EasyUnlockServiceRegular::SetHardlockAfterKeyOperation,
                   weak_ptr_factory_.GetWeakPtr(),
                   EasyUnlockScreenlockStateHandler::NO_PAIRING));
  }
}

void EasyUnlockServiceRegular::SetHardlockAfterKeyOperation(
    EasyUnlockScreenlockStateHandler::HardlockState state_on_success,
    bool success) {
  if (success)
    SetHardlockStateForUser(GetAccountId(), state_on_success);

  // Even if the refresh keys operation suceeded, we still fetch and check the
  // cryptohome keys against the keys in local preferences as a sanity check.
  CheckCryptohomeKeysAndMaybeHardlock();
}

void EasyUnlockServiceRegular::ClearPermitAccess() {
  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  pairing_update->RemoveWithoutPathExpansion(kKeyPermitAccess, NULL);
}

const base::ListValue* EasyUnlockServiceRegular::GetRemoteDevices() const {
  const base::DictionaryValue* pairing_dict =
      profile()->GetPrefs()->GetDictionary(prefs::kEasyUnlockPairing);
  const base::ListValue* devices = NULL;
  if (pairing_dict && pairing_dict->GetList(kKeyDevices, &devices))
    return devices;
  return NULL;
}

void EasyUnlockServiceRegular::SetRemoteDevices(
    const base::ListValue& devices) {
  std::string remote_devices_json;
  JSONStringValueSerializer serializer(&remote_devices_json);
  serializer.Serialize(devices);
  PA_LOG(INFO) << "Setting RemoteDevices:\n  " << remote_devices_json;

  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  if (devices.empty())
    pairing_update->RemoveWithoutPathExpansion(kKeyDevices, NULL);
  else
    pairing_update->SetKey(kKeyDevices, devices.Clone());

  RefreshCryptohomeKeysIfPossible();
}

void EasyUnlockServiceRegular::RunTurnOffFlow() {
  if (turn_off_flow_status_ == PENDING)
    return;

  LogToggleFeature(SmartLockToggleFeature::DISABLE);

  SetTurnOffFlowStatus(PENDING);

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    // Disabling EASY_UNLOCK_HOST is a special case that does not require a
    // public key to be passed; EASY_UNLOCK_HOST will be disabled for all hosts.
    device_sync_client_->SetSoftwareFeatureState(
        std::string() /* public_key */,
        cryptauth::SoftwareFeature::EASY_UNLOCK_HOST, false /* enabled */,
        false /* is_exclusive */,
        base::BindOnce(&EasyUnlockServiceRegular::OnTurnOffEasyUnlockCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    DCHECK(!cryptauth_client_);
    std::unique_ptr<cryptauth::CryptAuthClientFactory> factory =
        proximity_auth_client()->CreateCryptAuthClientFactory();
    cryptauth_client_ = factory->CreateInstance();

    cryptauth::ToggleEasyUnlockRequest request;
    request.set_enable(false);
    request.set_apply_to_all(true);
    cryptauth_client_->ToggleEasyUnlock(
        request,
        base::Bind(&EasyUnlockServiceRegular::OnToggleEasyUnlockApiComplete,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&EasyUnlockServiceRegular::OnToggleEasyUnlockApiFailed,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void EasyUnlockServiceRegular::ResetTurnOffFlow() {
  cryptauth_client_.reset();
  SetTurnOffFlowStatus(IDLE);
}

EasyUnlockService::TurnOffFlowStatus
EasyUnlockServiceRegular::GetTurnOffFlowStatus() const {
  return turn_off_flow_status_;
}

std::string EasyUnlockServiceRegular::GetChallenge() const {
  return std::string();
}

std::string EasyUnlockServiceRegular::GetWrappedSecret() const {
  return std::string();
}

void EasyUnlockServiceRegular::RecordEasySignInOutcome(
    const AccountId& account_id,
    bool success) const {
  NOTREACHED();
}

void EasyUnlockServiceRegular::RecordPasswordLoginEvent(
    const AccountId& account_id) const {
  NOTREACHED();
}

void EasyUnlockServiceRegular::InitializeInternal() {
  proximity_auth::ScreenlockBridge::Get()->AddObserver(this);

  pref_manager_.reset(new proximity_auth::ProximityAuthProfilePrefManager(
      profile()->GetPrefs(), multidevice_setup_client_));

  // TODO(tengs): Due to badly configured browser_tests, Chrome crashes during
  // shutdown. Revisit this condition after migration is fully completed.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    // Note: There is no local state in tests.
    if (g_browser_process->local_state()) {
      pref_manager_->StartSyncingToLocalState(g_browser_process->local_state(),
                                              GetAccountId());
    }

    if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      scoped_crypt_auth_device_manager_observer_.Add(
          GetCryptAuthDeviceManager());
    }

    LoadRemoteDevices();
  }

  registrar_.Init(profile()->GetPrefs());
  registrar_.Add(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
      base::Bind(&EasyUnlockServiceRegular::RefreshCryptohomeKeysIfPossible,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockServiceRegular::ShutdownInternal() {
  short_lived_user_context_.reset();
  pref_manager_.reset();

  turn_off_flow_status_ = EasyUnlockService::IDLE;
  proximity_auth::ScreenlockBridge::Get()->RemoveObserver(this);

  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    scoped_crypt_auth_device_manager_observer_.RemoveAll();
  }

  registrar_.RemoveAll();

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    device_sync_client_->RemoveObserver(this);

    if (base::FeatureList::IsEnabled(
            chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
      multidevice_setup_client_->RemoveObserver(this);
    }
  }
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

  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup) &&
      feature_state_ ==
          multidevice_setup::mojom::FeatureState::kProhibitedByPolicy) {
    return false;
  }

  if (!profile()->GetPrefs()->GetBoolean(prefs::kEasyUnlockAllowed))
    return false;

  return true;
}

bool EasyUnlockServiceRegular::IsEnabled() const {
  // TODO(crbug.com/894585): Remove the legacy special case after M71.
  if (base::FeatureList::IsEnabled(
          chromeos::features::kEnableUnifiedMultiDeviceSetup) &&
      !IsInLegacyHostMode()) {
    return feature_state_ ==
           multidevice_setup::mojom::FeatureState::kEnabledByUser;
  }

  return pref_manager_ && pref_manager_->IsEasyUnlockEnabled();
}

bool EasyUnlockServiceRegular::IsChromeOSLoginEnabled() const {
  return pref_manager_ && pref_manager_->IsChromeOSLoginEnabled();
}

bool EasyUnlockServiceRegular::IsInLegacyHostMode() const {
  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
    return false;

  if (!device_sync_client_->is_ready()) {
    PA_LOG(WARNING) << "EasyUnlockServiceRegular::IsInLegacyHostMode: "
                    << "DeviceSyncClient not ready. Returning false.";
    return false;
  }

  bool has_supported_easy_unlock_host = false;
  for (const cryptauth::RemoteDeviceRef& remote_device_ref :
       device_sync_client_->GetSyncedDevices()) {
    cryptauth::SoftwareFeatureState better_together_host_state =
        remote_device_ref.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST);
    // If there's any valid Better Together host, don't support legacy mode.
    if (better_together_host_state ==
            cryptauth::SoftwareFeatureState::kSupported ||
        better_together_host_state ==
            cryptauth::SoftwareFeatureState::kEnabled) {
      return false;
    }

    cryptauth::SoftwareFeatureState easy_unlock_host_state =
        remote_device_ref.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::EASY_UNLOCK_HOST);
    if (easy_unlock_host_state == cryptauth::SoftwareFeatureState::kSupported ||
        easy_unlock_host_state == cryptauth::SoftwareFeatureState::kEnabled) {
      has_supported_easy_unlock_host = true;
    }
  }

  return has_supported_easy_unlock_host;
}

void EasyUnlockServiceRegular::OnWillFinalizeUnlock(bool success) {
  will_unlock_using_easy_unlock_ = success;
}

void EasyUnlockServiceRegular::OnSuspendDoneInternal() {
  lock_screen_last_shown_timestamp_ = base::TimeTicks::Now();
}

void EasyUnlockServiceRegular::OnSyncStarted() {
  unlock_keys_before_sync_ = GetCryptAuthDeviceManager()->GetUnlockKeys();
}

void EasyUnlockServiceRegular::OnSyncFinished(
    cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
    cryptauth::CryptAuthDeviceManager::DeviceChangeResult
        device_change_result) {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  if (sync_result == cryptauth::CryptAuthDeviceManager::SyncResult::FAILURE)
    return;

  std::set<std::string> public_keys_before_sync;
  for (const auto& device_info : unlock_keys_before_sync_) {
    public_keys_before_sync.insert(device_info.public_key());
  }
  unlock_keys_before_sync_.clear();

  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys_after_sync =
      GetCryptAuthDeviceManager()->GetUnlockKeys();
  std::set<std::string> public_keys_after_sync;
  for (const auto& device_info : unlock_keys_after_sync) {
    public_keys_after_sync.insert(device_info.public_key());
  }

  ShowNotificationIfNewDevicePresent(public_keys_before_sync,
                                     public_keys_after_sync);

  LoadRemoteDevices();
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
  // This method copies EasyUnlockServiceRegular::OnSyncFinished().
  // TODO(crbug.com/848956): Remove EasyUnlockServiceRegular::OnSyncFinished().

  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  std::set<std::string> public_keys_before_sync;
  for (const auto& remote_device : remote_device_unlock_keys_before_sync_) {
    public_keys_before_sync.insert(remote_device.public_key());
  }

  cryptauth::RemoteDeviceRefList remote_device_unlock_keys_after_sync =
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
  // TODO(crbug.com/894585): Remove after M71.
  bool is_in_legacy_host_mode = IsInLegacyHostMode();
  if (pref_manager_)
    pref_manager_->SetIsInLegacyHostMode(is_in_legacy_host_mode);

  const auto it =
      feature_states_map.find(multidevice_setup::mojom::Feature::kSmartLock);
  if (it == feature_states_map.end()) {
    feature_state_ =
        multidevice_setup::mojom::FeatureState::kUnavailableNoVerifiedHost;
    if (!is_in_legacy_host_mode)
      return;
  } else {
    feature_state_ = it->second;
  }

  // Note: In order to properly start the EasyUnlock app when MultiDeviceSetup
  // is enabled, we must ensure that UpdateAppState() gets called when the
  // EasyUnlock feature-state changes from kProhibitedByPolicy to
  // kUnavailableNoVerifiedHost.
  if (is_in_legacy_host_mode)
    UpdateAppState();

  LoadRemoteDevices();
}

void EasyUnlockServiceRegular::ShowChromebookAddedNotification() {
  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
         base::FeatureList::IsEnabled(
             chromeos::features::kEnableUnifiedMultiDeviceSetup));

  // The user may have decided to disable Smart Lock or the whole multidevice
  // suite immediately after completing setup, so ensure that Smart Lock is
  // enabled.
  if (feature_state_ == multidevice_setup::mojom::FeatureState::kEnabledByUser)
    notification_controller_->ShowChromebookAddedNotification();
}

void EasyUnlockServiceRegular::ShowNotificationIfNewDevicePresent(
    const std::set<std::string>& public_keys_before_sync,
    const std::set<std::string>& public_keys_after_sync) {
  if (public_keys_after_sync.empty())
    ClearPermitAccess();

  if (public_keys_before_sync == public_keys_after_sync)
    return;

  // Show the appropriate notification if an unlock key is first synced or if it
  // changes an existing key.
  // Note: We do not show a notification when EasyUnlock is disabled by sync nor
  // if EasyUnlock was enabled through the setup app.
  bool is_setup_fresh =
      short_lived_user_context_ && short_lived_user_context_->user_context();

  if (!public_keys_after_sync.empty() && !is_setup_fresh) {
    if (public_keys_before_sync.empty()) {
      if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
          base::FeatureList::IsEnabled(
              chromeos::features::kEnableUnifiedMultiDeviceSetup)) {
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
      }

      notification_controller_->ShowChromebookAddedNotification();
    } else {
      shown_pairing_changed_notification_ = true;
      notification_controller_->ShowPairingChangeNotification();
    }
  }
}

void EasyUnlockServiceRegular::OnForceSyncCompleted(bool success) {
  if (!success)
    PA_LOG(WARNING) << "Failed to force device sync.";
}

void EasyUnlockServiceRegular::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  will_unlock_using_easy_unlock_ = false;
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
    PA_LOG(INFO) << "Loading deferred devices after screen unlock.";
    deferring_device_load_ = false;
    LoadRemoteDevices();
  }

  // Do not process events for the login screen.
  if (screen_type != proximity_auth::ScreenlockBridge::LockHandler::LOCK_SCREEN)
    return;

  if (shown_pairing_changed_notification_) {
    shown_pairing_changed_notification_ = false;
    std::vector<cryptauth::ExternalDeviceInfo> unlock_keys =
        GetCryptAuthDeviceManager()->GetUnlockKeys();
    if (!unlock_keys.empty()) {
      // TODO(tengs): Right now, we assume that there is only one possible
      // unlock key. We need to update this notification be more generic.
      notification_controller_->ShowPairingChangeAppliedNotification(
          unlock_keys[0].friendly_device_name());
    }
  }

  // Only record metrics for users who have enabled the feature.
  if (IsEnabled()) {
    EasyUnlockAuthEvent event = will_unlock_using_easy_unlock_
                                    ? EASY_UNLOCK_SUCCESS
                                    : GetPasswordAuthEvent();
    RecordEasyUnlockScreenUnlockEvent(event);

    if (will_unlock_using_easy_unlock_) {
      RecordEasyUnlockScreenUnlockDuration(base::TimeTicks::Now() -
                                           lock_screen_last_shown_timestamp_);
    }
  }

  will_unlock_using_easy_unlock_ = false;
}

void EasyUnlockServiceRegular::OnFocusedUserChanged(
    const AccountId& account_id) {
  // Nothing to do.
}

void EasyUnlockServiceRegular::SetTurnOffFlowStatus(TurnOffFlowStatus status) {
  turn_off_flow_status_ = status;
  NotifyTurnOffOperationStatusChanged();
}

void EasyUnlockServiceRegular::OnToggleEasyUnlockApiComplete(
    const cryptauth::ToggleEasyUnlockResponse& response) {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));
  cryptauth_client_.reset();
  OnTurnOffEasyUnlockSuccess();
}

void EasyUnlockServiceRegular::OnToggleEasyUnlockApiFailed(
    cryptauth::NetworkRequestError error) {
  PA_LOG(WARNING) << "ToggleEasyUnlock call failed: " << error;
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));
  OnTurnOffEasyUnlockFailure();
}

void EasyUnlockServiceRegular::OnTurnOffEasyUnlockCompleted(
    device_sync::mojom::NetworkRequestResult result_code) {
  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  if (result_code != device_sync::mojom::NetworkRequestResult::kSuccess) {
    PA_LOG(WARNING) << "ToggleEasyUnlock call failed: " << result_code;
    OnTurnOffEasyUnlockFailure();
  } else {
    OnTurnOffEasyUnlockSuccess();
  }
}

void EasyUnlockServiceRegular::OnTurnOffEasyUnlockSuccess() {
  PA_LOG(INFO) << "Successfully turned off Smart Lock.";
  LogToggleFeatureDisableResult(SmartLockResult::SUCCESS);

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    remote_device_unlock_keys_before_sync_ = GetUnlockKeys();
  } else {
    GetCryptAuthDeviceManager()->ForceSyncNow(
        cryptauth::InvocationReason::INVOCATION_REASON_FEATURE_TOGGLED);
  }

  EasyUnlockService::ResetLocalStateForUser(GetAccountId());
  SetRemoteDevices(base::ListValue());
  SetProximityAuthDevices(GetAccountId(), cryptauth::RemoteDeviceRefList(),
                          base::nullopt /* local_device */);
  pref_manager_->SetIsEasyUnlockEnabled(false);
  SetTurnOffFlowStatus(IDLE);
  ResetScreenlockState();
  registrar_.RemoveAll();
}

void EasyUnlockServiceRegular::OnTurnOffEasyUnlockFailure() {
  LogToggleFeatureDisableResult(SmartLockResult::FAILURE);
  SetTurnOffFlowStatus(FAIL);
}

cryptauth::CryptAuthEnrollmentManager*
EasyUnlockServiceRegular::GetCryptAuthEnrollmentManager() {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  cryptauth::CryptAuthEnrollmentManager* manager =
      ChromeCryptAuthServiceFactory::GetInstance()
          ->GetForBrowserContext(profile())
          ->GetCryptAuthEnrollmentManager();
  DCHECK(manager);
  return manager;
}

cryptauth::CryptAuthDeviceManager*
EasyUnlockServiceRegular::GetCryptAuthDeviceManager() {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  cryptauth::CryptAuthDeviceManager* manager =
      ChromeCryptAuthServiceFactory::GetInstance()
          ->GetForBrowserContext(profile())
          ->GetCryptAuthDeviceManager();
  DCHECK(manager);
  return manager;
}

void EasyUnlockServiceRegular::RefreshCryptohomeKeysIfPossible() {
  // If the user reauthed on the settings page, then the UserContext will be
  // cached.
  if (short_lived_user_context_ && short_lived_user_context_->user_context()) {
    // We only sync the remote devices to cryptohome if the user has enabled
    // EasyUnlock on the login screen.
    base::ListValue empty_list;
    const base::ListValue* remote_devices_list = GetRemoteDevices();
    if (!IsChromeOSLoginEnabled() || !remote_devices_list)
      remote_devices_list = &empty_list;

    UserSessionManager::GetInstance()->GetEasyUnlockKeyManager()->RefreshKeys(
        *short_lived_user_context_->user_context(),
        base::ListValue(remote_devices_list->GetList()),
        base::Bind(&EasyUnlockServiceRegular::SetHardlockAfterKeyOperation,
                   weak_ptr_factory_.GetWeakPtr(),
                   EasyUnlockScreenlockStateHandler::NO_HARDLOCK));
  } else {
    CheckCryptohomeKeysAndMaybeHardlock();
  }
}

cryptauth::RemoteDeviceRefList EasyUnlockServiceRegular::GetUnlockKeys() {
  cryptauth::RemoteDeviceRefList unlock_keys;
  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    bool unlock_key = remote_device.GetSoftwareFeatureState(
                          cryptauth::SoftwareFeature::EASY_UNLOCK_HOST) ==
                      cryptauth::SoftwareFeatureState::kEnabled;
    if (unlock_key)
      unlock_keys.push_back(remote_device);
  }
  return unlock_keys;
}

}  // namespace chromeos
