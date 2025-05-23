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
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user_test_util {

namespace {
void SetManualFilter(Profile* profile,
                     std::string_view content_pack_setting,
                     std::string_view host,
                     bool allowlist) {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());

  const base::Value::Dict& local_settings =
      settings_service->LocalSettingsForTest();
  base::Value::Dict dict_to_insert;

  if (const base::Value::Dict* dict_value =
          local_settings.FindDict(content_pack_setting)) {
    dict_to_insert = dict_value->Clone();
  }

  dict_to_insert.Set(host, allowlist);
  settings_service->SetLocalSetting(content_pack_setting,
                                    std::move(dict_to_insert));
}
}  // namespace

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

void PopulateAccountInfoWithName(AccountInfo& info,
                                 const std::string& given_name) {
  info.given_name = given_name;
  info.full_name = "fullname";
  info.hosted_domain = "example.com";
  info.locale = "en";
  info.picture_url = "https://example.com";

  CHECK(info.IsValid());
}

void SetManualFilterForHost(Profile* profile,
                            std::string_view host,
                            bool allowlist) {
  SetManualFilter(profile, supervised_user::kContentPackManualBehaviorHosts,
                  host, allowlist);
}

void SetManualFilterForUrl(Profile* profile,
                           std::string_view url,
                           bool allowlist) {
  SetManualFilter(profile, supervised_user::kContentPackManualBehaviorURLs, url,
                  allowlist);
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

  switch (web_filter_type) {
    case supervised_user::WebFilterType::kAllowAllSites:
      service->SetLocalSetting(
          supervised_user::kContentPackDefaultFilteringBehavior,
          base::Value(
              static_cast<int>(supervised_user::FilteringBehavior::kAllow)));
      service->SetLocalSetting(supervised_user::kSafeSitesEnabled,
                               base::Value(false));
      break;
    case supervised_user::WebFilterType::kTryToBlockMatureSites:
      service->SetLocalSetting(
          supervised_user::kContentPackDefaultFilteringBehavior,
          base::Value(
              static_cast<int>(supervised_user::FilteringBehavior::kAllow)));
      service->SetLocalSetting(supervised_user::kSafeSitesEnabled,
                               base::Value(true));
      break;
    case supervised_user::WebFilterType::kCertainSites:
      service->SetLocalSetting(
          supervised_user::kContentPackDefaultFilteringBehavior,
          base::Value(
              static_cast<int>(supervised_user::FilteringBehavior::kBlock)));

      // Value of kSupervisedUserSafeSites is not important here.
      break;
    case supervised_user::WebFilterType::kDisabled:
      NOTREACHED() << "To disable the URL filter, use "
                      "supervised_user::DisableParentalControls(.)";
    case supervised_user::WebFilterType::kMixed:
      NOTREACHED() << "That value is not intended to be set, but is rather "
                      "used to indicate multiple settings used in profiles "
                      "in metrics.";
  }
}

}  // namespace supervised_user_test_util
