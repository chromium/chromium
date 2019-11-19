// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace policy {

namespace {

const char kPublicAccountDomainPrefix[] = "public-accounts";
const char kKioskAppAccountDomainPrefix[] = "kiosk-apps";
const char kArcKioskAppAccountDomainPrefix[] = "arc-kiosk-apps";
const char kSAMLPublicAccountDomainPrefix[] = "saml-public-accounts";
const char kWebKioskAppAccountDomainPrefix[] = "web-kiosk-apps";

const char kDeviceLocalAccountDomainSuffix[] = ".device-local.localhost";

}  // namespace

ArcKioskAppBasicInfo::ArcKioskAppBasicInfo(const std::string& package_name,
                                           const std::string& class_name,
                                           const std::string& action,
                                           const std::string& display_name)
    : package_name_(package_name),
      class_name_(class_name),
      action_(action),
      display_name_(display_name) {}

ArcKioskAppBasicInfo::ArcKioskAppBasicInfo(const ArcKioskAppBasicInfo& other) =
    default;

ArcKioskAppBasicInfo::ArcKioskAppBasicInfo() {}

ArcKioskAppBasicInfo::~ArcKioskAppBasicInfo() {}

bool ArcKioskAppBasicInfo::operator==(const ArcKioskAppBasicInfo& other) const {
  return this->package_name_ == other.package_name_ &&
         this->action_ == other.action_ &&
         this->class_name_ == other.class_name_ &&
         this->display_name_ == other.display_name_;
}

WebKioskAppBasicInfo::WebKioskAppBasicInfo(const std::string& url)
    : url_(url) {}

WebKioskAppBasicInfo::WebKioskAppBasicInfo() {}

WebKioskAppBasicInfo::~WebKioskAppBasicInfo() {}

DeviceLocalAccount::DeviceLocalAccount(Type type,
                                       const std::string& account_id,
                                       const std::string& kiosk_app_id,
                                       const std::string& kiosk_app_update_url)
    : type(type),
      account_id(account_id),
      user_id(GenerateDeviceLocalAccountUserId(account_id, type)),
      kiosk_app_id(kiosk_app_id),
      kiosk_app_update_url(kiosk_app_update_url) {
}

DeviceLocalAccount::DeviceLocalAccount(
    const ArcKioskAppBasicInfo& arc_kiosk_app_info,
    const std::string& account_id)
    : type(DeviceLocalAccount::TYPE_ARC_KIOSK_APP),
      account_id(account_id),
      user_id(GenerateDeviceLocalAccountUserId(account_id, type)),
      arc_kiosk_app_info(arc_kiosk_app_info) {}

DeviceLocalAccount::DeviceLocalAccount(
    const WebKioskAppBasicInfo& web_kiosk_app_info,
    const std::string& account_id)
    : type(DeviceLocalAccount::TYPE_WEB_KIOSK_APP),
      account_id(account_id),
      user_id(GenerateDeviceLocalAccountUserId(account_id, type)),
      web_kiosk_app_info(web_kiosk_app_info) {}

DeviceLocalAccount::DeviceLocalAccount(const DeviceLocalAccount& other) =
    default;

DeviceLocalAccount::~DeviceLocalAccount() {
}

std::string GenerateDeviceLocalAccountUserId(const std::string& account_id,
                                             DeviceLocalAccount::Type type) {
  std::string domain_prefix;
  switch (type) {
    case DeviceLocalAccount::TYPE_PUBLIC_SESSION:
      domain_prefix = kPublicAccountDomainPrefix;
      break;
    case DeviceLocalAccount::TYPE_KIOSK_APP:
      domain_prefix = kKioskAppAccountDomainPrefix;
      break;
    case DeviceLocalAccount::TYPE_ARC_KIOSK_APP:
      domain_prefix = kArcKioskAppAccountDomainPrefix;
      break;
    case DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION:
      domain_prefix = kSAMLPublicAccountDomainPrefix;
      break;
    case DeviceLocalAccount::TYPE_WEB_KIOSK_APP:
      domain_prefix = kWebKioskAppAccountDomainPrefix;
      break;
    case DeviceLocalAccount::TYPE_COUNT:
      NOTREACHED();
      break;
  }
  return gaia::CanonicalizeEmail(
      base::HexEncode(account_id.c_str(), account_id.size()) + "@" +
      domain_prefix + kDeviceLocalAccountDomainSuffix);
}

