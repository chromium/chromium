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

namespace tpcd::support {
class TpcdSupportService;

class TpcdSupportServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TpcdSupportServiceFactory* GetInstance();
  static TpcdSupportService* GetForProfile(Profile* profile);
  static ProfileSelections CreateProfileSelections();

  TpcdSupportServiceFactory(const TpcdSupportServiceFactory&) = delete;
  TpcdSupportServiceFactory& operator=(const TpcdSupportServiceFactory&) =
      delete;
  TpcdSupportServiceFactory(TpcdSupportServiceFactory&&) = delete;
  TpcdSupportServiceFactory& operator=(TpcdSupportServiceFactory&&) = delete;

 private:
  friend class base::NoDestructor<TpcdSupportServiceFactory>;

  TpcdSupportServiceFactory();
  ~TpcdSupportServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace tpcd::support

#endif  // CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_SERVICE_FACTORY_H_
