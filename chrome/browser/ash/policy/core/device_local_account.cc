// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace policy {

namespace {

bool GetString(const base::Value::Dict& dict,
               const char* key,
               std::string* result) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    return false;
  }
  *result = *value;
  return true;
}

bool IsKioskType(DeviceLocalAccountType type) {
  switch (type) {
    case DeviceLocalAccountType::kKioskApp:
    case DeviceLocalAccountType::kWebKioskApp:
    case DeviceLocalAccountType::kKioskIsolatedWebApp:
      return true;
    case DeviceLocalAccountType::kPublicSession:
    case DeviceLocalAccountType::kSamlPublicSession:
      return false;
  }
  NOTREACHED();
}

}  // namespace

WebKioskAppBasicInfo::WebKioskAppBasicInfo(const std::string& url,
                                           const std::string& title,
                                           const std::string& icon_url)
    : url_(url), title_(title), icon_url_(icon_url) {}

WebKioskAppBasicInfo::WebKioskAppBasicInfo() = default;

WebKioskAppBasicInfo::~WebKioskAppBasicInfo() = default;

IsolatedWebAppKioskBasicInfo::IsolatedWebAppKioskBasicInfo(
    std::string web_bundle_id,
    std::string update_manifest_url)
    : web_bundle_id_(std::move(web_bundle_id)),
      update_manifest_url_(std::move(update_manifest_url)) {}

DeviceLocalAccount::DeviceLocalAccount(DeviceLocalAccountType type,
                                       EphemeralMode ephemeral_mode,
                                       const std::string& account_id,
                                       const std::string& kiosk_app_id,
                                       const std::string& kiosk_app_update_url)
    : type(type),
      ephemeral_mode(ephemeral_mode),
      account_id(account_id),
      user_id(GenerateDeviceLocalAccountUserId(account_id, type)),
      kiosk_app_id(kiosk_app_id),
      kiosk_app_update_url(kiosk_app_update_url) {}

DeviceLocalAccount::DeviceLocalAccount(
    EphemeralMode ephemeral_mode,
    const WebKioskAppBasicInfo& web_kiosk_app_info,
    const std::string& account_id)
    : type(DeviceLocalAccountType::kWebKioskApp),
      ephemeral_mode(ephemeral_mode),
      account_id(account_id),
      user_id(GenerateDeviceLocalAccountUserId(account_id, type)),
      web_kiosk_app_info(web_kiosk_app_info) {}

DeviceLocalAccount::DeviceLocalAccount(
    EphemeralMode ephemeral_mode,
    const IsolatedWebAppKioskBasicInfo& kiosk_iwa_info,
    const std::string& account_id)
    : type(DeviceLocalAccountType::kKioskIsolatedWebApp),
      ephemeral_mode(ephemeral_mode),
      account_id(account_id),
      user_id(GenerateDeviceLocalAccountUserId(account_id, type)),
      kiosk_iwa_info(kiosk_iwa_info) {}

DeviceLocalAccount::DeviceLocalAccount(const DeviceLocalAccount& other) =
    default;

DeviceLocalAccount::~DeviceLocalAccount() = default;

