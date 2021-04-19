// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/certificate_matching/certificate_principal_pattern.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/enterprise/util/jni_headers/ManagedBrowserUtils_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chrome {
namespace enterprise_util {

bool HasBrowserPoliciesApplied(Profile* profile) {
  // This profile may have policies configured.
  auto* profile_connector = profile->GetProfilePolicyConnector();
  if (profile_connector->IsManaged())
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This session's primary user may also have policies, and those policies may
  // not have per-profile support.
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user) {
    auto* primary_profile =
        chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
    if (primary_profile) {
      auto* primary_profile_connector =
          primary_profile->GetProfilePolicyConnector();
      if (primary_profile_connector->IsManaged())
        return true;
    }
  }

  // The machine may be enrolled, via Google Cloud or Active Directory.
  auto* browser_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (browser_connector->IsEnterpriseManaged())
    return true;
#else
  // There may be policies set in a platform-specific way (e.g. Windows
  // Registry), or with machine level user cloud policies.
  auto* browser_connector = g_browser_process->browser_policy_connector();
  if (browser_connector->HasMachineLevelPolicies())
    return true;
#endif

  return false;
}

std::string GetDomainFromEmail(const std::string& email) {
  size_t email_separator_pos = email.find('@');
  bool is_email = email_separator_pos != std::string::npos &&
                  email_separator_pos < email.length() - 1;

  if (!is_email)
    return std::string();

  return gaia::ExtractDomainName(email);
}

std::unique_ptr<net::ClientCertIdentity> AutoSelectCertificate(
    Profile* profile,
    const GURL& requesting_url,
    net::ClientCertIdentityList& client_certs) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::Value> setting =
      host_content_settings_map->GetWebsiteSetting(
          requesting_url, requesting_url,
          ContentSettingsType::AUTO_SELECT_CERTIFICATE, nullptr);

  if (!setting || !setting->is_dict())
    return nullptr;

  const base::Value* filters =
      setting->FindKeyOfType("filters", base::Value::Type::LIST);
  if (!filters) {
    // |setting_dict| has the wrong format (e.g. single filter instead of a
    // list of filters). This content setting is only provided by
    // the |PolicyProvider|, which should always set it to a valid format.
    // Therefore, delete the invalid value.
    host_content_settings_map->SetWebsiteSettingDefaultScope(
        requesting_url, requesting_url,
        ContentSettingsType::AUTO_SELECT_CERTIFICATE, nullptr);
    return nullptr;
  }

  for (const base::Value& filter : filters->GetList()) {
    DCHECK(filter.is_dict());

    auto issuer_pattern = certificate_matching::CertificatePrincipalPattern::
        ParseFromOptionalDict(
            filter.FindKeyOfType("ISSUER", base::Value::Type::DICTIONARY), "CN",
            "L", "O", "OU");
    auto subject_pattern = certificate_matching::CertificatePrincipalPattern::
        ParseFromOptionalDict(
            filter.FindKeyOfType("SUBJECT", base::Value::Type::DICTIONARY),
            "CN", "L", "O", "OU");
    // Use the first certificate that is matched by the filter.
    for (auto& client_cert : client_certs) {
      if (issuer_pattern.Matches(client_cert->certificate()->issuer()) &&
          subject_pattern.Matches(client_cert->certificate()->subject())) {
        return std::move(client_cert);
      }
    }
  }

  return nullptr;
}

bool IsMachinePolicyPref(const std::string& pref_name) {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(pref_name);

  return pref && pref->IsManaged();
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kManagedAutoSelectCertificateForUrls);
}

}  // namespace enterprise_util
}  // namespace chrome

#if defined(OS_ANDROID)

// static
jboolean JNI_ManagedBrowserUtils_HasBrowserPoliciesApplied(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profile) {
  return chrome::enterprise_util::HasBrowserPoliciesApplied(
      ProfileAndroid::FromProfileAndroid(profile));
}

#endif  // defined(OS_ANDROID)
