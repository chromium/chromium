// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class InstallationReporter;

class InstallationReporterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static InstallationReporter* GetForBrowserContext(
      content::BrowserContext* context);

  static InstallationReporterFactory* GetInstance();

 private:
  friend class base::NoDestructor<InstallationReporterFactory>;

  InstallationReporterFactory();
  ~InstallationReporterFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(InstallationReporterFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_FACTORY_H_
