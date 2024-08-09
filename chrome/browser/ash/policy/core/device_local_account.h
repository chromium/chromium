// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_H_

#include <string>
#include <vector>

#include "components/policy/core/common/device_local_account_type.h"

namespace ash {
class CrosSettings;
class OwnerSettingsServiceAsh;
}  // namespace ash

namespace policy {

struct WebKioskAppBasicInfo {
  WebKioskAppBasicInfo(const std::string& url,
                       const std::string& title,
                       const std::string& icon_url);
  WebKioskAppBasicInfo();
  ~WebKioskAppBasicInfo();

  const std::string& url() const { return url_; }
  const std::string& title() const { return title_; }
  const std::string& icon_url() const { return icon_url_; }

 private:
  std::string url_;
  std::string title_;
  std::string icon_url_;
};

struct IsolatedWebAppKioskBasicInfo {
 public:
  IsolatedWebAppKioskBasicInfo(std::string web_bundle_id,
                               std::string update_manifest_url);
  IsolatedWebAppKioskBasicInfo() = default;
  ~IsolatedWebAppKioskBasicInfo() = default;

  [[nodiscard]] const std::string& web_bundle_id() const {
    return web_bundle_id_;
  }

  [[nodiscard]] const std::string& update_manifest_url() const {
    return update_manifest_url_;
  }

 private:
  std::string web_bundle_id_;
  std::string update_manifest_url_;
};

// This must match DeviceLocalAccountInfoProto.AccountType in
// chrome_device_policy.proto.
struct DeviceLocalAccount {
  enum class EphemeralMode {
    // Default value. Same behaviour as `kFollowDeviceWidePolicy` value.
    kUnset = 0,
    // Device-local account ephemeral mode controlled by
    // `DeviceEphemeralUsersEnabled` policy.
    kFollowDeviceWidePolicy = 1,
    // Device-local account must be non-ephemeral.
    kDisable = 2,
    // Device-local account must be ephemeral.
    kEnable = 3,
    // Max value, must be last.
    kMaxValue = kEnable,
  };

  DeviceLocalAccount(DeviceLocalAccountType type,
                     EphemeralMode ephemeral_mode,
                     const std::string& account_id,
                     const std::string& kiosk_app_id,
                     const std::string& kiosk_app_update_url);

  DeviceLocalAccount(EphemeralMode ephemeral_mode,
                     const WebKioskAppBasicInfo& app_info,
                     const std::string& account_id);

  DeviceLocalAccount(EphemeralMode ephemeral_mode,
                     const IsolatedWebAppKioskBasicInfo& kiosk_iwa_info,
                     const std::string& account_id);

  DeviceLocalAccount(const DeviceLocalAccount& other);
  ~DeviceLocalAccount();

  DeviceLocalAccountType type;
  EphemeralMode ephemeral_mode;
  // A device-local account has two identifiers:
  // * The `account_id` is chosen by the entity that defines the device-local
  //   account. The only constraints are that the `account_id` be unique and,
  //   for legacy reasons, it contain an @ symbol.
  // * The `user_id` is a synthesized identifier that is guaranteed to be
  //   unique, contain an @ symbol, not collide with the `user_id` of any other
  //   user on the device (such as regular users or supervised users) and be
  //   identifiable as belonging to a device-local account by.
  // The `account_id` is primarily used by policy code: If device policy defines
  // a device-local account with a certain `account_id`, the user policy for
  // that account has to be fetched by referencing the same `account_id`.
  // The `user_id` is passed to the user_manager::UserManager where it becomes
  // part
  // of the global user list on the device. The `account_id` would not be safe
  // to use here as it is a free-form identifier that could conflict with
  // another `user_id` on the device and cannot be easily identified as
  // belonging to a device-local account.
  std::string account_id;
  std::string user_id;
  std::string kiosk_app_id;
  std::string kiosk_app_update_url;

  WebKioskAppBasicInfo web_kiosk_app_info;
  IsolatedWebAppKioskBasicInfo kiosk_iwa_info;
};

// Retrieves a list of device-local accounts from `cros_settings`.
std::vector<DeviceLocalAccount> GetDeviceLocalAccounts(
    ash::CrosSettings* cros_settings);

// Stores a list of device-local accounts in `service`. The accounts are stored
// as a list of dictionaries with each dictionary containing the information
// about one `DeviceLocalAccount`.
void SetDeviceLocalAccountsForTesting(
    ash::OwnerSettingsServiceAsh* service,
    const std::vector<DeviceLocalAccount>& accounts);
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_H_
