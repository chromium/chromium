// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace net {
class ServerCertificateDatabaseService;

class ServerCertificateDatabaseServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns the ServerCertificateDatabaseService for |browser_context|.
  static ServerCertificateDatabaseService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Returns the ServerCertificateDatabaseServiceFactory singleton.
  static ServerCertificateDatabaseServiceFactory* GetInstance();

  ServerCertificateDatabaseServiceFactory(
      const ServerCertificateDatabaseServiceFactory&) = delete;
  ServerCertificateDatabaseServiceFactory& operator=(
      const ServerCertificateDatabaseServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ServerCertificateDatabaseServiceFactory>;

  ServerCertificateDatabaseServiceFactory();
  ~ServerCertificateDatabaseServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace net

#endif  // CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_FACTORY_H_
