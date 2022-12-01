// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_signin.h"

#include <stdint.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/smartlock_state.h"
#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_challenge_wrapper.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_metrics.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_local_state_pref_manager.h"
#include "chromeos/ash/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "chromeos/ash/components/tpm/tpm_token_loader.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace ash {

namespace {

// The maximum allowed backoff interval when waiting for cryptohome to start.
uint32_t kMaxCryptohomeBackoffIntervalMs = 10000u;

// If the data load fails, the initial interval after which the load will be
// retried. Further intervals will exponentially increas by factor 2.
uint32_t kInitialCryptohomeBackoffIntervalMs = 200u;

// Calculates the backoff interval that should be used next.
// `backoff` The last backoff interval used.
uint32_t GetNextBackoffInterval(uint32_t backoff) {
  if (backoff == 0u)
    return kInitialCryptohomeBackoffIntervalMs;
  return backoff * 2;
}

void LoadDataForUser(const AccountId& account_id,
                     uint32_t backoff_ms,
                     EasyUnlockKeyManager::GetDeviceDataListCallback callback);

// Callback passed to `LoadDataForUser()`.
// If `LoadDataForUser` function succeeded, it invokes `callback` with the
// results.
// If `LoadDataForUser` failed and further retries are allowed, schedules new
// `LoadDataForUser` call with some backoff. If no further retires are allowed,
// it invokes `callback` with the `LoadDataForUser` results.
void RetryDataLoadOnError(
    const AccountId& account_id,
    uint32_t backoff_ms,
    EasyUnlockKeyManager::GetDeviceDataListCallback callback,
    bool success,
    const EasyUnlockDeviceKeyDataList& data_list) {
  if (success) {
    std::move(callback).Run(success, data_list);
    return;
  }

  uint32_t next_backoff_ms = GetNextBackoffInterval(backoff_ms);
  if (next_backoff_ms > kMaxCryptohomeBackoffIntervalMs) {
    std::move(callback).Run(false, data_list);
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LoadDataForUser, account_id, next_backoff_ms,
                     std::move(callback)),
      base::Milliseconds(next_backoff_ms));
}

// Loads device data list associated with the user's Easy unlock keys.
void LoadDataForUser(const AccountId& account_id,
                     uint32_t backoff_ms,
                     EasyUnlockKeyManager::GetDeviceDataListCallback callback) {
  EasyUnlockKeyManager* key_manager =
      UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
  DCHECK(key_manager);

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  key_manager->GetDeviceDataList(
      UserContext(*user), base::BindOnce(&RetryDataLoadOnError, account_id,
                                         backoff_ms, std::move(callback)));
}

// Deserializes a vector of BeaconSeeds. If an error occurs, an empty vector
// will be returned. Note: The logic to serialize BeaconSeeds lives in
// EasyUnlockServiceRegular.
// Note: The serialization of device data inside a user session is different
// than outside the user session (sign-in). RemoteDevices are serialized as
// protocol buffers inside the user session, but we have a custom serialization
// scheme for sign-in due to slightly different data requirements.
std::vector<multidevice::BeaconSeed> DeserializeBeaconSeeds(
    const std::string& serialized_beacon_seeds) {
  std::vector<multidevice::BeaconSeed> beacon_seeds;

  JSONStringValueDeserializer deserializer(serialized_beacon_seeds);
  std::string error;
  std::unique_ptr<base::Value> deserialized_value =
      deserializer.Deserialize(nullptr, &error);
  if (!deserialized_value) {
    PA_LOG(ERROR) << "Unable to deserialize BeaconSeeds: " << error;
    return beacon_seeds;
  }

  if (!deserialized_value->is_list()) {
    PA_LOG(ERROR) << "Deserialized BeaconSeeds value is not list.";
    return beacon_seeds;
  }

  for (const base::Value& beacon_seed_value : deserialized_value->GetList()) {
    if (!beacon_seed_value.is_string()) {
      PA_LOG(ERROR) << "Expected Base64 BeaconSeed.";
      continue;
    }
    const std::string& b64_beacon_seed = beacon_seed_value.GetString();

    std::string proto_serialized_beacon_seed;
    if (!base::Base64UrlDecode(b64_beacon_seed,
                               base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                               &proto_serialized_beacon_seed)) {
      PA_LOG(ERROR) << "Unable to decode BeaconSeed.";
      continue;
    }

    cryptauth::BeaconSeed beacon_seed;
    if (!beacon_seed.ParseFromString(proto_serialized_beacon_seed)) {
      PA_LOG(ERROR) << "Unable to parse BeaconSeed proto.";
      continue;
    }

    beacon_seeds.push_back(multidevice::FromCryptAuthSeed(beacon_seed));
  }

  PA_LOG(VERBOSE) << "Deserialized " << beacon_seeds.size() << " BeaconSeeds.";
  return beacon_seeds;
}

}  // namespace

