// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/status_provider_util.h"

#include "base/values.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "google_apis/gaia/gaia_auth_util.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/off_hours/device_off_hours_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ui/managed_ui.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/enterprise/util/affiliation.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#endif

const char kDevicePolicyStatusDescription[] = "statusDevice";
const char kUserPolicyStatusDescription[] = "statusUser";

void ExtractDomainFromUsername(base::Value::Dict* dict) {
  const std::string* username = dict->FindString("username");
  if (username && !username->empty())
    dict->Set(policy::kDomainKey, gaia::ExtractDomainName(*username));
}

void GetUserAffiliationStatus(base::Value::Dict* dict, Profile* profile) {
  CHECK(profile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return;
  dict->Set("isAffiliated", user->IsAffiliated());
#else
  // Don't show affiliation status if the browser isn't enrolled in CBCM.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile())
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  {
    if (!policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid())
      return;
  }
  dict->Set("isAffiliated",
            chrome::enterprise_util::IsProfileAffiliated(profile));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void SetDomainInUserStatus(base::Value::Dict& user_status) {
  const std::string* username = user_status.FindString(policy::kUsernameKey);
  if (username && !username->empty())
    user_status.Set(policy::kDomainKey, gaia::ExtractDomainName(*username));
}

void SetProfileId(base::Value::Dict* dict, Profile* profile) {
  CHECK(profile);
  auto* profile_id_service =
      enterprise::ProfileIdServiceFactory::GetForProfile(profile);
  if (!profile_id_service)
    return;

  auto profile_id = profile_id_service->GetProfileId();
  if (profile_id)
    dict->Set("profileId", profile_id.value());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void GetOffHoursStatus(base::Value::Dict* dict) {
  policy::off_hours::DeviceOffHoursController* off_hours_controller =
      ash::DeviceSettingsService::Get()->device_off_hours_controller();
  if (off_hours_controller) {
    dict->Set("isOffHoursActive", off_hours_controller->is_off_hours_mode());
  }
}

void GetUserManager(base::Value::Dict* dict, Profile* profile) {
  CHECK(profile);

  absl::optional<std::string> account_manager =
      chrome::GetAccountManagerIdentity(profile);
  if (account_manager) {
    dict->Set(policy::kEnterpriseDomainManagerKey, *account_manager);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
