// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_FACTORY_H_

#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;

namespace media_router {

class AccessCodeCastSinkService;

// A factory that lazily returns an AccessCodeCastSinkService
// implementation for a given BrowserContext.
class AccessCodeCastSinkServiceFactory : public ProfileKeyedServiceFactory {
 public:
  AccessCodeCastSinkServiceFactory(const AccessCodeCastSinkServiceFactory&) =
      delete;
  AccessCodeCastSinkServiceFactory& operator=(
      const AccessCodeCastSinkServiceFactory&) = delete;

  static AccessCodeCastSinkService* GetForProfile(Profile* profile);

  static AccessCodeCastSinkServiceFactory* GetInstance();

 protected:
  friend base::NoDestructor<AccessCodeCastSinkServiceFactory>;

  AccessCodeCastSinkServiceFactory();
  ~AccessCodeCastSinkServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_SINK_SERVICE_FACTORY_H_
