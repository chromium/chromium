// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CONCIERGE_HELPER_CONCIERGE_HELPER_SERVICE_H_
#define CHROME_BROWSER_ASH_CONCIERGE_HELPER_CONCIERGE_HELPER_SERVICE_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {

// This class starts Concierge on construction and provides functions to
// enable/disable throttling for various VMs (crostini, arc, plugin, etc).
class ConciergeHelperService : public KeyedService {
 public:
  // Return singleton instance for the given BrowserContext.
  static ConciergeHelperService* GetForBrowserContext(
      content::BrowserContext* context);

  ConciergeHelperService();

  ConciergeHelperService(const ConciergeHelperService&) = delete;
  ConciergeHelperService& operator=(const ConciergeHelperService&) = delete;

  ~ConciergeHelperService() override = default;

  void SetArcVmCpuRestriction(bool do_restrict);
  void SetTerminaVmCpuRestriction(bool do_restrict);
  void SetPluginVmCpuRestriction(bool do_restrict);
};

class ConciergeHelperServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ConciergeHelperServiceFactory* GetInstance();
  static ConciergeHelperService* GetForBrowserContext(
      content::BrowserContext* context);

  ConciergeHelperServiceFactory(const ConciergeHelperServiceFactory&) = delete;
  ConciergeHelperServiceFactory& operator=(
      const ConciergeHelperServiceFactory&) = delete;

 protected:
  friend class base::NoDestructor<ConciergeHelperServiceFactory>;

  ConciergeHelperServiceFactory();
  ~ConciergeHelperServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CONCIERGE_HELPER_CONCIERGE_HELPER_SERVICE_H_
