// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_SUPPORT_TOP_LEVEL_TRIAL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TPCD_SUPPORT_TOP_LEVEL_TRIAL_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace tpcd::trial {
class TopLevelTrialService;

class TopLevelTrialServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TopLevelTrialServiceFactory* GetInstance();
  static TopLevelTrialService* GetForProfile(Profile* profile);
  static ProfileSelections CreateProfileSelections();

  TopLevelTrialServiceFactory(const TopLevelTrialServiceFactory&) = delete;
  TopLevelTrialServiceFactory& operator=(const TopLevelTrialServiceFactory&) =
      delete;
  TopLevelTrialServiceFactory(TopLevelTrialServiceFactory&&) = delete;
  TopLevelTrialServiceFactory& operator=(TopLevelTrialServiceFactory&&) =
      delete;

 private:
  friend class base::NoDestructor<TopLevelTrialServiceFactory>;

  TopLevelTrialServiceFactory();
  ~TopLevelTrialServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace tpcd::trial

#endif  // CHROME_BROWSER_TPCD_SUPPORT_TOP_LEVEL_TRIAL_SERVICE_FACTORY_H_
