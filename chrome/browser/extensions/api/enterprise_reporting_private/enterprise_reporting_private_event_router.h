// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "url/gurl.h"

namespace extensions {

class EventRouter;

// Event router for the `enterprise.reportingPrivate` extension namespace.
class EnterpriseReportingPrivateEventRouter : public KeyedService {
 public:
  explicit EnterpriseReportingPrivateEventRouter(
      content::BrowserContext* context);
  ~EnterpriseReportingPrivateEventRouter() override;

  void OnUrlFilteringVerdict(const GURL& url,
                             const safe_browsing::RTLookupResponse& response);

 private:
  raw_ptr<EventRouter> event_router_;
};

class EnterpriseReportingPrivateEventRouterFactory
    : public ProfileKeyedServiceFactory {
 public:
  EnterpriseReportingPrivateEventRouterFactory(
      const EnterpriseReportingPrivateEventRouterFactory&) = delete;
  EnterpriseReportingPrivateEventRouterFactory& operator=(
      const EnterpriseReportingPrivateEventRouterFactory&) = delete;

  static EnterpriseReportingPrivateEventRouterFactory* GetInstance();

  // Always returns a non-null value.
  static EnterpriseReportingPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<EnterpriseReportingPrivateEventRouterFactory>;

  EnterpriseReportingPrivateEventRouterFactory();
  ~EnterpriseReportingPrivateEventRouterFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_EVENT_ROUTER_H_