EasyUnlockServiceSignin::UserData::UserData()
    : state(EasyUnlockServiceSignin::USER_DATA_STATE_INITIAL) {}

EasyUnlockServiceSignin::UserData::~UserData() {}

EasyUnlockServiceSignin::EasyUnlockServiceSignin(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client)
    : EasyUnlockService(profile, secure_channel_client),
      account_id_(EmptyAccountId()),
      user_pod_last_focused_timestamp_(base::TimeTicks::Now()),
      remote_device_cache_(multidevice::RemoteDeviceCache::Factory::Create()) {}

EasyUnlockServiceSignin::~EasyUnlockServiceSignin() {}

void EasyUnlockServiceSignin::WrapChallengeForUserAndDevice(
    const AccountId& account_id,
    const std::string& device_public_key,
    const std::string& channel_binding_data,
    base::OnceCallback<void(const std::string& wraped_challenge)> callback) {
  auto it = user_data_.find(account_id);
  if (it == user_data_.end() || it->second->state != USER_DATA_STATE_LOADED) {
    PA_LOG(ERROR) << "TPM data not loaded for " << account_id.Serialize();
    std::move(callback).Run(std::string());
    return;
  }

  std::string device_public_key_base64;
  base::Base64UrlEncode(device_public_key,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &device_public_key_base64);
  for (const auto& device_data : it->second->devices) {
    if (device_data.public_key == device_public_key_base64) {
      PA_LOG(VERBOSE) << "Wrapping challenge for " << account_id.Serialize()
                      << "...";
      challenge_wrapper_ = std::make_unique<EasyUnlockChallengeWrapper>(
          device_data.challenge, channel_binding_data, account_id,
          EasyUnlockTpmKeyManagerFactory::GetInstance()->Get(profile()));
      challenge_wrapper_->WrapChallenge(std::move(callback));
      return;
    }
  }

  PA_LOG(ERROR) << "Unable to find device record for "
                << account_id.Serialize();
  std::move(callback).Run(std::string());
}

proximity_auth::ProximityAuthPrefManager*
EasyUnlockServiceSignin::GetProximityAuthPrefManager() {
  return pref_manager_.get();
}

EasyUnlockService::Type EasyUnlockServiceSignin::GetType() const {
  return EasyUnlockService::TYPE_SIGNIN;
}

AccountId EasyUnlockServiceSignin::GetAccountId() const {
  return account_id_;
}

const base::Value::List* EasyUnlockServiceSignin::GetRemoteDevices() const {
  const UserData* data = FindLoadedDataForCurrentUser();
  if (!data)
    return nullptr;
  return &data->remote_devices_value;
}

std::string EasyUnlockServiceSignin::GetChallenge() const {
  const UserData* data = FindLoadedDataForCurrentUser();
  if (!data)
    return std::string();

  for (const auto& device : data->devices) {
    if (device.unlock_key)
      return device.challenge;
  }

  return std::string();
}

std::string EasyUnlockServiceSignin::GetWrappedSecret() const {
  const UserData* data = FindLoadedDataForCurrentUser();
  if (!data)
    return std::string();

  for (const auto& device : data->devices) {
    if (device.unlock_key)
      return device.wrapped_secret;
  }

  return std::string();
}

void EasyUnlockServiceSignin::RecordEasySignInOutcome(
    const AccountId& account_id,
    bool success) const {
  DCHECK(GetAccountId() == account_id)
      << "GetAccountId()=" << GetAccountId().Serialize()
      << " != account_id=" << account_id.Serialize();

  RecordEasyUnlockSigninEvent(success ? EASY_UNLOCK_SUCCESS
                                      : EASY_UNLOCK_FAILURE);
  if (success) {
    RecordEasyUnlockSigninDuration(base::TimeTicks::Now() -
                                   user_pod_last_focused_timestamp_);
  }
  DVLOG(1) << "Easy sign-in " << (success ? "success" : "failure");
}

