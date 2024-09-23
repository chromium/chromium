// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_SUPPORT_ORIGIN_TRIAL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TPCD_SUPPORT_ORIGIN_TRIAL_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace tpcd::trial {
class OriginTrialService;

class OriginTrialServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static OriginTrialServiceFactory* GetInstance();
  static OriginTrialService* GetForProfile(Profile* profile);
  static ProfileSelections CreateProfileSelections();

  OriginTrialServiceFactory(const OriginTrialServiceFactory&) = delete;
  OriginTrialServiceFactory& operator=(const OriginTrialServiceFactory&) =
      delete;
  OriginTrialServiceFactory(OriginTrialServiceFactory&&) = delete;
  OriginTrialServiceFactory& operator=(OriginTrialServiceFactory&&) = delete;

 private:
  friend class base::NoDestructor<OriginTrialServiceFactory>;

  OriginTrialServiceFactory();
  ~OriginTrialServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace tpcd::trial

#endif  // CHROME_BROWSER_TPCD_SUPPORT_ORIGIN_TRIAL_SERVICE_FACTORY_H_
