// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/certificate_matching/auto_select_certificates.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/host_port_pair.h"
#include "net/ssl/client_cert_identity.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_prefs.h"
#include "google_apis/gaia/gaia_id.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/enterprise/util/jni_headers/ManagedBrowserUtils_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace enterprise_util {

// Enterprise custom labels have a limit of 16 characters, so they will be cut
// at the 17th characters.
constexpr int kMaximumEnterpriseCustomLabelLengthCutOff = 17;

namespace {

// Returns client certificate auto-selection filters configured for the given
// URL in |ContentSettingsType::AUTO_SELECT_CERTIFICATE| content setting. The
// format of the returned filters corresponds to the "filter" property of the
// AutoSelectCertificateForUrls policy as documented at policy_templates.json.
base::ListValue GetCertAutoSelectionFilters(Profile* profile,
                                            const GURL& requesting_url) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value setting = host_content_settings_map->GetWebsiteSetting(
      requesting_url, requesting_url,
      ContentSettingsType::AUTO_SELECT_CERTIFICATE, nullptr);

  if (!setting.is_dict()) {
    return {};
  }

  base::ListValue* filters = setting.GetDict().FindList("filters");
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

// Expected to be called when Management is set and enterprise badging is
// enabled. Returns:
// - true for Work.
// - false for School.
bool IsManagementWork(Profile* profile) {
  CHECK(enterprise_util::CanShowEnterpriseBadgingForAvatar(profile));
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto management_environment = enterprise_util::GetManagementEnvironment(
      profile, identity_manager->FindExtendedAccountInfoByAccountId(
                   identity_manager->GetPrimaryAccountId(
                       signin::ConsentLevel::kSignin)));
  CHECK_NE(management_environment,
           enterprise_util::ManagementEnvironment::kNone);
  return management_environment ==
         enterprise_util::ManagementEnvironment::kWork;
}

// Helper function to get the correct annotation based on the policy name.
net::NetworkTrafficAnnotationTag GetTrafficAnnotationForPolicy(
    EnterpriseLogoUrlScope url_scope) {
  switch (url_scope) {
    case EnterpriseLogoUrlScope::kBrowser: {
      return net::DefineNetworkTrafficAnnotation(
          "enterprise_logo_fetcher_for_browser",
          R"(
        semantics {
          sender: "Browser Management Service"
          description:
            "Retrieves an image set by the admin as the enterprise logo. This "
            "is used to show the user which organization manages their browser "
            "in the new tab page footer."
          trigger:
            "When the user launches the browser and the "
            "EnterpriseLogoUrlForBrowser policy is set."
          data:
            "An admin-controlled URL for an image in the managed browser "
            "footer."
          destination: OTHER
          internal {
            contacts {
              email: "cec-growth@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
          }
          last_reviewed: "2025-08-22"
        }
        policy {
          cookies_allowed: NO
          setting:
            "There is no setting. This fetch is enabled for any managed browser "
            "with the EnterpriseLogoUrlForBrowser policy set."
          chrome_policy {
            EnterpriseLogoUrlForBrowser {
              EnterpriseLogoUrlForBrowser: ""
            }
          }
        })");
    }
    case EnterpriseLogoUrlScope::kProfile:
      return net::DefineNetworkTrafficAnnotation("enterprise_logo_fetcher",
                                                 R"(
        semantics {
          sender: "Browser Management Service"
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
              email: "cec-growth@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
          }
          last_reviewed: "2025-08-22"
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
  }
}
}  // namespace

bool IsBrowserManaged(Profile* profile) {
  CHECK(profile, base::NotFatalUntil::M161);
  return policy::ManagementServiceFactory::GetForProfile(profile)->IsManaged();
}

std::string GetDomainFromEmail(std::string_view email) {
  size_t email_separator_pos = email.find('@');
  bool is_email = email_separator_pos != std::string_view::npos &&
                  email_separator_pos < email.length() - 1;

  if (!is_email) {
    return std::string();
  }

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
  const base::ListValue auto_selection_filters =
      GetCertAutoSelectionFilters(profile, requesting_url);
  certificate_matching::AutoSelectCertificates(
      auto_selection_filters, std::move(client_certs), matching_client_certs,
      nonmatching_client_certs);
}

bool IsMachinePolicyPref(const std::string& pref_name) {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(pref_name);

  return pref && pref->IsManaged();
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kManagedAutoSelectCertificateForUrls);
}

#if BUILDFLAG(IS_ANDROID)
void SetUserAcceptedAccountManagementAndroid(Profile* profile,
                                             const GaiaId& gaia_id,
                                             bool accepted) {
  CHECK(profile);
  CHECK(!gaia_id.empty());
  CHECK(!profile->IsOffTheRecord());
  // The management notice on Android also asks the user for consent to share
  // device signals, so both are backed by the same per-account bit.
  SigninPrefs(*profile->GetPrefs())
      .SetDeviceSignalsPermanentConsentReceived(gaia_id, accepted);
}

bool UserAcceptedAccountManagementAndroid(Profile* profile,
                                          const GaiaId& gaia_id) {
  CHECK(profile);
  CHECK(!gaia_id.empty());
  CHECK(!profile->IsOffTheRecord());
  return SigninPrefs(*profile->GetPrefs())
      .GetDeviceSignalsPermanentConsentReceived(gaia_id);
}
#endif  // BUILDFLAG(IS_ANDROID)

void SetUserAcceptedAccountManagement(Profile* profile, bool accepted) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager()) {
    return;
  }
  // The updated consent screen also ask the user for consent to share device
  // signals.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  profile->GetPrefs()->SetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived, accepted);
