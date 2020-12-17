// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include <memory>

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace enterprise_connectors {

const base::Feature kEnterpriseConnectorsEnabled{
    "EnterpriseConnectorsEnabled", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPerProfileConnectorsEnabled{
    "PerProfileConnectorsEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

const char kServiceProviderConfig[] = R"({
  "version": "1",
  "service_providers" : [
    {
      "name": "google",
      "display_name": "Google Cloud",
      "version": {
        "1": {
          "analysis": {
            "url": "https://safebrowsing.google.com/safebrowsing/uploads/scan",
            "supported_tags": [
              {
                "name": "malware",
                "display_name": "Threat protection",
                "mime_types": [
                  "application/vnd.microsoft.portable-executable",
                  "application/vnd.rar",
                  "application/x-msdos-program",
                  "application/zip"
                ],
                "max_file_size": 52428800
              },
              {
                "name": "dlp",
                "display_name": "Sensitive data protection",
                "mime_types": [
                  "application/gzip",
                  "application/msword",
                  "application/pdf",
                  "application/postscript",
                  "application/rtf",
                  "application/vnd.google-apps.document.internal",
                  "application/vnd.google-apps.spreadsheet.internal",
                  "application/vnd.ms-cab-compressed",
                  "application/vnd.ms-excel",
                  "application/vnd.ms-powerpoint",
                  "application/vnd.ms-xpsdocument",
                  "application/vnd.oasis.opendocument.text",
                  "application/vnd.openxmlformats-officedocument.presentationml.presentation",
                  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                  "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
                  "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                  "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
                  "application/vnd.ms-excel.sheet.macroenabled.12",
                  "application/vnd.ms-excel.template.macroenabled.12",
                  "application/vnd.ms-word.document.macroenabled.12",
                  "application/vnd.ms-word.template.macroenabled.12",
                  "application/vnd.rar",
                  "application/vnd.wordperfect",
                  "application/x-7z-compressed",
                  "application/x-bzip",
                  "application/x-bzip2",
                  "application/x-tar",
                  "application/zip",
                  "text/csv",
                  "text/plain"
                ],
                "max_file_size": 52428800
              }
            ]
          },
          "reporting": {
            "url": "https://chromereporting-pa.googleapis.com/v1/events"
          }
        }
      }
    }
  ]
})";

ServiceProviderConfig* GetServiceProviderConfig() {
  static base::NoDestructor<ServiceProviderConfig> config(
      kServiceProviderConfig);
  return config.get();
}

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

base::Optional<ReportingSettings> ConnectorsService::GetReportingSettings(
    ReportingConnector connector) {
  if (!ConnectorsEnabled())
    return base::nullopt;

  base::Optional<DmToken> dm_token = GetDmToken(ConnectorScopePref(connector));
  if (!dm_token.has_value())
    return base::nullopt;

  base::Optional<ReportingSettings> settings =
      connectors_manager_->GetReportingSettings(connector);
  if (settings.has_value()) {
    settings.value().dm_token = dm_token.value().value;
    settings.value().per_profile =
        dm_token.value().scope == policy::POLICY_SCOPE_USER;
  }

  return settings;
}

base::Optional<AnalysisSettings> ConnectorsService::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  if (!ConnectorsEnabled())
    return base::nullopt;

  base::Optional<DmToken> dm_token = GetDmToken(ConnectorScopePref(connector));
  if (!dm_token.has_value())
    return base::nullopt;

  base::Optional<AnalysisSettings> settings =
      connectors_manager_->GetAnalysisSettings(url, connector);
  if (settings.has_value()) {
    settings.value().dm_token = dm_token.value().value;
  }

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

bool ConnectorsService::DelayUntilVerdict(AnalysisConnector connector) {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->DelayUntilVerdict(connector);
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

base::Optional<ConnectorsService::DmToken> ConnectorsService::GetDmToken(
    const char* scope_pref) const {
#if defined(OS_CHROMEOS)
  // On CrOS, the device must be affiliated to use the DM token for
  // scanning/reporting so we always use the browser DM token.
  return GetBrowserDmToken();
#else
  return GetPolicyScope(scope_pref) == policy::POLICY_SCOPE_USER
             ? GetProfileDmToken()
             : GetBrowserDmToken();
#endif
}

base::Optional<ConnectorsService::DmToken>
ConnectorsService::GetBrowserDmToken() const {
  policy::DMToken dm_token =
      policy::GetDMToken(Profile::FromBrowserContext(context_));

  if (!dm_token.is_valid())
    return base::nullopt;

  return DmToken(dm_token.value(), policy::POLICY_SCOPE_MACHINE);
}

#if !defined(OS_CHROMEOS)
base::Optional<ConnectorsService::DmToken>
ConnectorsService::GetProfileDmToken() const {
  if (!base::FeatureList::IsEnabled(kPerProfileConnectorsEnabled))
    return base::nullopt;

  if (!CanUseProfileDmToken())
    return base::nullopt;

  policy::UserCloudPolicyManager* policy_manager =
      Profile::FromBrowserContext(context_)->GetUserCloudPolicyManager();
  if (!policy_manager || !policy_manager->IsClientRegistered())
    return base::nullopt;

  return DmToken(policy_manager->core()->client()->dm_token(),
                 policy::POLICY_SCOPE_USER);
}

bool ConnectorsService::CanUseProfileDmToken() const {
  // If the browser isn't managed by CBCM, then the profile DM token can be
  // used.
  if (!policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid())
    return true;

  policy::UserCloudPolicyManager* profile_policy_manager =
      Profile::FromBrowserContext(context_)->GetUserCloudPolicyManager();
  policy::MachineLevelUserCloudPolicyManager* browser_policy_manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();

  if (!profile_policy_manager || !browser_policy_manager ||
      !profile_policy_manager->IsClientRegistered() ||
      !browser_policy_manager->IsClientRegistered()) {
    return false;
  }

  auto* profile_policy = profile_policy_manager->core()->store()->policy();
  auto* browser_policy = browser_policy_manager->core()->store()->policy();

  if (!profile_policy || !browser_policy)
    return false;

  return chrome::enterprise_util::IsProfileAffiliated(*profile_policy,
                                                      *browser_policy);
}
#endif  // !defined(OS_CHROMEOS)

policy::PolicyScope ConnectorsService::GetPolicyScope(
    const char* scope_pref) const {
  return static_cast<policy::PolicyScope>(
      Profile::FromBrowserContext(context_)->GetPrefs()->GetInteger(
          scope_pref));
}

bool ConnectorsService::ConnectorsEnabled() const {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return false;

  return !Profile::FromBrowserContext(context_)->IsOffTheRecord();
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
  return new ConnectorsService(
      context,
      std::make_unique<ConnectorsManager>(
          user_prefs::UserPrefs::Get(context), GetServiceProviderConfig(),
          base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled)));
}

content::BrowserContext* ConnectorsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace enterprise_connectors
