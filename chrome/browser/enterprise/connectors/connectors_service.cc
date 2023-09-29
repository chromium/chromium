// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "device_management_backend.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace enterprise_connectors {

namespace {

void PopulateBrowserMetadata(bool include_device_info,
                             ClientMetadata::Browser* browser_proto) {
  base::FilePath browser_id;
  if (base::PathService::Get(base::DIR_EXE, &browser_id))
    browser_proto->set_browser_id(browser_id.AsUTF8Unsafe());
  browser_proto->set_chrome_version(
      std::string(version_info::GetVersionNumber()));
  if (include_device_info)
    browser_proto->set_machine_user(policy::GetOSUsername());
}

void PopulateDeviceMetadata(const ReportingSettings& reporting_settings,
                            Profile* profile,
                            ClientMetadata::Device* device_proto) {
  if (!reporting_settings.per_profile && !device_proto->has_dm_token()) {
    device_proto->set_dm_token(reporting_settings.dm_token);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string client_id;
  auto* manager = profile->GetUserCloudPolicyManagerAsh();
  if (manager && manager->core() && manager->core()->client())
    client_id = manager->core()->client()->client_id();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  Profile* main_profile = GetMainProfileLacros();
  std::string client_id;
  if (main_profile) {
    client_id = reporting::GetUserClientId(main_profile).value_or("");
  }
#else
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
#endif
  device_proto->set_client_id(client_id);
  device_proto->set_os_version(policy::GetOSVersion());
  device_proto->set_os_platform(policy::GetOSPlatform());
  device_proto->set_name(policy::GetDeviceName());
}

bool IsURLExemptFromAnalysis(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme))
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      extension_misc::IsSystemUIApp(url.host_piece())) {
    return true;
  }
#endif

  return false;
}

#if BUILDFLAG(IS_CHROMEOS)
absl::optional<std::string> GetDeviceDMToken() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  if (init_params->DeviceProperties()) {
    return init_params->DeviceProperties()->device_dm_token;
  }
  return absl::nullopt;
#else
  const enterprise_management::PolicyData* policy_data =
      ash::DeviceSettingsService::Get()->policy_data();
  if (policy_data && policy_data->has_request_token())
    return policy_data->request_token();
  return absl::nullopt;
#endif
}
#endif

std::unique_ptr<ClientMetadata> GetBasicClientMetadata() {
  // In this case, we are just using the client metadata to indicate to
  // WebProtect whether or not the request is coming from a Managed Guest
  // Session on ChromeOS, so we do not need the other info.
  if (base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabledOnMGS)) {
    auto metadata = std::make_unique<ClientMetadata>();

    metadata->mutable_profile()->set_is_chrome_os_managed_guest_session(
        profiles::IsManagedGuestSession());
    return metadata;
  } else {
    return nullptr;
  }
}
}  // namespace

BASE_FEATURE(kEnterpriseConnectorsEnabledOnMGS,
             "EnterpriseConnectorsEnabledOnMGS",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableRelaxedAffiliationCheck,
             "EnableRelaxedAffiliationCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

// --------------------------------
// ConnectorsService implementation
// --------------------------------

ConnectorsService::ConnectorsService(content::BrowserContext* context,
                                     std::unique_ptr<ConnectorsManager> manager)
    : context_(context), connectors_manager_(std::move(manager)) {
  DCHECK(context_);
  DCHECK(connectors_manager_);
}

ConnectorsService::~ConnectorsService() = default;

absl::optional<ReportingSettings> ConnectorsService::GetReportingSettings(
    ReportingConnector connector) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  absl::optional<ReportingSettings> settings =
      connectors_manager_->GetReportingSettings(connector);
  if (!settings.has_value())
    return absl::nullopt;

#if BUILDFLAG(IS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(context_);
  if (IncludeDeviceInfo(profile, /*per_profile=*/false)) {
    // The device dm token includes additional information like a device id,
    // which is relevant for reporting and should only be used for
    // IncludeDeviceInfo==true.
    absl::optional<std::string> device_dm_token = GetDeviceDMToken();
    if (device_dm_token.has_value()) {
      settings.value().dm_token = device_dm_token.value();
      settings.value().per_profile = false;
      return settings;
    }
  }
#endif
  absl::optional<DmToken> dm_token = GetDmToken(ConnectorScopePref(connector));
  if (!dm_token.has_value())
    return absl::nullopt;

  settings.value().dm_token = dm_token.value().value;
  settings.value().per_profile =
      dm_token.value().scope == policy::POLICY_SCOPE_USER;

  return settings;
}

