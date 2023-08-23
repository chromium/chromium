// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SHARING_SHARING_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}

class SharingService;

// Factory for SharingService.
class SharingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns singleton instance of SharingServiceFactory.
  static SharingServiceFactory* GetInstance();

  // Returns the SharingService associated with |context|.
  static SharingService* GetForBrowserContext(content::BrowserContext* context);

  SharingServiceFactory(const SharingServiceFactory&) = delete;
  SharingServiceFactory& operator=(const SharingServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SharingServiceFactory>;

  SharingServiceFactory();
  ~SharingServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_FACTORY_H_
