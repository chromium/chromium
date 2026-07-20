// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class V5GetHashProtocolManager;

// Singleton that owns V5GetHashProtocolManager objects, one for each active
// Profile (including off-the-record profiles).
class V5GetHashProtocolManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given `profile`.
  // If the service already exists, return its pointer.
  static V5GetHashProtocolManager* GetForProfile(Profile* profile);

  static V5GetHashProtocolManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Get the singleton instance.
  static V5GetHashProtocolManagerFactory* GetInstance();

  V5GetHashProtocolManagerFactory(const V5GetHashProtocolManagerFactory&) =
      delete;
  V5GetHashProtocolManagerFactory& operator=(
      const V5GetHashProtocolManagerFactory&) = delete;

 private:
  friend base::NoDestructor<V5GetHashProtocolManagerFactory>;

  V5GetHashProtocolManagerFactory();
  ~V5GetHashProtocolManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_
