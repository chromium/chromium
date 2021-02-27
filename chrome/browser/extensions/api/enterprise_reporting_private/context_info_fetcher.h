// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONTEXT_INFO_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONTEXT_INFO_FETCHER_H_

#include "base/callback_forward.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace enterprise_connectors {
enum AnalysisConnector : int;
class ConnectorsService;
}  // namespace enterprise_connectors

namespace extensions {
namespace enterprise_reporting {

// Interface used by the chrome.enterprise.reportingPrivate.getContextInfo()
// function that fetches context information on Chrome. Each supported platform
// has its own subclass implementation.
class ContextInfoFetcher {
 public:
  using ContextInfoCallback =
      base::OnceCallback<void(api::enterprise_reporting_private::ContextInfo)>;
  ContextInfoFetcher(
      content::BrowserContext* browser_context,
      enterprise_connectors::ConnectorsService* connectors_service);
  virtual ~ContextInfoFetcher();

  ContextInfoFetcher(const ContextInfoFetcher&) = delete;
  ContextInfoFetcher operator=(const ContextInfoFetcher&) = delete;

  // Returns a platform specific instance of ContextInfoFetcher.
  static std::unique_ptr<ContextInfoFetcher> CreateInstance(
      content::BrowserContext* browser_context,
      enterprise_connectors::ConnectorsService* connectors_service);

  // Fetches the context information for the current platform. Eventually calls
  // |callback_|. This function takes a callback to return a ContextInfo instead
  // of returning synchronously because some attributes need to be fetched
  // asynchronously.
  void Fetch(ContextInfoCallback callback);

 private:
  // The following private methods each populate an attribute of ContextInfo. If
  // an attribute can't share implementation across platforms, its corresponding
  // function should be virtual and overridden in the platform subclasses.

  std::vector<std::string> GetBrowserAffiliationIDs();

  std::vector<std::string> GetProfileAffiliationIDs();

  std::vector<std::string> GetAnalysisConnectorProviders(
      enterprise_connectors::AnalysisConnector connector);

  api::enterprise_reporting_private::RealtimeUrlCheckMode
  GetRealtimeUrlCheckMode();

  std::vector<std::string> GetOnSecurityEventProviders();

  content::BrowserContext* browser_context_;

  // |connectors_service| is used to obtain the value of each Connector policy.
  enterprise_connectors::ConnectorsService* connectors_service_;
};

}  // namespace enterprise_reporting
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONTEXT_INFO_FETCHER_H_
