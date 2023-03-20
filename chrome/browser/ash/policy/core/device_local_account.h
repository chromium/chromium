// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_H_

#include <string>
#include <vector>

namespace ash {
class CrosSettings;
class OwnerSettingsServiceAsh;
}  // namespace ash

namespace policy {

struct ArcKioskAppBasicInfo {
  ArcKioskAppBasicInfo(const std::string& package_name,
                       const std::string& class_name,
                       const std::string& action,
                       const std::string& display_name);
  ArcKioskAppBasicInfo(const ArcKioskAppBasicInfo& other);
  ArcKioskAppBasicInfo();
  ~ArcKioskAppBasicInfo();

  bool operator==(const ArcKioskAppBasicInfo& other) const;

  const std::string& package_name() const { return package_name_; }
  const std::string& class_name() const { return class_name_; }
  const std::string& action() const { return action_; }
  const std::string& display_name() const { return display_name_; }

 private:
  std::string package_name_;
  std::string class_name_;
  std::string action_;
  std::string display_name_;
};

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

// This must match DeviceLocalAccountInfoProto.AccountType in
// chrome_device_policy.proto.
struct DeviceLocalAccount {
  enum Type {
    // A login-less, policy-configured browsing session.
    TYPE_PUBLIC_SESSION,
    // An account that serves as a container for a single full-screen app.
    TYPE_KIOSK_APP,
    // An account that serves as a container for a single full-screen
    // Android app.
    TYPE_ARC_KIOSK_APP,
    // SAML public session account
    TYPE_SAML_PUBLIC_SESSION,
    // An account that serves as a container for a single full-screen web app.
    TYPE_WEB_KIOSK_APP,
    // Sentinel, must be last.
    TYPE_COUNT
  };

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

  DeviceLocalAccount(Type type,
                     EphemeralMode ephemeral_mode,
                     const std::string& account_id,
                     const std::string& kiosk_app_id,
                     const std::string& kiosk_app_update_url);
  DeviceLocalAccount(EphemeralMode ephemeral_mode,
                     const ArcKioskAppBasicInfo& arc_kiosk_app_info,
                     const std::string& account_id);
  DeviceLocalAccount(EphemeralMode ephemeral_mode,
                     const WebKioskAppBasicInfo& app_info,
                     const std::string& account_id);
  DeviceLocalAccount(const DeviceLocalAccount& other);
  ~DeviceLocalAccount();

  Type type;
  EphemeralMode ephemeral_mode;
  // A device-local account has two identifiers:
  // * The |account_id| is chosen by the entity that defines the device-local
  //   account. The only constraints are that the |account_id| be unique and,
  //   for legacy reasons, it contain an @ symbol.
  // * The |user_id| is a synthesized identifier that is guaranteed to be
  //   unique, contain an @ symbol, not collide with the |user_id| of any other
  //   user on the device (such as regular users or supervised users) and be
  //   identifiable as belonging to a device-local account by.
  // The |account_id| is primarily used by policy code: If device policy defines
  // a device-local account with a certain |account_id|, the user policy for
  // that account has to be fetched by referencing the same |account_id|.
  // The |user_id| is passed to the user_manager::UserManager where it becomes
  // part
  // of the global user list on the device. The |account_id| would not be safe
  // to use here as it is a free-form identifier that could conflict with
  // another |user_id| on the device and cannot be easily identified as
  // belonging to a device-local account.
  std::string account_id;
  std::string user_id;
  std::string kiosk_app_id;
  std::string kiosk_app_update_url;

  ArcKioskAppBasicInfo arc_kiosk_app_info;
  WebKioskAppBasicInfo web_kiosk_app_info;
};

std::string GenerateDeviceLocalAccountUserId(const std::string& account_id,
                                             DeviceLocalAccount::Type type);

// Determines whether |user_id| belongs to a device-local account and if so,
// returns the type of device-local account in |type| unless |type| is NULL.
bool IsDeviceLocalAccountUser(const std::string& user_id,
                              DeviceLocalAccount::Type* type);

// Stores a list of device-local accounts in |service|. The accounts are stored
// as a list of dictionaries with each dictionary containing the information
// about one |DeviceLocalAccount|.
void SetDeviceLocalAccounts(ash::OwnerSettingsServiceAsh* service,
                            const std::vector<DeviceLocalAccount>& accounts);

// Retrieves a list of device-local accounts from |cros_settings|.
std::vector<DeviceLocalAccount> GetDeviceLocalAccounts(
    ash::CrosSettings* cros_settings);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_H_
