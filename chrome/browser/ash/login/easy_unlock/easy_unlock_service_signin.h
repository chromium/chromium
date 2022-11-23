// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_SIGNIN_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_SIGNIN_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"

namespace proximity_auth {
class ProximityAuthLocalStatePrefManager;
}  // namespace proximity_auth

namespace ash {

class EasyUnlockChallengeWrapper;

namespace multidevice {
class RemoteDeviceCache;
}

namespace secure_channel {
class SecureChannelClient;
}

// EasyUnlockService instance that should be used for signin profile.
class EasyUnlockServiceSignin : public EasyUnlockService {
 public:
  EasyUnlockServiceSignin(
      Profile* profile,
      secure_channel::SecureChannelClient* secure_channel_client);

  EasyUnlockServiceSignin(const EasyUnlockServiceSignin&) = delete;
  EasyUnlockServiceSignin& operator=(const EasyUnlockServiceSignin&) = delete;

  ~EasyUnlockServiceSignin() override;

  // Wraps the challenge for the remote device identified by `account_id` and
  // the
  // `device_public_key`. The `channel_binding_data` is signed by the TPM
  // included in the wrapped challenge.
  // `callback` will be invoked when wrapping is complete. If the user data is
  // not loaded yet, then `callback` will be invoked with an empty string.
  void WrapChallengeForUserAndDevice(
      const AccountId& account_id,
      const std::string& device_public_key,
      const std::string& channel_binding_data,
      base::OnceCallback<void(const std::string& wraped_challenge)> callback);

 private:
  // The load state of a user's cryptohome key data.
  enum UserDataState {
    // Initial state, the key data is empty and not being loaded.
    USER_DATA_STATE_INITIAL,
    // The key data is empty, but being loaded.
    USER_DATA_STATE_LOADING,
    // The key data has been loaded.
    USER_DATA_STATE_LOADED
  };

  // Structure containing a user's key data loaded from cryptohome.
  struct UserData {
    UserData();

    UserData(const UserData&) = delete;
    UserData& operator=(const UserData&) = delete;

    ~UserData();

    // The loading state of the data.
    UserDataState state;

    // The data as returned from cryptohome.
    EasyUnlockDeviceKeyDataList devices;

    // The list of remote device dictionaries understood by Easy unlock app.
    // This will be returned by `GetRemoteDevices` method.
    base::Value::List remote_devices_value;
  };

  // EasyUnlockService implementation:
  proximity_auth::ProximityAuthPrefManager* GetProximityAuthPrefManager()
      override;
  EasyUnlockService::Type GetType() const override;
  AccountId GetAccountId() const override;
  const base::Value::List* GetRemoteDevices() const override;
  std::string GetChallenge() const override;
  std::string GetWrappedSecret() const override;
  void RecordEasySignInOutcome(const AccountId& account_id,
                               bool success) const override;
  void RecordPasswordLoginEvent(const AccountId& account_id) const override;
  void InitializeInternal() override;
  void ShutdownInternal() override;
  bool IsAllowedInternal() const override;
  bool IsEnabled() const override;
  bool IsChromeOSLoginEnabled() const override;
  SmartLockState GetInitialSmartLockState() const override;
  void OnSuspendDoneInternal() override;

  // EasyUnlockService:
  void OnScreenDidLock(proximity_auth::ScreenlockBridge::LockHandler::ScreenType
                           screen_type) override;
  void OnScreenDidUnlock(
      proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type)
      override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  // Loads the device data associated with the user's Easy unlock keys from
  // crypthome.
  void LoadCurrentUserDataIfNeeded();

  // Callback invoked when the user's device data is loaded from cryptohome.
  void OnUserDataLoaded(const AccountId& account_id,
                        bool success,
                        const EasyUnlockDeviceKeyDataList& data);

  // If the device data has been loaded for the current user, returns it.
  // Otherwise, returns NULL.
  const UserData* FindLoadedDataForCurrentUser() const;

  // Shows the hardlock or connecting state as initial UI before cryptohome
  // keys checking and state update from the app.
  void ShowInitialUserPodState();

  // User id of the user currently associated with the service.
  AccountId account_id_;

  // Maps account ids to their fetched cryptohome key data.
  std::map<AccountId, std::unique_ptr<UserData>> user_data_;

  // Whether failed attempts to load user data should be retried.
  // This is to handle case where cryptohome daemon is not started in time the
  // service attempts to load some data. Retries will be allowed only until the
  // first data load finishes (even if it fails).
  bool allow_cryptohome_backoff_ = true;

  // Whether the service has been successfully initialized, and has not been
  // shut down.
  bool service_active_ = false;

  // The timestamp for the most recent time when a user pod was focused.
  base::TimeTicks user_pod_last_focused_timestamp_;

  std::unique_ptr<multidevice::RemoteDeviceCache> remote_device_cache_;

  // Handles wrapping the user's challenge with the TPM.
  std::unique_ptr<EasyUnlockChallengeWrapper> challenge_wrapper_;

  // Manages the EasyUnlock prefs for the local state.
  std::unique_ptr<proximity_auth::ProximityAuthLocalStatePrefManager>
      pref_manager_;

  base::WeakPtrFactory<EasyUnlockServiceSignin> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_SIGNIN_H_
