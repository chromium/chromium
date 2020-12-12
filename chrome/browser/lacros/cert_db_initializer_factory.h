// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_FACTORY_H_
#define CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class CertDbInitializer;
class Profile;

class CertDbInitializerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CertDbInitializerFactory* GetInstance();
  static CertDbInitializer* GetForProfileIfExists(Profile* profile);

 private:
  friend class base::NoDestructor<CertDbInitializerFactory>;

  CertDbInitializerFactory();
  ~CertDbInitializerFactory() override = default;

  // BrowserStateKeyedServiceFactory
  bool ServiceIsCreatedWithBrowserContext() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_FACTORY_H_
