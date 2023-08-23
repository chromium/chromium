// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DOWNLOAD_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DOWNLOAD_OBSERVER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace policy {

class DlpDownloadObserverFactory : public ProfileKeyedServiceFactory {
 public:
  DlpDownloadObserverFactory();

  static DlpDownloadObserverFactory* GetInstance();

  // ProfileKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy

#endif  //  CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DOWNLOAD_OBSERVER_FACTORY_H_
