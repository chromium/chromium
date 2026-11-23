// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class NetworkContextService;

// Factory for creating the `KeyedService` `NetworkContextService`. Supported
// off-the-record profiles can either redirect to the original profile's
// service (legacy behavior for unmigrated consumers) or receive their own
// isolated in-memory `NetworkContextService` (for migrated consumers). Features
// using this network context are expected to behave correctly while
// off-the-record.
class NetworkContextServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static NetworkContextServiceFactory* GetInstance();

  // Returns the `NetworkContextService` for `context` (which must not be
  // null), creating it if needed, or `nullptr` if `context` does not support a
  // `NetworkContextService`. When `use_same_instance_in_off_the_record` is
  // `true`, off-the-record profiles are redirected to their original profile's
  // service; when `false`, supported off-the-record profiles receive their own
  // isolated in-memory instance.
  // TODO(crbug.com/587263559): Remove `use_same_instance_in_off_the_record`
  // once all off-the-record consumers are migrated.
  static NetworkContextService* GetForBrowserContext(
      content::BrowserContext* context,
      bool use_same_instance_in_off_the_record);

  // Returns the existing `NetworkContextService` for `context` (which must not
  // be null) without creating one, or `nullptr` if no service has been created
  // or `context` does not support a `NetworkContextService`.
  // `use_same_instance_in_off_the_record` has the same meaning as in
  // `GetForBrowserContext()`.
  static NetworkContextService* GetForBrowserContextIfExistsForTesting(
      content::BrowserContext* context,
      bool use_same_instance_in_off_the_record);

 private:
  friend base::NoDestructor<NetworkContextServiceFactory>;

  NetworkContextServiceFactory();
  ~NetworkContextServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_FACTORY_H_
