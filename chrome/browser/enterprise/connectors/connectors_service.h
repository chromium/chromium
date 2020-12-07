// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

// Controls whether the Enterprise Connectors policies should be read by
// ConnectorsManager.
extern const base::Feature kEnterpriseConnectorsEnabled;

// For the moment, service provider configurations are static and only support
// google endpoints.  Therefore the configuration is placed here directly.
// Once the configuration becomes more dynamic this static string will be
// removed and replaced with a service to keep it up to date.
extern const char kServiceProviderConfig[];

// Accessor for the ServiceProviderConfig.
ServiceProviderConfig* GetServiceProviderConfig();

// A keyed service to access ConnectorsManager, which tracks Connector policies.
class ConnectorsService : public KeyedService {
 public:
  explicit ConnectorsService(std::unique_ptr<ConnectorsManager> manager);
  ~ConnectorsService() override;

  // Accessors that check kEnterpriseConnectorsEnabled is enabled, and then call
  // the corresponding method in ConnectorsManager.
  base::Optional<ReportingSettings> GetReportingSettings(
      ReportingConnector connector);
  base::Optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      AnalysisConnector connector);

  bool IsConnectorEnabled(AnalysisConnector connector) const;
  bool IsConnectorEnabled(ReportingConnector connector) const;

  bool DelayUntilVerdict(AnalysisConnector connector);

  // Testing functions.
  ConnectorsManager* ConnectorsManagerForTesting();

 private:
  std::unique_ptr<ConnectorsManager> connectors_manager_;
};

class ConnectorsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ConnectorsServiceFactory* GetInstance();
  static ConnectorsService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  ConnectorsServiceFactory();
  ~ConnectorsServiceFactory() override;
  friend struct base::DefaultSingletonTraits<ConnectorsServiceFactory>;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
