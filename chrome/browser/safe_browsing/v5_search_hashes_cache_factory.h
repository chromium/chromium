// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_V5_SEARCH_HASHES_CACHE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_V5_SEARCH_HASHES_CACHE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class V5SearchHashesCache;

// Singleton that owns V5SearchHashesCache objects, one for each active Profile
// (including off-the-record profiles).
class V5SearchHashesCacheFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given `profile`.
  // If the service already exists, return its pointer.
  static V5SearchHashesCache* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static V5SearchHashesCacheFactory* GetInstance();

  V5SearchHashesCacheFactory(const V5SearchHashesCacheFactory&) = delete;
  V5SearchHashesCacheFactory& operator=(const V5SearchHashesCacheFactory&) =
      delete;

 private:
  friend base::NoDestructor<V5SearchHashesCacheFactory>;

  V5SearchHashesCacheFactory();
  ~V5SearchHashesCacheFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_V5_SEARCH_HASHES_CACHE_FACTORY_H_
