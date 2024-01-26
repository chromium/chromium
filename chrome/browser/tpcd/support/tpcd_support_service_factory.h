// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace tpcd::trial {
class TpcdTrialService;

class TpcdTrialServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TpcdTrialServiceFactory* GetInstance();
  static TpcdTrialService* GetForProfile(Profile* profile);
  static ProfileSelections CreateProfileSelections();

  TpcdTrialServiceFactory(const TpcdTrialServiceFactory&) = delete;
  TpcdTrialServiceFactory& operator=(const TpcdTrialServiceFactory&) = delete;
  TpcdTrialServiceFactory(TpcdTrialServiceFactory&&) = delete;
  TpcdTrialServiceFactory& operator=(TpcdTrialServiceFactory&&) = delete;

 private:
  friend class base::NoDestructor<TpcdTrialServiceFactory>;

  TpcdTrialServiceFactory();
  ~TpcdTrialServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace tpcd::trial

#endif  // CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_SERVICE_FACTORY_H_