#endif
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    SetEnterpriseProfileLabel(profile);
#endif
    entry->SetUserAcceptedAccountManagement(accepted);
  }
}

bool UserAcceptedAccountManagement(Profile* profile) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager()) {
    return false;
  }
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->UserAcceptedAccountManagement();
}

void SetEnterpriseProfileLabel(Profile* profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    // Can be null in tests.
    return;
  }
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    return;
  }
  std::string label = IsEnterpriseBadgingEnabledForToolbar(profile)
                          ? profile->GetPrefs()->GetString(
                                prefs::kEnterpriseCustomLabelForProfile)
                          : std::string();
  entry->SetEnterpriseProfileLabel(base::UTF8ToUTF16(label));
}

bool ProfileCanBeManaged(Profile* profile) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager() || !profile) {
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/553341908): During feature flag cleanup, change #endif to
  // #else.
  if (base::FeatureList::IsEnabled(
          chrome::android::kAndroidNewManagementNotice)) {
    // If the new management notice is enabled, the acceptance bit is stored
    // in `SigninPrefs` so this implementation bypasses the profile attribute
    // storage.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    if (!identity_manager) {
      return false;
    }

    // `HasPrimaryAccount()` only guarantees a non-empty account id; guard
    // against an empty Gaia ID since the Android helpers `CHECK()` on it.
    const GaiaId gaia_id =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .gaia;
    if (gaia_id.empty()) {
      return false;
    }

    return UserAcceptedAccountManagementAndroid(profile, gaia_id);
  }
#endif  // BUILDFLAG(IS_ANDROID)

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
  return CanShowEnterpriseProfileUI(profile) || profile->IsChild();
}

bool CanShowEnterpriseBadgingForAvatar(Profile* profile) {
  return CanShowEnterpriseProfileUI(profile) &&
         IsEnterpriseBadgingEnabledForToolbar(profile);
}

bool CanShowEnterpriseProfileUI(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return false;
  }
  if (!UserAcceptedAccountManagement(profile) ||
      !policy::ManagementServiceFactory::GetForProfile(profile)
           ->IsAccountManaged()) {
    return false;
  }
  return true;
}

bool CanShowEnterpriseBadgingForNTPFooter(Profile* profile) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  BrowserManagementNoticeState management_notice_state =
      GetManagementNoticeStateForNTPFooter(profile);
  switch (management_notice_state) {
    case BrowserManagementNoticeState::kNotApplicable:
      return false;
    case BrowserManagementNoticeState::kEnabled:
    case BrowserManagementNoticeState::kDisabled:
    case BrowserManagementNoticeState::kEnabledByPolicy:
      return true;
  }
#else
  return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

BrowserManagementNoticeState GetManagementNoticeStateForNTPFooter(
    Profile* profile) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  if (!management_service->IsBrowserManaged() ||
      !g_browser_process->local_state()->GetBoolean(
          prefs::kNTPFooterManagementNoticeEnabled)) {
    return BrowserManagementNoticeState::kNotApplicable;
  }

  bool has_custom_badging =
      !g_browser_process->local_state()
           ->GetString(prefs::kEnterpriseCustomLabelForBrowser)
           .empty() ||
      !g_browser_process->local_state()
           ->GetString(prefs::kEnterpriseLogoUrlForBrowser)
           .empty();
  if (has_custom_badging &&
      base::FeatureList::IsEnabled(features::kNTPFooterBadgingPolicies)) {
    return BrowserManagementNoticeState::kEnabledByPolicy;
  }

  const bool is_low_trust =
      management_service->GetManagementAuthorityTrustworthiness() <=
      policy::ManagementAuthorityTrustworthiness::LOW;

  if (!is_low_trust) {
    return profile->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible)
               ? BrowserManagementNoticeState::kEnabled
               : BrowserManagementNoticeState::kDisabled;
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return BrowserManagementNoticeState::kNotApplicable;
}

