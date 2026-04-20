// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_LOG_ROUTER_FACTORY_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_LOG_ROUTER_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace multistep_filter {

class MultistepFilterLogRouterImpl;

// Factory for MultistepFilterLogRouterImpl.
class MultistepFilterLogRouterFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the MultistepFilterLogRouterImpl for the given |profile|.
  // Returns nullptr for Off-The-Record (Incognito) profiles, as well as when
  // the feature is disabled.
  static MultistepFilterLogRouterImpl* GetForProfile(Profile* profile);
  static MultistepFilterLogRouterFactory* GetInstance();

  MultistepFilterLogRouterFactory(const MultistepFilterLogRouterFactory&) =
      delete;
  MultistepFilterLogRouterFactory& operator=(
      const MultistepFilterLogRouterFactory&) = delete;

 private:
  friend base::NoDestructor<MultistepFilterLogRouterFactory>;

  MultistepFilterLogRouterFactory();
  ~MultistepFilterLogRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_LOG_ROUTER_FACTORY_H_