bool IsDeviceLocalAccountUser(const std::string& user_id,
                              DeviceLocalAccount::Type* type) {
  // For historical reasons, the guest user ID does not contain an @ symbol and
  // therefore, cannot be parsed by gaia::ExtractDomainName().
  if (user_id == user_manager::GuestAccountId().GetUserEmail())
    return false;
  const std::string domain = gaia::ExtractDomainName(user_id);
  if (!base::EndsWith(domain, kDeviceLocalAccountDomainSuffix,
                      base::CompareCase::SENSITIVE))
    return false;

  const std::string domain_prefix = domain.substr(
      0, domain.size() - base::size(kDeviceLocalAccountDomainSuffix) + 1);

  if (domain_prefix == kPublicAccountDomainPrefix) {
    if (type)
      *type = DeviceLocalAccount::TYPE_PUBLIC_SESSION;
    return true;
  }
  if (domain_prefix == kKioskAppAccountDomainPrefix) {
    if (type)
      *type = DeviceLocalAccount::TYPE_KIOSK_APP;
    return true;
  }
  if (domain_prefix == kArcKioskAppAccountDomainPrefix) {
    if (type)
      *type = DeviceLocalAccount::TYPE_ARC_KIOSK_APP;
    return true;
  }
  if (domain_prefix == kSAMLPublicAccountDomainPrefix) {
    if (type)
      *type = DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION;
    return true;
  }
  if (domain_prefix == kWebKioskAppAccountDomainPrefix) {
    if (type)
      *type = DeviceLocalAccount::TYPE_WEB_KIOSK_APP;
    return true;
  }

  // |user_id| is a device-local account but its type is not recognized.
  NOTREACHED();
  if (type)
    *type = DeviceLocalAccount::TYPE_COUNT;
  return true;
}

void SetDeviceLocalAccounts(chromeos::OwnerSettingsServiceChromeOS* service,
                            const std::vector<DeviceLocalAccount>& accounts) {
  // TODO(https://crbug.com/984021): handle TYPE_SAML_PUBLIC_SESSION
  base::ListValue list;
  for (std::vector<DeviceLocalAccount>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    entry->SetKey(chromeos::kAccountsPrefDeviceLocalAccountsKeyId,
                  base::Value(it->account_id));
    entry->SetKey(chromeos::kAccountsPrefDeviceLocalAccountsKeyType,
                  base::Value(it->type));
    if (it->type == DeviceLocalAccount::TYPE_KIOSK_APP) {
      entry->SetKey(chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                    base::Value(it->kiosk_app_id));
      if (!it->kiosk_app_update_url.empty()) {
        entry->SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
            base::Value(it->kiosk_app_update_url));
      }
    } else if (it->type == DeviceLocalAccount::TYPE_ARC_KIOSK_APP) {
      entry->SetKey(
          chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage,
          base::Value(it->arc_kiosk_app_info.package_name()));
      if (!it->arc_kiosk_app_info.class_name().empty()) {
        entry->SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskClass,
            base::Value(it->arc_kiosk_app_info.class_name()));
      }
      if (!it->arc_kiosk_app_info.action().empty()) {
        entry->SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskAction,
            base::Value(it->arc_kiosk_app_info.action()));
      }
      if (!it->arc_kiosk_app_info.display_name().empty()) {
        entry->SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName,
            base::Value(it->arc_kiosk_app_info.display_name()));
      }
    } else if (it->type == DeviceLocalAccount::TYPE_WEB_KIOSK_APP) {
      entry->SetKey(chromeos::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
                    base::Value(it->web_kiosk_app_info.url()));
    }
    list.Append(std::move(entry));
  }

  service->Set(chromeos::kAccountsPrefDeviceLocalAccounts, list);
}

