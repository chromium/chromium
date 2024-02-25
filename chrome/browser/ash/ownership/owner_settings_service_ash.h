// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/owner_settings_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

namespace ownership {
class OwnerKeyUtil;
}  // namespace ownership

namespace ash {

class OwnerKeyLoader;

// The class is a profile-keyed service which holds public/private key pair
// corresponds to a profile. The key pair is reloaded automatically when profile
// is created and TPM token is ready. Note that the private part of a key can be
// loaded only for the owner.
//
// TODO (ygorshenin@): move write path for device settings here
// (crbug.com/230018).
class OwnerSettingsServiceAsh : public ownership::OwnerSettingsService,
                                public ProfileManagerObserver,
                                public SessionManagerClient::Observer,
                                public DeviceSettingsService::Observer {
 public:
  struct ManagementSettings {
    ManagementSettings();
    ~ManagementSettings();

    std::string request_token;
    std::string device_id;
  };

  OwnerSettingsServiceAsh(const OwnerSettingsServiceAsh&) = delete;
  OwnerSettingsServiceAsh& operator=(const OwnerSettingsServiceAsh&) = delete;

  ~OwnerSettingsServiceAsh() override;

  static OwnerSettingsServiceAsh* FromWebUI(content::WebUI* web_ui);

  void OnTPMTokenReady();

  void OnEasyUnlockKeyOpsFinished();

  bool HasPendingChanges() const;

  // ownership::OwnerSettingsService implementation:
  bool IsOwner() override;
  void IsOwnerAsync(IsOwnerCallback callback) override;
  bool HandlesSetting(const std::string& setting) override;
  bool Set(const std::string& setting, const base::Value& value) override;
  bool AppendToList(const std::string& setting,
                    const base::Value& value) override;
  bool RemoveFromList(const std::string& setting,
                      const base::Value& value) override;
  bool CommitTentativeDeviceSettings(
      std::unique_ptr<enterprise_management::PolicyData> policy) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // SessionManagerClient::Observer:
  void OwnerKeySet(bool success) override;

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;
  void DeviceSettingsUpdated() override;
  void OnDeviceSettingsServiceShutdown() override;

  // Checks if the user is the device owner, without the user profile having to
  // been initialized. Should be used only if login state is in safe mode.
  static void IsOwnerForSafeModeAsync(
      const std::string& user_hash,
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util,
      IsOwnerCallback callback);

  // Assembles PolicyData based on |settings|, |policy_data|, |user_id| and
  // |pending_management_settings|. Applies local-owner policy fixups if needed.
  static std::unique_ptr<enterprise_management::PolicyData> AssemblePolicy(
      const std::string& user_id,
      const enterprise_management::PolicyData* policy_data,
      enterprise_management::ChromeDeviceSettingsProto* settings);

  // Updates device |settings|.
  static void UpdateDeviceSettings(
      const std::string& path,
      const base::Value& value,
      enterprise_management::ChromeDeviceSettingsProto& settings);

  void SetPrivateKeyForTesting(
      scoped_refptr<ownership::PrivateKey> private_key);

 protected:
  OwnerSettingsServiceAsh(
      DeviceSettingsService* device_settings_service,
      Profile* profile,
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

 private:
  friend class OwnerSettingsServiceAshFactory;

  // Perform fixups required to ensure sensical local-owner device policy:
  //  1) user whitelisting must be explicitly allowed or disallowed, and
  //  2) the owner user must be on the whitelist, if it's enforced.
  static void FixupLocalOwnerPolicy(
      const std::string& user_id,
      enterprise_management::ChromeDeviceSettingsProto* settings);

  // OwnerSettingsService protected interface overrides:

  // Reloads private key from profile's NSS slots, responds via |callback|. On
  // success, |private_key| is non-null, but if the private key doesn't exist,
  // |private_key->key()| may be null.
  void ReloadKeypairImpl(
      base::OnceCallback<void(scoped_refptr<ownership::PublicKey> public_key,
                              scoped_refptr<ownership::PrivateKey> private_key)>
          callback) override;

  void OnReloadedKeypairImpl(
      base::OnceCallback<void(scoped_refptr<ownership::PublicKey>,
                              scoped_refptr<ownership::PrivateKey>)> callback,
      scoped_refptr<ownership::PublicKey> public_key,
      scoped_refptr<ownership::PrivateKey> private_key);

  // Possibly notifies DeviceSettingsService that owner's key pair is loaded.
  void OnPostKeypairLoadedActions() override;

  // Tries to apply recent changes to device settings proto, sign it and store.
  void StorePendingChanges();

  // Returns the latest list for setting.
  base::Value::List GetListForSetting(const std::string& setting) const;

  // Called when current device settings are successfully signed. |public_key|
  // is the public part of the key that was used for signing. Sends signed
  // settings for storage.
  void OnPolicyAssembledAndSigned(
      scoped_refptr<ownership::PublicKey> public_key,
      std::unique_ptr<enterprise_management::PolicyFetchResponse>
          policy_response);

  // Called by DeviceSettingsService when modified and signed device
  // settings are stored. |public_key| is the public part of the key that was
  // used for signing.
  void OnSignedPolicyStored(scoped_refptr<ownership::PublicKey> public_key,
                            bool success);

  // Report status to observers and tries to continue storing pending chages to
  // device settings.
  void ReportStatusAndContinueStoring(bool success);

  // Migrates feature flags from being stored as raw switches to being stored as
  // feature flag names.
  void MigrateFeatureFlags(
      enterprise_management::ChromeDeviceSettingsProto* settings);

  raw_ptr<DeviceSettingsService> device_settings_service_;

  // Profile this service instance belongs to.
  raw_ptr<Profile> profile_;

  // User ID this service instance belongs to.
  std::string user_id_;

  // Whether TPM token still needs to be initialized.
  bool waiting_for_tpm_token_ = true;

  // True if local-owner policy fixups are still pending.
  bool has_pending_fixups_ = false;

  // A set of pending changes to device settings.
  std::unordered_map<std::string, std::unique_ptr<base::Value>>
      pending_changes_;

  // A protobuf containing pending changes to device settings.
  std::unique_ptr<enterprise_management::ChromeDeviceSettingsProto>
      tentative_settings_;

  // A helper to load an existing owner key or generate a new one when
  // necessary.
  std::unique_ptr<OwnerKeyLoader> owner_key_loader_;
  crypto::ScopedSECKEYPrivateKey old_owner_key_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::WeakPtrFactory<OwnerSettingsServiceAsh> weak_factory_{this};

  base::WeakPtrFactory<OwnerSettingsServiceAsh> store_settings_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OWNERSHIP_OWNER_SETTINGS_SERVICE_ASH_H_
