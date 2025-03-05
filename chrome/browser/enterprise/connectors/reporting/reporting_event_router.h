// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

// An event router that collects safe browsing events and then sends
// events to reporting server.
class ReportingEventRouter : public KeyedService {
 public:
  explicit ReportingEventRouter(content::BrowserContext* context);

  ReportingEventRouter(const ReportingEventRouter&) = delete;
  ReportingEventRouter& operator=(const ReportingEventRouter&) = delete;
  ReportingEventRouter(ReportingEventRouter&&) = delete;
  ReportingEventRouter& operator=(ReportingEventRouter&&) = delete;

  ~ReportingEventRouter() override;

  bool IsEventEnabled(const std::string& event);

  void OnLoginEvent(const GURL& url,
                    bool is_federated,
                    const url::SchemeHostPort& federated_origin,
                    const std::u16string& username);

  void OnPasswordBreach(
      const std::string& trigger,
      const std::vector<std::pair<GURL, std::u16string>>& identities);

 private:
  raw_ptr<content::BrowserContext> context_;
  raw_ptr<RealtimeReportingClient> reporting_client_;
};

class ReportingEventRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ReportingEventRouterFactory* GetInstance();
  static ReportingEventRouter* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  ReportingEventRouterFactory();
  ~ReportingEventRouterFactory() override;
  friend struct base::DefaultSingletonTraits<ReportingEventRouterFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_EVENT_ROUTER_H_
