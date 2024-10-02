// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_

#include <memory>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/connectors_manager_base.h"
#include "components/enterprise/connectors/core/connectors_service_base.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {

// Controls whether the Enterprise Connectors policies should be read by
// ConnectorsManager in Managed Guest Sessions.
BASE_DECLARE_FEATURE(kEnterpriseConnectorsEnabledOnMGS);

// A keyed service to access ConnectorsManager, which tracks Connector policies.
class ConnectorsService : public ConnectorsServiceBase, public KeyedService {
 public:
  ConnectorsService(content::BrowserContext* context,
                    std::unique_ptr<ConnectorsManager> manager);
  ~ConnectorsService() override;

  // Accessors that call the corresponding method in ConnectorsManager.
  std::optional<ReportingSettings> GetReportingSettings(
      ReportingConnector connector) override;
  std::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      AnalysisConnector connector);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<AnalysisSettings> GetAnalysisSettings(
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      AnalysisConnector connector);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  bool IsConnectorEnabled(AnalysisConnector connector) const override;

  bool DelayUntilVerdict(AnalysisConnector connector);

  // Gets custom message if set by the admin.
  std::optional<std::u16string> GetCustomMessage(AnalysisConnector connector,
                                                 const std::string& tag);

  // Gets custom learn more URL if provided by the admin.
  std::optional<GURL> GetLearnMoreUrl(AnalysisConnector connector,
                                      const std::string& tag);

  // Returns true if the admin enabled Bypass Justification.
  bool GetBypassJustificationRequired(AnalysisConnector connector,
                                      const std::string& tag);

  // Returns true if the admin has opted into custom message, learn more URL or
  // letting the user provide bypass justifications in an input dialog.
  bool HasExtraUiToDisplay(AnalysisConnector connector, const std::string& tag);

  std::vector<std::string> GetAnalysisServiceProviderNames(
      AnalysisConnector connector);

  std::vector<const AnalysisConfig*> GetAnalysisServiceConfigs(
      AnalysisConnector connector);

  std::optional<std::string> GetBrowserDmToken() const;

  // Obtain a ClientMetadata instance corresponding to the current
  // OnSecurityEvent policy value.  `is_cloud` is true when using a cloud-
  // based service provider and false when using a local service provider.
  std::unique_ptr<ClientMetadata> BuildClientMetadata(bool is_cloud);

  // Returns the profile email if real-time URL check is set for the profile,
  // the device ID if it is set for the device, or an empty string if it is
  // unset.
  std::string GetRealTimeUrlCheckIdentifier() const;

  // Returns the CBCM domain or profile domain that enables connector policies.
  // If both set Connector policies, the CBCM domain is returned as it has
  // precedence.
  std::string GetManagementDomain();

  // Testing functions.
  ConnectorsManager* ConnectorsManagerForTesting();

  // Observe if reporting policies have changed to include telemetry event.
  void ObserveTelemetryReporting(base::RepeatingCallback<void()> callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceProfileTypeBrowserTest, IsEnabled);
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceReportingFeatureTest,
                           ChromeOsManagedGuestSessionFlagSetInMgs);
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceReportingFeatureTest,
                           ChromeOsManagedGuestSessionFlagNotSetInUserSession);

  std::optional<AnalysisSettings> GetCommonAnalysisSettings(
      std::optional<AnalysisSettings> settings,
      AnalysisConnector connector);

  // ConnectorsServiceBase:
  std::optional<DmToken> GetDmToken(const char* scope_pref) const override;
  bool ConnectorsEnabled() const override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  ConnectorsManagerBase* GetConnectorsManagerBase() override;
  const ConnectorsManagerBase* GetConnectorsManagerBase() const override;
  policy::CloudPolicyManager* GetManagedUserCloudPolicyManager() const override;

  // Returns the policy::PolicyScope stored in the given |scope_pref|.
  policy::PolicyScope GetPolicyScope(const char* scope_pref) const;

  // Returns ClientMetadata populated with minimum required information
  std::unique_ptr<ClientMetadata> GetBasicClientMetadata(Profile* profile);

  raw_ptr<content::BrowserContext> context_;
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