absl::optional<AnalysisSettings> ConnectorsService::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  DCHECK_NE(connector, AnalysisConnector::FILE_TRANSFER);
  if (!ConnectorsEnabled())
    return absl::nullopt;

  if (IsURLExemptFromAnalysis(url))
    return absl::nullopt;

  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    GURL inner = url.inner_url() ? *url.inner_url() : GURL(url.path());
    return GetCommonAnalysisSettings(
        connectors_manager_->GetAnalysisSettings(inner, connector), connector);
  }

  return GetCommonAnalysisSettings(
      connectors_manager_->GetAnalysisSettings(url, connector), connector);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
absl::optional<AnalysisSettings> ConnectorsService::GetAnalysisSettings(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    AnalysisConnector connector) {
  DCHECK_EQ(connector, AnalysisConnector::FILE_TRANSFER);
  if (!ConnectorsEnabled())
    return absl::nullopt;

  return GetCommonAnalysisSettings(
      connectors_manager_->GetAnalysisSettings(context_, source_url,
                                               destination_url, connector),
      connector);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

absl::optional<AnalysisSettings> ConnectorsService::GetCommonAnalysisSettings(
    absl::optional<AnalysisSettings> settings,
    AnalysisConnector connector) {
  if (!settings.has_value())
    return absl::nullopt;

#if !BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  if (settings->cloud_or_local_settings.is_local_analysis()) {
    return absl::nullopt;
  }
#endif

  absl::optional<DmToken> dm_token = GetDmToken(ConnectorScopePref(connector));
  bool is_cloud = settings.value().cloud_or_local_settings.is_cloud_analysis();

  if (is_cloud) {
    if (!dm_token.has_value())
      return absl::nullopt;

    absl::get<CloudAnalysisSettings>(settings.value().cloud_or_local_settings)
        .dm_token = dm_token.value().value;
  }

  settings.value().per_profile =
      (dm_token.has_value() &&
       dm_token.value().scope == policy::POLICY_SCOPE_USER) ||
      GetPolicyScope(ConnectorScopePref(connector)) ==
          policy::POLICY_SCOPE_USER;
  settings.value().client_metadata = BuildClientMetadata(is_cloud);

  return settings;
}

bool ConnectorsService::IsConnectorEnabled(AnalysisConnector connector) const {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->IsConnectorEnabled(connector);
}

bool ConnectorsService::IsConnectorEnabled(ReportingConnector connector) const {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->IsConnectorEnabled(connector);
}

std::vector<std::string> ConnectorsService::GetReportingServiceProviderNames(
    ReportingConnector connector) {
  if (!ConnectorsEnabled())
    return {};

  if (!GetDmToken(ConnectorScopePref(connector)).has_value())
    return {};

  return connectors_manager_->GetReportingServiceProviderNames(connector);
}

std::vector<const AnalysisConfig*> ConnectorsService::GetAnalysisServiceConfigs(
    AnalysisConnector connector) {
  if (!ConnectorsEnabled())
    return {};

  return connectors_manager_->GetAnalysisServiceConfigs(connector);
}

bool ConnectorsService::DelayUntilVerdict(AnalysisConnector connector) {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->DelayUntilVerdict(connector);
}

absl::optional<std::u16string> ConnectorsService::GetCustomMessage(
    AnalysisConnector connector,
    const std::string& tag) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  return connectors_manager_->GetCustomMessage(connector, tag);
}

absl::optional<GURL> ConnectorsService::GetLearnMoreUrl(
    AnalysisConnector connector,
    const std::string& tag) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  return connectors_manager_->GetLearnMoreUrl(connector, tag);
}

bool ConnectorsService::GetBypassJustificationRequired(
    AnalysisConnector connector,
    const std::string& tag) {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->GetBypassJustificationRequired(connector, tag);
}

bool ConnectorsService::HasExtraUiToDisplay(AnalysisConnector connector,
                                            const std::string& tag) {
  return GetCustomMessage(connector, tag) || GetLearnMoreUrl(connector, tag) ||
         GetBypassJustificationRequired(connector, tag);
}

std::vector<std::string> ConnectorsService::GetAnalysisServiceProviderNames(
    AnalysisConnector connector) {
  if (!ConnectorsEnabled())
    return {};

  if (!GetDmToken(ConnectorScopePref(connector)).has_value())
    return {};

  return connectors_manager_->GetAnalysisServiceProviderNames(connector);
}

