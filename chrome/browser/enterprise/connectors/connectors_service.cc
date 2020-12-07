// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include <memory>

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace enterprise_connectors {

const base::Feature kEnterpriseConnectorsEnabled{
    "EnterpriseConnectorsEnabled", base::FEATURE_ENABLED_BY_DEFAULT};

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

ConnectorsService::ConnectorsService(std::unique_ptr<ConnectorsManager> manager)
    : connectors_manager_(std::move(manager)) {
  DCHECK(connectors_manager_);
}

ConnectorsService::~ConnectorsService() = default;

base::Optional<ReportingSettings> ConnectorsService::GetReportingSettings(
    ReportingConnector connector) {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return base::nullopt;

  return connectors_manager_->GetReportingSettings(connector);
}

base::Optional<AnalysisSettings> ConnectorsService::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return base::nullopt;

  return connectors_manager_->GetAnalysisSettings(url, connector);
}

bool ConnectorsService::IsConnectorEnabled(AnalysisConnector connector) const {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return false;

  return connectors_manager_->IsConnectorEnabled(connector);
}

bool ConnectorsService::IsConnectorEnabled(ReportingConnector connector) const {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return false;

  return connectors_manager_->IsConnectorEnabled(connector);
}

bool ConnectorsService::DelayUntilVerdict(AnalysisConnector connector) {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return false;

  return connectors_manager_->DelayUntilVerdict(connector);
}

ConnectorsManager* ConnectorsService::ConnectorsManagerForTesting() {
  return connectors_manager_.get();
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
  return new ConnectorsService(std::make_unique<ConnectorsManager>(
      user_prefs::UserPrefs::Get(context), GetServiceProviderConfig(),
      base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled)));
}

content::BrowserContext* ConnectorsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace enterprise_connectors