void EasyUnlockServiceSignin::RecordPasswordLoginEvent(
    const AccountId& account_id) const {
  // This happens during tests, where a user could log in without the user pod
  // being focused.
  if (GetAccountId() != account_id)
    return;

  if (!IsEnabled())
    return;

  EasyUnlockAuthEvent event = GetPasswordAuthEvent();
  RecordEasyUnlockSigninEvent(event);

  SmartLockMetricsRecorder::RecordAuthMethodChoiceSignInPasswordState(
      GetSmartUnlockPasswordAuthEvent());

  DVLOG(1) << "Easy Sign-in password login event, event=" << event;
}

void EasyUnlockServiceSignin::InitializeInternal() {
  if (LoginState::Get()->IsUserLoggedIn())
    return;

  service_active_ = true;

  pref_manager_ =
      std::make_unique<proximity_auth::ProximityAuthLocalStatePrefManager>(
          g_browser_process->local_state());

  proximity_auth::ScreenlockBridge* screenlock_bridge =
      proximity_auth::ScreenlockBridge::Get();
  if (screenlock_bridge->focused_account_id().is_valid())
    OnFocusedUserChanged(screenlock_bridge->focused_account_id());
}

void EasyUnlockServiceSignin::ShutdownInternal() {
  if (!service_active_)
    return;
  service_active_ = false;

  remote_device_cache_.reset();
  challenge_wrapper_.reset();
  pref_manager_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();
  user_data_.clear();
}

bool EasyUnlockServiceSignin::IsAllowedInternal() const {
  return service_active_ && account_id_.is_valid() &&
         !LoginState::Get()->IsUserLoggedIn() &&
         (pref_manager_ && pref_manager_->IsEasyUnlockAllowed() &&
          pref_manager_->IsChromeOSLoginAllowed());
}

bool EasyUnlockServiceSignin::IsEnabled() const {
  return pref_manager_ && pref_manager_->IsEasyUnlockEnabled();
}

bool EasyUnlockServiceSignin::IsChromeOSLoginEnabled() const {
  return pref_manager_ && pref_manager_->IsChromeOSLoginEnabled();
}

SmartLockState EasyUnlockServiceSignin::GetInitialSmartLockState() const {
  if (IsChromeOSLoginEnabled())
    return EasyUnlockService::GetInitialSmartLockState();

  return SmartLockState::kDisabled;
}

void EasyUnlockServiceSignin::OnSuspendDoneInternal() {
  // Ignored.
}

void EasyUnlockServiceSignin::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  // In production code, the screen type should always be the signin screen; but
  // in tests, the screen type might be different.
  if (screen_type !=
      proximity_auth::ScreenlockBridge::LockHandler::SIGNIN_SCREEN)
    return;

  EasyUnlockService::OnScreenDidLock(screen_type);

  if (!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    // Update initial UI is when the account picker on login screen is ready.
    ShowInitialUserPodState();
  }
  user_pod_last_focused_timestamp_ = base::TimeTicks::Now();
}

void EasyUnlockServiceSignin::OnScreenDidUnlock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  // In production code, the screen type should always be the signin screen; but
  // in tests, the screen type might be different.
  if (screen_type !=
      proximity_auth::ScreenlockBridge::LockHandler::SIGNIN_SCREEN)
    return;

  // TODO(crbug.com/1171972): Deprecate this metric. Note also that checking
  // IsEnabled() here often incorrectly returns false, because
  // OnScreenDidUnlock() is occurring during user session startup. See
  // https://crbug.com/1154766 for more.
  if (IsEnabled()) {
    SmartLockMetricsRecorder::RecordSmartLockSignInAuthMethodChoice(
        will_authenticate_using_easy_unlock()
            ? SmartLockMetricsRecorder::SmartLockAuthMethodChoice::kSmartLock
            : SmartLockMetricsRecorder::SmartLockAuthMethodChoice::kOther);
  }

  // TODO(crbug.com/972156): A KeyedService shutting itself seems dangerous;
  // look into other ways to "reset state" besides this.
  Shutdown();
}

