// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class NearbySharingService;

// Factory for NearbySharingService.
class NearbySharingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Disallow copy and assignment.
  NearbySharingServiceFactory(const NearbySharingServiceFactory&) = delete;
  NearbySharingServiceFactory& operator=(const NearbySharingServiceFactory&) =
      delete;

  // Returns singleton instance of NearbySharingServiceFactory.
  static NearbySharingServiceFactory* GetInstance();

  // Returns whether or not Nearby Share is supported for |context|.
  static bool IsNearbyShareSupportedForBrowserContext(
      content::BrowserContext* context);

  // Returns the NearbySharingService associated with |context|.
  static NearbySharingService* GetForBrowserContext(
      content::BrowserContext* context);

  // Forces IsNearbyShareSupportedForBrowserContext() to return |is_supported|.
  static void SetIsNearbyShareSupportedForBrowserContextForTesting(
      bool is_supported);

 private:
  friend base::NoDestructor<NearbySharingServiceFactory>;

  NearbySharingServiceFactory();
  ~NearbySharingServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_
