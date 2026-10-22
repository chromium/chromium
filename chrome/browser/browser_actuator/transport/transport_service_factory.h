// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_TRANSPORT_TRANSPORT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_TRANSPORT_TRANSPORT_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace browser_actuator::transport {

class TransportService;

// Creates the per-profile `TransportService`, which owns the bi-directional
// channel to the server and the sessions multiplexed over it.
//
// This factory is where Chrome decides how the transport is configured: which
// endpoints it talks to and whether it records sessions for
// chrome://browser-actuator-internals. The transport itself does not read any
// of its clients' features.
//
// Features that send or receive payloads over the transport depend on this
// factory and register their `TransportHandlerFactory` with
// `GetForProfile(profile)->GetChannel()->GetHandlerFactoryRegistry()`.
class TransportServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the `TransportService` for `profile`, or nullptr if
  // `kBrowserActuator` is disabled or `profile` is not a regular profile (for
  // example an OffTheRecord or Guest profile).
  static TransportService* GetForProfile(Profile* profile);

  static TransportServiceFactory* GetInstance();

  TransportServiceFactory(const TransportServiceFactory&) = delete;
  TransportServiceFactory& operator=(const TransportServiceFactory&) = delete;

 private:
  friend base::NoDestructor<TransportServiceFactory>;

  TransportServiceFactory();
  ~TransportServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace browser_actuator::transport

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_TRANSPORT_TRANSPORT_SERVICE_FACTORY_H_
