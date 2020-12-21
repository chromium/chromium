// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_ENGAGEMENT_METRICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_ENGAGEMENT_METRICS_SERVICE_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/guest_os/guest_os_engagement_metrics.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

// A KeyedService for tracking engagement with Crostini, reporting values as
// per GuestOsEngagementMetrics.
class CrostiniEngagementMetricsService : public KeyedService {
 public:
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static CrostiniEngagementMetricsService* GetForProfile(Profile* profile);
    static Factory* GetInstance();

   private:
    friend class base::NoDestructor<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory:
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
    bool ServiceIsCreatedWithBrowserContext() const override;
    bool ServiceIsNULLWhileTesting() const override;
  };

  explicit CrostiniEngagementMetricsService(Profile* profile);
  ~CrostiniEngagementMetricsService() override;

  // This needs to be called when Crostini starts and stops being active so we
  // can correctly track background active time.
  void SetBackgroundActive(bool background_active);

 private:
  std::unique_ptr<guest_os::GuestOsEngagementMetrics>
      guest_os_engagement_metrics_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniEngagementMetricsService);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_ENGAGEMENT_METRICS_SERVICE_H_