std::vector<DeviceLocalAccount> GetDeviceLocalAccounts(
    chromeos::CrosSettings* cros_settings) {
  // TODO(https://crbug.com/984021): handle TYPE_SAML_PUBLIC_SESSION
  std::vector<DeviceLocalAccount> accounts;

  const base::ListValue* list = NULL;
  cros_settings->GetList(chromeos::kAccountsPrefDeviceLocalAccounts, &list);
  if (!list)
    return accounts;

  std::set<std::string> account_ids;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* entry = NULL;
    if (!list->GetDictionary(i, &entry)) {
      LOG(ERROR) << "Corrupt entry in device-local account list at index " << i
                 << ".";
      continue;
    }

    std::string account_id;
    if (!entry->GetStringWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyId, &account_id) ||
        account_id.empty()) {
      LOG(ERROR) << "Missing account ID in device-local account list at index "
                 << i << ".";
      continue;
    }

    int type;
    if (!entry->GetIntegerWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyType, &type) ||
        type < 0 || type >= DeviceLocalAccount::TYPE_COUNT) {
      LOG(ERROR) << "Missing or invalid account type in device-local account "
                 << "list at index " << i << ".";
      continue;
    }

    if (!account_ids.insert(account_id).second) {
      LOG(ERROR) << "Duplicate entry in device-local account list at index "
                 << i << ": " << account_id << ".";
      continue;
    }

    switch (type) {
      case DeviceLocalAccount::TYPE_PUBLIC_SESSION:
        accounts.push_back(DeviceLocalAccount(
            DeviceLocalAccount::TYPE_PUBLIC_SESSION, account_id, "", ""));
        break;
      case DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION:
        accounts.push_back(DeviceLocalAccount(
            DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION, account_id, "", ""));
        break;
      case DeviceLocalAccount::TYPE_KIOSK_APP: {
        std::string kiosk_app_id;
        std::string kiosk_app_update_url;
        if (!entry->GetStringWithoutPathExpansion(
                chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                &kiosk_app_id)) {
          LOG(ERROR) << "Missing app ID in device-local account entry at index "
                     << i << ".";
          continue;
        }
        entry->GetStringWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
            &kiosk_app_update_url);

        accounts.push_back(
            DeviceLocalAccount(DeviceLocalAccount::TYPE_KIOSK_APP, account_id,
                               kiosk_app_id, kiosk_app_update_url));
        break;
      }
      case DeviceLocalAccount::TYPE_ARC_KIOSK_APP: {
        std::string package_name;
        std::string class_name;
        std::string action;
        std::string display_name;
        if (!entry->GetStringWithoutPathExpansion(
                chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage,
                &package_name)) {
          LOG(ERROR) << "Missing package name in ARC kiosk type device-local "
                        "account at index "
                     << i << ".";
          continue;
        }
        entry->GetStringWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskClass,
            &class_name);
        entry->GetStringWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskAction,
            &action);
        entry->GetStringWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName,
            &display_name);
        const ArcKioskAppBasicInfo arc_kiosk_app(package_name, class_name,
                                                 action, display_name);

        accounts.push_back(DeviceLocalAccount(arc_kiosk_app, account_id));
        break;
      }
      case DeviceLocalAccount::TYPE_WEB_KIOSK_APP: {
        std::string url;
        if (!entry->GetStringWithoutPathExpansion(
                chromeos::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
                &url)) {
          LOG(ERROR) << "Missing install url in Web kiosk type device-local "
                        "account at index "
                     << i << ".";
          continue;
        }

        accounts.push_back(
            DeviceLocalAccount(WebKioskAppBasicInfo(url), account_id));
        break;
      }
      default:
        NOTREACHED();
    }
  }
  return accounts;
}

}  // namespace policy