std::string ConnectorsService::GetManagementDomain() {
  if (!ConnectorsEnabled())
    return std::string();

  absl::optional<policy::PolicyScope> scope = absl::nullopt;
  for (const char* scope_pref :
       {prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope,
        ConnectorScopePref(AnalysisConnector::FILE_ATTACHED),
        ConnectorScopePref(AnalysisConnector::FILE_DOWNLOADED),
        ConnectorScopePref(AnalysisConnector::BULK_DATA_ENTRY),
        ConnectorScopePref(AnalysisConnector::PRINT),
        ConnectorScopePref(ReportingConnector::SECURITY_EVENT)}) {
    absl::optional<DmToken> dm_token = GetDmToken(scope_pref);
    if (dm_token.has_value()) {
      scope = dm_token.value().scope;

      // Having one CBCM Connector policy set implies that profile ones will be
      // ignored for another domain, so the loop can stop immediately.
      if (scope == policy::PolicyScope::POLICY_SCOPE_MACHINE)
        break;
    }
  }

  if (!scope.has_value())
    return std::string();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return chrome::GetAccountManagerIdentity(
             Profile::FromBrowserContext(context_))
      .value_or(std::string());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // In LaCros it's always managed by main profile policy.
  const enterprise_management::PolicyData* policy =
      policy::PolicyLoaderLacros::main_user_policy_data();
  if (policy && policy->has_managed_by())
    return policy->managed_by();
  return std::string();
#else
  if (scope.value() == policy::PolicyScope::POLICY_SCOPE_USER) {
    return chrome::GetAccountManagerIdentity(
               Profile::FromBrowserContext(context_))
        .value_or(std::string());
  }

  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  if (!manager)
    return std::string();

  policy::CloudPolicyStore* store = manager->store();
  return (store && store->has_policy())
             ? gaia::ExtractDomainName(store->policy()->username())
             : std::string();
#endif
}

absl::optional<std::string> ConnectorsService::GetDMTokenForRealTimeUrlCheck()
    const {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  if (Profile::FromBrowserContext(context_)->GetPrefs()->GetInteger(
          prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode) ==
      safe_browsing::REAL_TIME_CHECK_DISABLED) {
    return absl::nullopt;
  }

  absl::optional<DmToken> dm_token =
      GetDmToken(prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope);

  if (dm_token.has_value())
    return dm_token.value().value;
  return absl::nullopt;
}

safe_browsing::EnterpriseRealTimeUrlCheckMode
ConnectorsService::GetAppliedRealTimeUrlCheck() const {
  if (!ConnectorsEnabled() ||
      !GetDmToken(prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope)
           .has_value()) {
    return safe_browsing::REAL_TIME_CHECK_DISABLED;
  }

  return static_cast<safe_browsing::EnterpriseRealTimeUrlCheckMode>(
      Profile::FromBrowserContext(context_)->GetPrefs()->GetInteger(
          prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode));
}

ConnectorsManager* ConnectorsService::ConnectorsManagerForTesting() {
  return connectors_manager_.get();
}

ConnectorsService::DmToken::DmToken(const std::string& value,
                                    policy::PolicyScope scope)
    : value(value), scope(scope) {}
ConnectorsService::DmToken::DmToken(DmToken&&) = default;
ConnectorsService::DmToken& ConnectorsService::DmToken::operator=(DmToken&&) =
    default;
ConnectorsService::DmToken::~DmToken() = default;

absl::optional<ConnectorsService::DmToken> ConnectorsService::GetDmToken(
    const char* scope_pref) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On CrOS the settings from primary profile applies to all profiles.
  return GetBrowserDmToken();
#else
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  Profile* profile = Profile::FromBrowserContext(context_);
  if (profile->IsMainProfile()) {
    return GetBrowserDmToken();
  }
#endif
  return GetPolicyScope(scope_pref) == policy::POLICY_SCOPE_USER
             ? GetProfileDmToken()
             : GetBrowserDmToken();
#endif
}

