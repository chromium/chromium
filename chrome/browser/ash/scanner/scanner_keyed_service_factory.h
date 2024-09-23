// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNER_SCANNER_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_SCANNER_SCANNER_KEYED_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/scanner/scanner_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

class ScannerKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ScannerKeyedService* GetForProfile(Profile* profile);
  static ScannerKeyedServiceFactory* GetInstance();
  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<ScannerKeyedServiceFactory>;

  ScannerKeyedServiceFactory();
  ~ScannerKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ASH_SCANNER_SCANNER_KEYED_SERVICE_FACTORY_H_
