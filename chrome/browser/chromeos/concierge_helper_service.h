// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONCIERGE_HELPER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CONCIERGE_HELPER_SERVICE_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

// This class starts Concierge on construction and provides functions to
// enable/disable throttling for various VMs (crostini, arc, plugin, etc).
class ConciergeHelperService : public KeyedService {
 public:
  // Return singleton instance for the given BrowserContext.
  static ConciergeHelperService* GetForBrowserContext(
      content::BrowserContext* context);

  ConciergeHelperService();
  ~ConciergeHelperService() override = default;

  void SetArcVmCpuRestriction(bool do_restrict);
  void SetTerminaVmCpuRestriction(bool do_restrict);
  void SetPluginVmCpuRestriction(bool do_restrict);

 private:
  DISALLOW_COPY_AND_ASSIGN(ConciergeHelperService);
};

class ConciergeHelperServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ConciergeHelperServiceFactory* GetInstance();
  static ConciergeHelperService* GetForBrowserContext(
      content::BrowserContext* context);

 protected:
  friend class base::NoDestructor<ConciergeHelperServiceFactory>;

  ConciergeHelperServiceFactory();
  ~ConciergeHelperServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConciergeHelperServiceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CONCIERGE_HELPER_SERVICE_H_
