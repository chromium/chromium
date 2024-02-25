// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_ROUTER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ax {
class AccessibilityServiceRouter;

// Used to get the AccessibilityServiceRouter for a BrowserContext. This allows
// a different AccessibilityService per profile.
class AccessibilityServiceRouterFactory : public ProfileKeyedServiceFactory {
 public:
  static AccessibilityServiceRouter* GetForBrowserContext(
      content::BrowserContext* context);

  static AccessibilityServiceRouterFactory* GetInstanceForTest() {
    return GetInstance();
  }

  static void EnsureFactoryBuilt();

 private:
  friend class base::NoDestructor<AccessibilityServiceRouterFactory>;
  static AccessibilityServiceRouterFactory* GetInstance();

  AccessibilityServiceRouterFactory();
  ~AccessibilityServiceRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ax

#endif  // CHROME_BROWSER_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_ROUTER_FACTORY_H_