void EasyUnlockServiceSignin::OnFocusedUserChanged(
    const AccountId& account_id) {
  if (account_id_ == account_id)
    return;

  // Even if |pref_manager_| is not present, continue resetting state for
  // |account_id| below. The function will return once IsAllowed() below
  // returns false (because |pref_manager_| is not present).
  if (pref_manager_) {
    pref_manager_->SetActiveUser(account_id);
  }

  account_id_ = account_id;
  user_pod_last_focused_timestamp_ = base::TimeTicks::Now();
  SetProximityAuthDevices(account_id_, multidevice::RemoteDeviceRefList(),
                          absl::nullopt /* local_device */);
  ResetSmartLockState();

  // Changing the "Active User" above changes the return values of IsAllowed()
  // and IsEnabled() below.
  if (!IsAllowed() || !IsEnabled())
    return;

  // Update initial UI is when the account picker on login screen is ready.
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    ShowInitialSmartLockState();
  } else {
    ShowInitialUserPodState();
  }

  // If there is a hardlock, then there is no point in loading the devices.
  SmartLockStateHandler::HardlockState hardlock_state;
  if (GetPersistedHardlockState(&hardlock_state) &&
      hardlock_state != SmartLockStateHandler::NO_HARDLOCK) {
    PA_LOG(VERBOSE) << "Hardlock present, skipping remaining login flow.";
    return;
  }

  UpdateAppState();
  LoadCurrentUserDataIfNeeded();

  // Start loading TPM system token.
  // The system token will be needed to sign a nonce using TPM private key
  // during the sign-in protocol.
  TPMTokenLoader::Get()->EnsureStarted();
}

void EasyUnlockServiceSignin::LoadCurrentUserDataIfNeeded() {
  // TODO(xiyuan): Revisit this when adding tests.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return;

  if (!account_id_.is_valid() || !service_active_)
    return;

  const auto it = user_data_.find(account_id_);
  if (it == user_data_.end())
    user_data_.insert(
        std::make_pair(account_id_, std::make_unique<UserData>()));

  UserData* data = user_data_[account_id_].get();

  if (data->state == USER_DATA_STATE_LOADING)
    return;
  data->state = USER_DATA_STATE_LOADING;

  LoadDataForUser(
      account_id_,
      allow_cryptohome_backoff_ ? 0u : kMaxCryptohomeBackoffIntervalMs,
      base::BindOnce(&EasyUnlockServiceSignin::OnUserDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), account_id_));
}

