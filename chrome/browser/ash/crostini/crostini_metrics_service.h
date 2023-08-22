// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_METRICS_SERVICE_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_METRICS_SERVICE_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/guest_os/guest_os_engagement_metrics.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/wm/public/activation_change_observer.h"

class Profile;

namespace crostini {

// A KeyedService for various Crostini metrics.
class CrostiniMetricsService : public KeyedService,
                               public wm::ActivationChangeObserver {
 public:
  class Factory : public ProfileKeyedServiceFactory {
   public:
    static CrostiniMetricsService* GetForProfile(Profile* profile);
    static Factory* GetInstance();

   private:
    friend class base::NoDestructor<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory:
    std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const override;
    bool ServiceIsCreatedWithBrowserContext() const override;
    bool ServiceIsNULLWhileTesting() const override;
  };

  explicit CrostiniMetricsService(Profile* profile);

  CrostiniMetricsService(const CrostiniMetricsService&) = delete;
  CrostiniMetricsService& operator=(const CrostiniMetricsService&) = delete;

  ~CrostiniMetricsService() override;

  // This needs to be called when Crostini starts and stops being active so we
  // can correctly track background active time.
  void SetBackgroundActive(bool background_active);

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  std::unique_ptr<guest_os::GuestOsEngagementMetrics>
      guest_os_engagement_metrics_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_METRICS_SERVICE_H_