std::vector<DeviceLocalAccount> GetDeviceLocalAccounts(
    ash::CrosSettings* cros_settings) {
  // TODO(crbug.com/40636049): handle TYPE_SAML_PUBLIC_SESSION
  std::vector<DeviceLocalAccount> accounts;

  const base::Value::List* list = nullptr;
  if (!cros_settings->GetList(ash::kAccountsPrefDeviceLocalAccounts, &list)) {
    return accounts;
  }

  std::set<std::string> account_ids;
  for (size_t i = 0; i < list->size(); ++i) {
    const base::Value& entry = (*list)[i];
    if (!entry.is_dict()) {
      LOG(ERROR) << "Corrupt entry in device-local account list at index " << i
                 << ".";
      continue;
    }

    const base::Value::Dict& entry_dict = entry.GetDict();
    std::string account_id;
    if (!GetString(entry_dict, ash::kAccountsPrefDeviceLocalAccountsKeyId,
                   &account_id) ||
        account_id.empty()) {
      LOG(ERROR) << "Missing account ID in device-local account list at index "
                 << i << ".";
      continue;
    }

    std::optional<int> raw_type =
        entry_dict.FindInt(ash::kAccountsPrefDeviceLocalAccountsKeyType);
    if (!raw_type || !IsValidDeviceLocalAccountType(*raw_type)) {
      LOG(ERROR) << "Missing or invalid account type in device-local account "
                 << "list at index " << i << ".";
      continue;
    }
    auto type = static_cast<DeviceLocalAccountType>(*raw_type);

    DeviceLocalAccount::EphemeralMode ephemeral_mode_value =
        DeviceLocalAccount::EphemeralMode::kUnset;
    if (IsKioskType(type)) {
      std::optional<int> ephemeral_mode = entry_dict.FindInt(
          ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode);
      if (!ephemeral_mode || ephemeral_mode.value() < 0 ||
          ephemeral_mode.value() >
              static_cast<int>(DeviceLocalAccount::EphemeralMode::kMaxValue)) {
        LOG(ERROR) << "Missing or invalid ephemeral mode (value="
                   << ephemeral_mode.value_or(-1)
                   << ") in device-local account list at index " << i
                   << ", using default kUnset value for ephemeral mode.";
      } else {
        ephemeral_mode_value = static_cast<DeviceLocalAccount::EphemeralMode>(
            ephemeral_mode.value());
      }
    }

    if (!account_ids.insert(account_id).second) {
      LOG(ERROR) << "Duplicate entry in device-local account list at index "
                 << i << ": " << account_id << ".";
      continue;
    }

    switch (type) {
      case DeviceLocalAccountType::kPublicSession:
        accounts.emplace_back(DeviceLocalAccountType::kPublicSession,
                              ephemeral_mode_value, account_id, "", "");
        break;
      case DeviceLocalAccountType::kSamlPublicSession:
        accounts.emplace_back(DeviceLocalAccountType::kSamlPublicSession,
                              ephemeral_mode_value, account_id, "", "");
        break;
      case DeviceLocalAccountType::kKioskApp: {
        std::string kiosk_app_id;
        std::string kiosk_app_update_url;
        if (!GetString(entry_dict,
                       ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                       &kiosk_app_id)) {
          LOG(ERROR) << "Missing app ID in device-local account entry at index "
                     << i << ".";
          continue;
        }
        GetString(entry_dict,
                  ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
                  &kiosk_app_update_url);

        accounts.emplace_back(DeviceLocalAccountType::kKioskApp,
                              ephemeral_mode_value, account_id, kiosk_app_id,
                              kiosk_app_update_url);
        break;
      }
      case DeviceLocalAccountType::kWebKioskApp: {
        std::string url;
        std::string title;
        std::string icon_url;
        if (!GetString(entry_dict,
                       ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
                       &url)) {
          LOG(ERROR) << "Missing install url in Web kiosk type device-local "
                        "account at index "
                     << i << ".";
          continue;
        }

        GetString(entry_dict,
                  ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle,
                  &title);
        GetString(entry_dict,
                  ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl,
                  &icon_url);
        accounts.emplace_back(ephemeral_mode_value,
                              WebKioskAppBasicInfo(url, title, icon_url),
                              account_id);
        break;
      }
      case DeviceLocalAccountType::kKioskIsolatedWebApp: {
        std::string web_bundle_id;
        if (!GetString(entry_dict,
                       ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskBundleId,
                       &web_bundle_id)) {
          LOG(ERROR) << "Missing web bundle ID in IWA kiosk type device-local "
                        "account at index "
                     << i << ".";
          continue;
        }

        std::string update_manifest_url;
        if (!GetString(
                entry_dict,
                ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskUpdateUrl,
                &update_manifest_url)) {
          LOG(ERROR) << "Missing manifest url in IWA kiosk type device-local "
                        "account at index "
                     << i << ".";
          continue;
        }

        accounts.emplace_back(
            ephemeral_mode_value,
            IsolatedWebAppKioskBasicInfo(web_bundle_id, update_manifest_url),
            account_id);
        break;
      }
    }
  }

  return accounts;
}

void SetDeviceLocalAccountsForTesting(
    ash::OwnerSettingsServiceAsh* service,
    const std::vector<DeviceLocalAccount>& accounts) {
  // TODO(crbug.com/40636049): handle TYPE_SAML_PUBLIC_SESSION
  base::Value::List list;
  for (const auto& account : accounts) {
    auto entry =
        base::Value::Dict()
            .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId, account.account_id)
            .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
                 static_cast<int>(account.type))
            .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                 static_cast<int>(account.ephemeral_mode));
    switch (account.type) {
      case DeviceLocalAccountType::kPublicSession:
      case DeviceLocalAccountType::kSamlPublicSession:
        // Do nothing.
        break;
      case DeviceLocalAccountType::kKioskApp:
        entry.Set(ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                  account.kiosk_app_id);
        if (!account.kiosk_app_update_url.empty()) {
          entry.Set(ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
                    account.kiosk_app_update_url);
        }
        break;
      case DeviceLocalAccountType::kWebKioskApp:
        entry.Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
                  account.web_kiosk_app_info.url());
        if (!account.web_kiosk_app_info.title().empty()) {
          entry.Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle,
                    account.web_kiosk_app_info.title());
        }
        if (!account.web_kiosk_app_info.icon_url().empty()) {
          entry.Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl,
                    account.web_kiosk_app_info.icon_url());
        }
        break;
      case DeviceLocalAccountType::kKioskIsolatedWebApp:
        entry.Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskBundleId,
                  account.kiosk_iwa_info.web_bundle_id());
        entry.Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskUpdateUrl,
                  account.kiosk_iwa_info.update_manifest_url());
        break;
    }
    list.Append(std::move(entry));
  }

  service->Set(ash::kAccountsPrefDeviceLocalAccounts,
               base::Value(std::move(list)));
}

}  // namespace policy
