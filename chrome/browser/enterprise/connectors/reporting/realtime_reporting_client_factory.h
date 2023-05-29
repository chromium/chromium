// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REALTIME_REPORTING_CLIENT_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REALTIME_REPORTING_CLIENT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace enterprise_connectors {

class RealtimeReportingClient;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the safeBrowsingPrivate event router per profile (since the
// extension event router is per profile).
class RealtimeReportingClientFactory : public ProfileKeyedServiceFactory {
 public:
  RealtimeReportingClientFactory(const RealtimeReportingClientFactory&) =
      delete;
  RealtimeReportingClientFactory& operator=(
      const RealtimeReportingClientFactory&) = delete;

  // Returns the RealtimeReportingClient for |profile|, creating it if
  // it is not yet created.
  static RealtimeReportingClient* GetForProfile(
      content::BrowserContext* context);

  // Returns the RealtimeReportingClientFactory instance.
  static RealtimeReportingClientFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend base::NoDestructor<RealtimeReportingClientFactory>;

  RealtimeReportingClientFactory();
  ~RealtimeReportingClientFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REALTIME_REPORTING_CLIENT_FACTORY_H_
