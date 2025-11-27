// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_test_util.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user_test_util {

void AddCustodians(Profile* profile) {
  DCHECK(profile->IsChild());
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kSupervisedUserCustodianEmail,
                   "test_parent_0@google.com");
  prefs->SetString(prefs::kSupervisedUserCustodianObfuscatedGaiaId,
                   "239029320");

  prefs->SetString(prefs::kSupervisedUserSecondCustodianEmail,
                   "test_parent_1@google.com");
  prefs->SetString(prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
                   "85948533");
}

void SetSupervisedUserExtensionsMayRequestPermissionsPref(Profile* profile,
                                                          bool enabled) {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey());
  settings_service->SetLocalSetting(supervised_user::kGeolocationDisabled,
                                    base::Value(!enabled));
  profile->GetPrefs()->SetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions, enabled);

  // Geolocation content setting is also set to the same value. See
  // SupervisedUsePrefStore.
  content_settings::ProviderType provider;
  bool is_geolocation_allowed =
      HostContentSettingsMapFactory::GetForProfile(profile)
          ->GetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                     &provider) == CONTENT_SETTING_ALLOW;
  if (is_geolocation_allowed != enabled) {
    SetSupervisedUserGeolocationEnabledContentSetting(profile, enabled);
  }
}

void SetSkipParentApprovalToInstallExtensionsPref(Profile* profile,
                                                  bool enabled) {
  // TODO(b/324898798): Once the new extension handling mode is releaded, this
  // method replaces `SetSupervisedUserExtensionsMayRequestPermissionsPref` for
  // handling the Extensions behaviour.
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey());
  settings_service->SetLocalSetting(
      supervised_user::kSkipParentApprovalToInstallExtensions,
      base::Value(enabled));
  profile->GetPrefs()->SetBoolean(prefs::kSkipParentApprovalToInstallExtensions,
                                  enabled);
}

void SetSupervisedUserGeolocationEnabledContentSetting(Profile* profile,
                                                       bool enabled) {
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetDefaultContentSetting(
          ContentSettingsType::GEOLOCATION,
          enabled ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (profile->GetPrefs()->GetBoolean(
          prefs::kSupervisedUserExtensionsMayRequestPermissions) != enabled) {
    // Permissions preference is also set to the same value. See
    // SupervisedUsePrefStore.
    SetSupervisedUserExtensionsMayRequestPermissionsPref(profile, enabled);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

AccountInfo PopulateAccountInfoWithName(const AccountInfo& info,
                                        const std::string& given_name) {
  AccountInfo populated_info = AccountInfo::Builder(info)
                                   .SetFullName("fullname")
                                   .SetGivenName(given_name)
                                   .SetHostedDomain("example.com")
                                   .SetAvatarUrl("https://example.com")
                                   .SetLocale("en")
                                   .Build();
  AccountCapabilitiesTestMutator(&populated_info.capabilities)
      .set_is_subject_to_enterprise_features(true);

  CHECK(populated_info.IsValid());
  return populated_info;
}

void SetManualFilterForHost(Profile* profile,
                            std::string_view host,
                            bool allowlist) {
  supervised_user::SupervisedUserTestEnvironment::SetManualFilterForHost(
      host, allowlist,
      *SupervisedUserSettingsServiceFactory::GetForKey(
          profile->GetProfileKey()));
}

void SetManualFilterForUrl(Profile* profile,
                           std::string_view url,
                           bool allowlist) {
  supervised_user::SupervisedUserTestEnvironment::SetManualFilterForUrl(
      url, allowlist,
      *SupervisedUserSettingsServiceFactory::GetForKey(
          profile->GetProfileKey()));
}

void SetWebFilterType(const Profile* profile,
                      supervised_user::WebFilterType web_filter_type) {
  supervised_user::SupervisedUserSettingsService* service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  CHECK(service) << "Missing settings service might indicate misconfigured "
                    "test environment. If this is a unittest, consider using "
                    "SupervisedUserSyncDataFake";
  CHECK(service->IsReady())
      << "If settings service is not ready, the change will not be successful";
  supervised_user::SupervisedUserTestEnvironment::SetWebFilterType(
      web_filter_type, *service);
}

}  // namespace supervised_user_test_util
