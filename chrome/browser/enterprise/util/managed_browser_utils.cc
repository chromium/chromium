// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_matching/certificate_principal_pattern.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/enterprise/util/jni_headers/ManagedBrowserUtils_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/managed_ui.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chrome {
namespace enterprise_util {

namespace {

// Returns client certificate auto-selection filters configured for the given
// URL in |ContentSettingsType::AUTO_SELECT_CERTIFICATE| content setting. The
// format of the returned filters corresponds to the "filter" property of the
// AutoSelectCertificateForUrls policy as documented at policy_templates.json.
base::Value::List GetCertAutoSelectionFilters(Profile* profile,
                                              const GURL& requesting_url) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value setting = host_content_settings_map->GetWebsiteSetting(
      requesting_url, requesting_url,
      ContentSettingsType::AUTO_SELECT_CERTIFICATE, nullptr);

  if (!setting.is_dict())
    return {};

  base::Value::List* filters = setting.GetDict().FindList("filters");
  if (!filters) {
    // |setting_dict| has the wrong format (e.g. single filter instead of a
    // list of filters). This content setting is only provided by
    // the |PolicyProvider|, which should always set it to a valid format.
    // Therefore, delete the invalid value.
    host_content_settings_map->SetWebsiteSettingDefaultScope(
        requesting_url, requesting_url,
        ContentSettingsType::AUTO_SELECT_CERTIFICATE, base::Value());
    return {};
  }
  return std::move(*filters);
}

// Returns whether the client certificate matches any of the auto-selection
// filters. Returns false when there's no valid filter.
bool CertMatchesSelectionFilters(
    const net::ClientCertIdentity& client_cert,
    const base::Value::List& auto_selection_filters) {
  for (const auto& filter : auto_selection_filters) {
    if (!filter.is_dict()) {
      // The filter has a wrong format, so ignore it. Note that reporting of
      // schema violations, like this, to UI is already implemented in the
      // policy handler - see configuration_policy_handler_list_factory.cc.
      continue;
    }
    auto issuer_pattern = certificate_matching::CertificatePrincipalPattern::
        ParseFromOptionalDict(filter.GetDict().FindDict("ISSUER"), "CN", "L",
                              "O", "OU");
    auto subject_pattern = certificate_matching::CertificatePrincipalPattern::
        ParseFromOptionalDict(filter.GetDict().FindDict("SUBJECT"), "CN", "L",
                              "O", "OU");

    if (issuer_pattern.Matches(client_cert.certificate()->issuer()) &&
        subject_pattern.Matches(client_cert.certificate()->subject())) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool IsBrowserManaged(Profile* profile) {
  DCHECK(profile);

  if (base::FeatureList::IsEnabled(features::kUseManagementService)) {
    return policy::ManagementServiceFactory::GetForProfile(profile)
        ->IsManaged();
  }

  // This profile may have policies configured.
  auto* profile_connector = profile->GetProfilePolicyConnector();
  if (profile_connector && profile_connector->IsManaged())
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This session's primary user may also have policies, and those policies may
  // not have per-profile support.
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user) {
    auto* primary_profile =
        ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
    if (primary_profile) {
      auto* primary_profile_connector =
          primary_profile->GetProfilePolicyConnector();
      if (primary_profile_connector->IsManaged())
        return true;
    }
  }

  // The machine may be enrolled, via Google Cloud or Active Directory.
  auto* browser_connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return browser_connector && browser_connector->IsDeviceEnterpriseManaged();
#else
  // There may be policies set in a platform-specific way (e.g. Windows
  // Registry), or with machine level user cloud policies.
  auto* browser_connector = g_browser_process->browser_policy_connector();
  return browser_connector && browser_connector->HasMachineLevelPolicies();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

std::string GetDomainFromEmail(const std::string& email) {
  size_t email_separator_pos = email.find('@');
  bool is_email = email_separator_pos != std::string::npos &&
                  email_separator_pos < email.length() - 1;

  if (!is_email)
    return std::string();

  return gaia::ExtractDomainName(email);
}

void AutoSelectCertificates(
    Profile* profile,
    const GURL& requesting_url,
    net::ClientCertIdentityList client_certs,
    net::ClientCertIdentityList* matching_client_certs,
    net::ClientCertIdentityList* nonmatching_client_certs) {
  matching_client_certs->clear();
  nonmatching_client_certs->clear();
  const base::Value::List auto_selection_filters =
      GetCertAutoSelectionFilters(profile, requesting_url);
  for (auto& client_cert : client_certs) {
    if (CertMatchesSelectionFilters(*client_cert, auto_selection_filters))
      matching_client_certs->push_back(std::move(client_cert));
    else
      nonmatching_client_certs->push_back(std::move(client_cert));
  }
}

bool IsMachinePolicyPref(const std::string& pref_name) {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(pref_name);

  return pref && pref->IsManaged();
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kManagedAutoSelectCertificateForUrls);
}

void SetUserAcceptedAccountManagement(Profile* profile, bool accepted) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager())
    return;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry)
    entry->SetUserAcceptedAccountManagement(accepted);
}

bool UserAcceptedAccountManagement(Profile* profile) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager())
    return false;
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->UserAcceptedAccountManagement();
}

bool ProfileCanBeManaged(Profile* profile) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager())
    return false;
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->CanBeManaged();
}

#if BUILDFLAG(IS_ANDROID)

std::string GetBrowserManagerName(Profile* profile) {
  DCHECK(profile);

  // @TODO(https://crbug.com/1227786): There are some use-cases where the
  // expected behavior of chrome://management is to show more than one domain.
  absl::optional<std::string> manager = GetAccountManagerIdentity(profile);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = GetDeviceManagerIdentity();
  }
  return manager.value_or(std::string());
}

// static
jboolean JNI_ManagedBrowserUtils_IsBrowserManaged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profile) {
  return IsBrowserManaged(ProfileAndroid::FromProfileAndroid(profile));
}

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_ManagedBrowserUtils_GetBrowserManagerName(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profile) {
  return base::android::ConvertUTF8ToJavaString(
      env, GetBrowserManagerName(ProfileAndroid::FromProfileAndroid(profile)));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace enterprise_util
}  // namespace chrome
