// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/owner_settings_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

class Profile;

namespace content {
class WebUI;
}

namespace ownership {
class OwnerKeyUtil;
}

namespace chromeos {

// The class is a profile-keyed service which holds public/private keypair
// corresponds to a profile. The keypair is reloaded automatically when profile
// is created and TPM token is ready. Note that the private part of a key can be
// loaded only for the owner.
//
// TODO (ygorshenin@): move write path for device settings here
// (crbug.com/230018).
class OwnerSettingsServiceChromeOS : public ownership::OwnerSettingsService,
                                     public ProfileManagerObserver,
                                     public SessionManagerClient::Observer,
                                     public DeviceSettingsService::Observer {
 public:
  typedef base::Callback<void(bool success)> OnManagementSettingsSetCallback;

  struct ManagementSettings {
    ManagementSettings();
    ~ManagementSettings();

    std::string request_token;
    std::string device_id;
  };

  ~OwnerSettingsServiceChromeOS() override;

  static OwnerSettingsServiceChromeOS* FromWebUI(content::WebUI* web_ui);

  void OnTPMTokenReady(bool tpm_token_enabled);

  void OnEasyUnlockKeyOpsFinished();

  bool HasPendingChanges() const;

  // ownership::OwnerSettingsService implementation:
  bool IsOwner() override;
  void IsOwnerAsync(const IsOwnerCallback& callback) override;
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
      const IsOwnerCallback& callback);

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

 protected:
  OwnerSettingsServiceChromeOS(
      DeviceSettingsService* device_settings_service,
      Profile* profile,
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

 private:
  friend class OwnerSettingsServiceChromeOSFactory;

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
  void ReloadKeypairImpl(const base::Callback<
      void(const scoped_refptr<ownership::PublicKey>& public_key,
           const scoped_refptr<ownership::PrivateKey>& private_key)>& callback)
      override;

  // Possibly notifies DeviceSettingsService that owner's keypair is loaded.
  void OnPostKeypairLoadedActions() override;

  // Tries to apply recent changes to device settings proto, sign it and store.
  void StorePendingChanges();

  // Called when current device settings are successfully signed.
  // Sends signed settings for storage.
  void OnPolicyAssembledAndSigned(
      std::unique_ptr<enterprise_management::PolicyFetchResponse>
          policy_response);

  // Called by DeviceSettingsService when modified and signed device
  // settings are stored.
  void OnSignedPolicyStored(bool success);

  // Report status to observers and tries to continue storing pending chages to
  // device settings.
  void ReportStatusAndContinueStoring(bool success);

  DeviceSettingsService* device_settings_service_;

  // Profile this service instance belongs to.
  Profile* profile_;

  // User ID this service instance belongs to.
  std::string user_id_;

  // Whether TPM token still needs to be initialized.
  bool waiting_for_tpm_token_ = true;

  // Whether easy unlock operation is finished.
  bool waiting_for_easy_unlock_operation_finshed_ = true;

  // True if local-owner policy fixups are still pending.
  bool has_pending_fixups_ = false;

  // A set of pending changes to device settings.
  std::unordered_map<std::string, std::unique_ptr<base::Value>>
      pending_changes_;

  // A protobuf containing pending changes to device settings.
  std::unique_ptr<enterprise_management::ChromeDeviceSettingsProto>
      tentative_settings_;

  base::WeakPtrFactory<OwnerSettingsServiceChromeOS> weak_factory_{this};

  base::WeakPtrFactory<OwnerSettingsServiceChromeOS> store_settings_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_H_
