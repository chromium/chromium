// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_FACTORY_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace arc {

class ArcDocumentsProviderRootMap;

class ArcDocumentsProviderRootMapFactory : public ProfileKeyedServiceFactory {
 public:
  ArcDocumentsProviderRootMapFactory(
      const ArcDocumentsProviderRootMapFactory&) = delete;
  ArcDocumentsProviderRootMapFactory& operator=(
      const ArcDocumentsProviderRootMapFactory&) = delete;

  // Returns the ArcDocumentsProviderRootMap for |context|, creating it if not
  // created yet.
  static ArcDocumentsProviderRootMap* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns the singleton ArcDocumentsProviderRootMapFactory instance.
  static ArcDocumentsProviderRootMapFactory* GetInstance();

 private:
  friend base::NoDestructor<ArcDocumentsProviderRootMapFactory>;

  ArcDocumentsProviderRootMapFactory();
  ~ArcDocumentsProviderRootMapFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_FACTORY_H_