absl::optional<ConnectorsService::DmToken>
ConnectorsService::GetBrowserDmToken() const {
  policy::DMToken dm_token =
      policy::GetDMToken(Profile::FromBrowserContext(context_));

  if (!dm_token.is_valid())
    return absl::nullopt;

  return DmToken(dm_token.value(), policy::POLICY_SCOPE_MACHINE);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
absl::optional<ConnectorsService::DmToken>
ConnectorsService::GetProfileDmToken() const {
  if (!base::FeatureList::IsEnabled(kEnableRelaxedAffiliationCheck) &&
      !CanUseProfileDmToken()) {
    return absl::nullopt;
  }
  Profile* profile = Profile::FromBrowserContext(context_);

  policy::UserCloudPolicyManager* policy_manager =
      profile->GetUserCloudPolicyManager();
  if (!policy_manager || !policy_manager->IsClientRegistered())
    return absl::nullopt;

  return DmToken(policy_manager->core()->client()->dm_token(),
                 policy::POLICY_SCOPE_USER);
}

bool ConnectorsService::CanUseProfileDmToken() const {
  Profile* profile = Profile::FromBrowserContext(context_);

  // If the browser isn't managed by CBCM, then the profile DM token can be
  // used.
  if (!policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid())
    return true;

  return chrome::enterprise_util::IsProfileAffiliated(profile);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

policy::PolicyScope ConnectorsService::GetPolicyScope(
    const char* scope_pref) const {
#if BUILDFLAG(IS_CHROMEOS)
  // CrOS always uses a browser DM throughout connectors code, so its policy
  // scope should always be POLICY_SCOPE_MACHINE.
  return policy::PolicyScope::POLICY_SCOPE_MACHINE;
#else
  return static_cast<policy::PolicyScope>(
      Profile::FromBrowserContext(context_)->GetPrefs()->GetInteger(
          scope_pref));
#endif
}

bool ConnectorsService::ConnectorsEnabled() const {
  if (profiles::IsManagedGuestSession() &&
      !base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabledOnMGS)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(context_);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // On desktop, the guest profile is actually the primary OTR profile of
  // the "regular" guest profile.  The regular guest profile is never used
  // directly by users.  Also, user are not able to create child OTR profiles
  // from guest profiles, the menu item "New incognito window" is not
  // available.  So, if this is a guest session, allow it only if it is a
  // child OTR profile as well.
  if (profile->IsGuestSession())
    return profile->GetOriginalProfile() != profile;

  // Never allow system profiles.
  if (profile->IsSystemProfile())
    return false;
#endif

  return !profile->IsOffTheRecord();
}

std::unique_ptr<ClientMetadata> ConnectorsService::BuildClientMetadata(
    bool is_cloud) {
  auto reporting_settings =
      GetReportingSettings(ReportingConnector::SECURITY_EVENT);

  if (is_cloud && !reporting_settings.has_value()) {
    return GetBasicClientMetadata();
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  auto metadata = std::make_unique<ClientMetadata>(
      reporting::GetContextAsClientMetadata(profile));

  // Device info is only useful for cloud service providers since local
  // providers can already determine all this info themselves. For this reason,
  // we only include browser metadata.
  if (!is_cloud) {
    PopulateBrowserMetadata(/*include_device_info=*/true,
                            metadata->mutable_browser());
    return metadata;
  }

  if (base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabledOnMGS)) {
    metadata->mutable_profile()->set_is_chrome_os_managed_guest_session(
        profiles::IsManagedGuestSession());
  }

  bool include_device_info =
      IncludeDeviceInfo(profile, reporting_settings.value().per_profile);

  PopulateBrowserMetadata(include_device_info, metadata->mutable_browser());

  if (include_device_info) {
    PopulateDeviceMetadata(reporting_settings.value(), profile,
                           metadata->mutable_device());
  }

  return metadata;
}

// ---------------------------------------
// ConnectorsServiceFactory implementation
// ---------------------------------------

// static
ConnectorsServiceFactory* ConnectorsServiceFactory::GetInstance() {
  return base::Singleton<ConnectorsServiceFactory>::get();
}

ConnectorsService* ConnectorsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ConnectorsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ConnectorsServiceFactory::ConnectorsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ConnectorsService",
          BrowserContextDependencyManager::GetInstance()) {}

ConnectorsServiceFactory::~ConnectorsServiceFactory() = default;

KeyedService* ConnectorsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  bool observe_prefs =
      profiles::IsManagedGuestSession()
          ? base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabledOnMGS)
          : true;

  return new ConnectorsService(
      context, std::make_unique<ConnectorsManager>(
                   std::make_unique<BrowserCrashEventRouter>(context),
                   std::make_unique<ExtensionInstallEventRouter>(context),
                   user_prefs::UserPrefs::Get(context),
                   GetServiceProviderConfig(), observe_prefs));
}

content::BrowserContext* ConnectorsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Do not construct the connectors service if the extensions are disabled for
  // the given context.
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(context)) {
    return nullptr;
  }

  // On Chrome OS, settings from the primary/main profile apply to all
  // profiles, besides incognito.
  // However, the primary/main profile might not exist in tests - then the
  // provided |context| is still used.
  if (context && !context->IsOffTheRecord() &&
      !Profile::FromBrowserContext(context)->AsTestingProfile()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto* user_manager = user_manager::UserManager::Get();
    if (auto* primary_user = user_manager->GetPrimaryUser()) {
      if (auto* primary_browser_context =
              ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
                  primary_user)) {
        return primary_browser_context;
      }
    }
#endif
  }
  return context;
}

}  // namespace enterprise_connectors