bool IsKnownConsumerDomain(const std::string& email_domain) {
  return !signin::AccountManagedStatusFinder::MayBeEnterpriseDomain(
      email_domain);
}

#if BUILDFLAG(IS_ANDROID)

// static
static bool JNI_ManagedBrowserUtils_IsBrowserManaged(JNIEnv* env,
                                                     Profile* profile) {
  return policy::ManagementServiceFactory::GetForProfile(profile)
      ->IsBrowserManaged();
}

// static
static bool JNI_ManagedBrowserUtils_IsProfileManaged(JNIEnv* env,
                                                     Profile* profile) {
  return policy::ManagementServiceFactory::GetForProfile(profile)
      ->IsAccountManaged();
}

// static
static std::u16string JNI_ManagedBrowserUtils_GetTitle(JNIEnv* env,
                                                       Profile* profile) {
  return GetManagementPageSubtitle(profile);
}

// static
static bool JNI_ManagedBrowserUtils_IsProfileReportingEnabled(
    JNIEnv* env,
    Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      enterprise_reporting::kCloudProfileReportingEnabled);
}

// static
static bool JNI_ManagedBrowserUtils_IsOnSecurityEventEnterpriseConnectorEnabled(
    JNIEnv* env,
    Profile* profile) {
  CHECK(profile, base::NotFatalUntil::M161);

  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  if (!service) {
    return false;
  }

  return !service->GetReportingServiceProviderNames().empty();
}

// static
static bool JNI_ManagedBrowserUtils_IsEnterpriseRealTimeUrlCheckModeEnabled(
    JNIEnv* env,
    Profile* profile) {
  CHECK(profile, base::NotFatalUntil::M161);

  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  if (!service) {
    return false;
  }

  return service->GetAppliedRealTimeUrlCheck() !=
         enterprise_connectors::REAL_TIME_CHECK_DISABLED;
}

// static
static bool
JNI_ManagedBrowserUtils_IsOnFileDownloadedEnterpriseConnectorEnabled(
    JNIEnv* env,
    Profile* profile) {
  CHECK(profile, base::NotFatalUntil::M161);

  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);

  return service &&
         !service
              ->GetAnalysisServiceProviderNames(
                  enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED)
              .empty();
}

#endif  // BUILDFLAG(IS_ANDROID)

void GetManagementIcon(const GURL& url,
                       Profile* profile,
                       EnterpriseLogoUrlScope url_scope,
                       base::OnceCallback<void(const gfx::Image&)> callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      GetTrafficAnnotationForPolicy(url_scope);

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
          traffic_annotation,
          /*uma_client_name=*/"BrowserManagementMetadata"));
}

std::u16string GetEnterpriseLabel(Profile* profile, bool truncated) {
  if (!CanShowEnterpriseBadgingForAvatar(profile)) {
    return std::u16string();
  }
  const std::string enterprise_custom_label =
      profile->GetPrefs()->GetString(prefs::kEnterpriseCustomLabelForProfile);
  if (!enterprise_custom_label.empty()) {
    return truncated
               ? gfx::TruncateString(base::UTF8ToUTF16(enterprise_custom_label),
                                     kMaximumEnterpriseCustomLabelLengthCutOff,
                                     gfx::CHARACTER_BREAK)
               : base::UTF8ToUTF16(enterprise_custom_label);
  } else if (IsManagementWork(profile)) {
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_WORK);
  } else {
    // School.
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SCHOOL);
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
base::ScopedClosureRunner DisableAutomaticManagementDisclaimerUntilReset(
    Profile* profile) {
  auto* disclaimer_service =
      ProfileManagementDisclaimerServiceFactory::GetForProfile(profile);
  if (!disclaimer_service) {
    return base::ScopedClosureRunner(base::DoNothing());
  }
  return disclaimer_service->DisableManagementDisclaimerUntilReset();
}

base::ScopedClosureRunner
EnabledAutomaticManagementDisclaimerAcceptanceUntilReset(Profile* profile) {
  auto* disclaimer_service =
      ProfileManagementDisclaimerServiceFactory::GetForProfile(profile);
  if (!disclaimer_service) {
    return base::ScopedClosureRunner();
  }
  return disclaimer_service->AutoAcceptManagementDisclaimerUntilReset();
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace enterprise_util

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(ManagedBrowserUtils)
#endif
