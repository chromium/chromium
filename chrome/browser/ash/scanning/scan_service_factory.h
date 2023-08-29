// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_SCAN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_SCANNING_SCAN_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class ScanService;

// Factory for ScanService.
class ScanServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ScanService* GetForBrowserContext(content::BrowserContext* context);
  static ScanServiceFactory* GetInstance();
  static KeyedService* BuildInstanceFor(content::BrowserContext* context);

 private:
  friend base::NoDestructor<ScanServiceFactory>;

  ScanServiceFactory();
  ~ScanServiceFactory() override;

  ScanServiceFactory(const ScanServiceFactory&) = delete;
  ScanServiceFactory& operator=(const ScanServiceFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_SCAN_SERVICE_FACTORY_H_
