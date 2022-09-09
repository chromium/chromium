// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_FACTORY_H_

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace ash {

class CupsPrintJobManager;

class CupsPrintJobManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static CupsPrintJobManagerFactory* GetInstance();
  static CupsPrintJobManager* GetForBrowserContext(
      content::BrowserContext* context);

  CupsPrintJobManagerFactory(const CupsPrintJobManagerFactory&) = delete;
  CupsPrintJobManagerFactory& operator=(const CupsPrintJobManagerFactory&) =
      delete;

 private:
  friend struct base::LazyInstanceTraitsBase<CupsPrintJobManagerFactory>;

  CupsPrintJobManagerFactory();
  ~CupsPrintJobManagerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_FACTORY_H_