// TODO(crbug.com/856387): Write tests for device retrieval from the TPM.
void EasyUnlockServiceSignin::OnUserDataLoaded(
    const AccountId& account_id,
    bool success,
    const EasyUnlockDeviceKeyDataList& devices) {
  allow_cryptohome_backoff_ = false;

  UserData* data = user_data_[account_id].get();
  data->state = USER_DATA_STATE_LOADED;
  if (success) {
    data->devices = devices;
    EasyUnlockKeyManager::DeviceDataListToRemoteDeviceList(
        account_id, devices, &data->remote_devices_value);

    // User could have a NO_HARDLOCK state but has no remote devices if
    // previous user session shuts down before
    // CheckCryptohomeKeysAndMaybeHardlock finishes. Set NO_PAIRING state
    // and update UI to remove the confusing spinner in this case.
    SmartLockStateHandler::HardlockState hardlock_state;
    if (devices.empty() && GetPersistedHardlockState(&hardlock_state) &&
        hardlock_state == SmartLockStateHandler::NO_HARDLOCK) {
      SetHardlockStateForUser(account_id, SmartLockStateHandler::NO_PAIRING);
    }
  }

  if (devices.empty())
    return;

  multidevice::RemoteDeviceList remote_devices;
  for (const auto& device : devices) {
    std::string decoded_public_key, decoded_psk;
    if (!base::Base64UrlDecode(device.public_key,
                               base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                               &decoded_public_key) ||
        !base::Base64UrlDecode(device.psk,
                               base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                               &decoded_psk)) {
      PA_LOG(ERROR) << "Unable to decode stored remote device:\n"
                    << "  public_key: " << device.public_key << "\n"
                    << "  psk: " << device.psk;
      continue;
    }

    std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>
        software_features;
    software_features[multidevice::SoftwareFeature::kSmartLockHost] =
        device.unlock_key ? multidevice::SoftwareFeatureState::kEnabled
                          : multidevice::SoftwareFeatureState::kNotSupported;

    std::vector<multidevice::BeaconSeed> beacon_seeds;
    if (!device.serialized_beacon_seeds.empty()) {
      PA_LOG(VERBOSE) << "Deserializing BeaconSeeds: "
                      << device.serialized_beacon_seeds;
      beacon_seeds = DeserializeBeaconSeeds(device.serialized_beacon_seeds);
    } else {
      PA_LOG(WARNING) << "No BeaconSeeds were loaded.";
    }

    // Values such as the `instance_id` and `name` of the device are not
    // provided in the device dictionary that is persisted to the TPM during the
    // user session. However, in this particular scenario, we do not need these
    // values to safely construct and use the RemoteDevice objects.
    multidevice::RemoteDevice remote_device(
        account_id.GetUserEmail(), std::string() /* instance_id */,
        std::string() /* name */, std::string() /* pii_free_name */,
        decoded_public_key, decoded_psk /* persistent_symmetric_key */,
        0L /* last_update_time_millis */, software_features, beacon_seeds,
        std::string() /* bluetooth_public_address */);

    remote_devices.push_back(remote_device);
    PA_LOG(VERBOSE) << "Loaded Remote Device:\n"
                    << "  user email: " << remote_device.user_email << "\n"
                    << "  device id: "
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           remote_device.GetDeviceId());
  }

  // Both a remote device and local device are expected, and this service cannot
  // continue unless both are present.
  // TODO(crbug.com/856380): The remote and local devices need to be passed in a
  // less hacky way.
  if (remote_devices.size() > 2u) {
    PA_LOG(ERROR)
        << "Expected a device list of size 1 or 2, received list of size "
        << remote_devices.size();
    SetHardlockStateForUser(account_id, SmartLockStateHandler::NO_PAIRING);
    return;
  }

  if (remote_devices.size() != 2u) {
    PA_LOG(ERROR) << "Expected a device list of size 2, received list of size "
                  << remote_devices.size();
    SetHardlockStateForUser(account_id, SmartLockStateHandler::PAIRING_CHANGED);
    return;
  }

  std::string unlock_key_id;
  // This may be left unset if the local device was not passed along.
  std::string local_device_id;

  for (const auto& remote_device : remote_devices) {
    if (base::Contains(remote_device.software_features,
                       multidevice::SoftwareFeature::kSmartLockHost) &&
        remote_device.software_features.at(
            multidevice::SoftwareFeature::kSmartLockHost) ==
            multidevice::SoftwareFeatureState::kEnabled) {
      if (!unlock_key_id.empty()) {
        PA_LOG(ERROR) << "Only one of the devices should be an unlock key.";
        SetHardlockStateForUser(account_id, SmartLockStateHandler::NO_PAIRING);
        return;
      }

      unlock_key_id = remote_device.GetDeviceId();
    } else {
      if (!local_device_id.empty()) {
        PA_LOG(ERROR) << "Only one of the devices should be the local device.";
        SetHardlockStateForUser(account_id, SmartLockStateHandler::NO_PAIRING);
        return;
      }

      local_device_id = remote_device.GetDeviceId();
    }
  }

  remote_device_cache_->SetRemoteDevices(remote_devices);

  absl::optional<multidevice::RemoteDeviceRef> unlock_key_device =
      remote_device_cache_->GetRemoteDevice(
          absl::nullopt /* instance_id */,
          unlock_key_id /* legacy_device_id */);
  absl::optional<multidevice::RemoteDeviceRef> local_device =
      remote_device_cache_->GetRemoteDevice(
          absl::nullopt /* instance_id */,
          local_device_id /* legacy_device_id */);

  // TODO(hansberry): It is possible that there may not be an unlock key by this
  // point. If this occurs, it is due to a bug in how device metadata is
  // persisted in CryptoHome. See https://crbug.com/856380 for more details. For
  // now, simply return early here to prevent a potential crash which can occur
  // in this situation (see https://crbug.com/866711).
  if (!unlock_key_device) {
    SetHardlockStateForUser(account_id, SmartLockStateHandler::NO_PAIRING);
    return;
  }

  if (!local_device) {
    SetHardlockStateForUser(account_id, SmartLockStateHandler::NO_PAIRING);
    return;
  }

  SetProximityAuthDevices(account_id, {*unlock_key_device}, local_device);
}

const EasyUnlockServiceSignin::UserData*
EasyUnlockServiceSignin::FindLoadedDataForCurrentUser() const {
  if (!account_id_.is_valid())
    return nullptr;

  const auto it = user_data_.find(account_id_);
  if (it == user_data_.end())
    return nullptr;
  if (it->second->state != USER_DATA_STATE_LOADED)
    return nullptr;
  return it->second.get();
}

void EasyUnlockServiceSignin::ShowInitialUserPodState() {
  DCHECK(!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp));

  if (!IsAllowed() || !IsEnabled())
    return;

  if (!pref_manager_->IsChromeOSLoginEnabled()) {
    // Show a hardlock state if the user has not enabled Smart Lock to the log
    // in to the user's Google account.
    SetHardlockStateForUser(account_id_, SmartLockStateHandler::LOGIN_DISABLED);
  } else {
    // This UI is simply a placeholder until the RemoteDevices are loaded from
    // cryptohome and the ProximityAuthSystem is started. Hardlock states are
    // automatically taken into account.
    UpdateSmartLockState(SmartLockState::kConnectingToPhone);
  }
}

}  // namespace ash
