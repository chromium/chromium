// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_matching/certificate_principal_pattern.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/host_port_pair.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/enterprise/util/jni_headers/ManagedBrowserUtils_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

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

void OnManagementIconReceived(
    base::OnceCallback<void(const gfx::Image&)> callback,
    const gfx::Image& icon,
    const image_fetcher::RequestMetadata& metadata) {
  if (icon.IsEmpty()) {
    LOG(WARNING) << "EnterpriseLogoUrl fetch failed with error code "
                 << metadata.http_response_code << " and MIME type "
                 << metadata.mime_type;
  }
  std::move(callback).Run(icon);
}

}  // namespace

bool IsBrowserManaged(Profile* profile) {
  DCHECK(profile);
  return policy::ManagementServiceFactory::GetForProfile(profile)->IsManaged();
}

std::string GetDomainFromEmail(const std::string& email) {
  size_t email_separator_pos = email.find('@');
  bool is_email = email_separator_pos != std::string::npos &&
                  email_separator_pos < email.length() - 1;

  if (!is_email)
    return std::string();

  return gaia::ExtractDomainName(email);
}

GURL GetRequestingUrl(const net::HostPortPair host_port_pair) {
  return GURL("https://" + host_port_pair.ToString());
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
  if (!g_browser_process->profile_manager() || !profile) {
    return false;
  }
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->CanBeManaged();
}

ManagementEnvironment GetManagementEnvironment(
    Profile* profile,
    const AccountInfo& account_info) {
  if (!UserAcceptedAccountManagement(profile)) {
    return ManagementEnvironment::kNone;
  }
  return account_info.IsEduAccount() ? ManagementEnvironment::kSchool
                                     : ManagementEnvironment::kWork;
}

bool IsEnterpriseBadgingEnabledForToolbar(Profile* profile) {
  return profile->GetPrefs()->GetInteger(
             prefs::kEnterpriseProfileBadgeToolbarSettings) == 0;
}

bool CanShowEnterpriseBadgingForMenu(Profile* profile) {
  if (!UserAcceptedAccountManagement(profile)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kEnterpriseProfileBadgingForMenu)) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(
          features::kEnterpriseProfileBadgingPolicies)) {
    return false;
  }

  return !profile->GetPrefs()
              ->GetString(prefs::kEnterpriseLogoUrlForProfile)
              .empty();
}

bool CanShowEnterpriseBadgingForAvatar(Profile* profile) {
  if (!UserAcceptedAccountManagement(profile)) {
    return false;
  }
  if (!IsEnterpriseBadgingEnabledForToolbar(profile)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kEnterpriseProfileBadgingForAvatar)) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(
          features::kEnterpriseProfileBadgingPolicies)) {
    return false;
  }

  return !profile->GetPrefs()
              ->GetString(prefs::kEnterpriseCustomLabelForProfile)
              .empty();
}

bool IsKnownConsumerDomain(const std::string& email_domain) {
  return !signin::AccountManagedStatusFinder::MayBeEnterpriseDomain(
      email_domain);
}

#if BUILDFLAG(IS_ANDROID)

// static
jboolean JNI_ManagedBrowserUtils_IsBrowserManaged(JNIEnv* env,
                                                  Profile* profile) {
  return policy::ManagementServiceFactory::GetForProfile(profile)
      ->IsBrowserManaged();
}

// static
jboolean JNI_ManagedBrowserUtils_IsProfileManaged(JNIEnv* env,
                                                  Profile* profile) {
  return policy::ManagementServiceFactory::GetForProfile(profile)
      ->IsAccountManaged();
}

// static
base::android::ScopedJavaLocalRef<jstring> JNI_ManagedBrowserUtils_GetTitle(
    JNIEnv* env,
    Profile* profile) {
  return base::android::ConvertUTF16ToJavaString(
      env, chrome::GetManagementPageSubtitle(profile));
}

// static
jboolean JNI_ManagedBrowserUtils_IsBrowserReportingEnabled(JNIEnv* env) {
  return g_browser_process->local_state()->GetBoolean(
      enterprise_reporting::kCloudReportingEnabled);
}

// static
jboolean JNI_ManagedBrowserUtils_IsProfileReportingEnabled(JNIEnv* env,
                                                           Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      enterprise_reporting::kCloudProfileReportingEnabled);
}

#endif  // BUILDFLAG(IS_ANDROID)
void GetManagementIcon(const GURL& url,
                       Profile* profile,
                       base::OnceCallback<void(const gfx::Image&)> callback) {
  constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation("enterprise_logo_fetcher",
                                          R"(
        semantics {
          sender: "Chrome Profiles"
          description:
            "Retrieves an image set by the admin as the enterprise logo. This "
            "is used to show the user which organization manages their browser "
            "in the profile menu."
          trigger:
            "When the user launches the browser and the EnterpriseLogoUrl "
            "policy is set."
          data:
            "An admin-controlled URL for an image on the profile menu."
          destination: OTHER
          internal {
            contacts {
              email: "cbe-magic@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
          }
          last_reviewed: "2024-07-22"
        }
        policy {
          cookies_allowed: NO
          setting:
            "There is no setting. This fetch is enabled for any managed user "
            "with the EnterpriseLogoUrl policy set."
          chrome_policy {
            EnterpriseLogoUrl {
              EnterpriseLogoUrl: ""
            }
          }
        })");

  if (!url.is_valid()) {
    std::move(callback).Run(gfx::Image());
    return;
  }
  image_fetcher::ImageFetcher* fetcher =
      ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey())
          ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
  fetcher->FetchImage(
      url, base::BindOnce(&OnManagementIconReceived, std::move(callback)),
      image_fetcher::ImageFetcherParams(
          kTrafficAnnotation,
          /*uma_client_name=*/"BrowserManagementMetadata"));
}

}  // namespace enterprise_util
