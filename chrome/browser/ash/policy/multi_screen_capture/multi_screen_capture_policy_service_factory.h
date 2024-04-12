// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_MULTI_SCREEN_CAPTURE_MULTI_SCREEN_CAPTURE_POLICY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_POLICY_MULTI_SCREEN_CAPTURE_MULTI_SCREEN_CAPTURE_POLICY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace policy {

class MultiScreenCapturePolicyService;

// This factory reacts to profile creation and instantiates profile-keyed
// services that handles the prevention of dynamic refresh for screen capture
// policies. The keyed service only runs on the ash side.
class MultiScreenCapturePolicyServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static MultiScreenCapturePolicyService* GetForBrowserContext(
      content::BrowserContext* context);
  static MultiScreenCapturePolicyServiceFactory* GetInstance();

  MultiScreenCapturePolicyServiceFactory(
      const MultiScreenCapturePolicyServiceFactory&) = delete;
  MultiScreenCapturePolicyServiceFactory& operator=(
      const MultiScreenCapturePolicyServiceFactory&) = delete;

 private:
  friend base::NoDestructor<MultiScreenCapturePolicyServiceFactory>;

  MultiScreenCapturePolicyServiceFactory();
  ~MultiScreenCapturePolicyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_MULTI_SCREEN_CAPTURE_MULTI_SCREEN_CAPTURE_POLICY_SERVICE_FACTORY_H_
