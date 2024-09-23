// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_FACTORY_H_
#define CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class CertDbInitializer;

// Factory that manages creation of CertDbInitializer. The initialization is
// handled differently depending on the environment:
// * On real ChromeOS devices with TPMs:
// ** if the user is affiliated: CertDbInitializer is automatically
// created right after its profile is created. It receives a path to software
// cert database and slot IDs for Chaps from Ash and uses them.
// ** if the user is not affiliated: TODO(b/197082753): not officially supported
// yet, handled as if there's no TPM.
// * In emulated environments (e.g. when running ChromeOS on Linux) and in the
// future on ChromeOS without TPMs: Same as real ChromeOS, but Ash only sends
// the software database path.
// * In browsertests: CertDbInitializer is not created by default because it
// requires crosapi mojo interface. It is configured through the
// `SetCreateWithBrowserContextForTesting()` method. This can be overridden by
// individual tests or they can create their own instances of the service.
// * In unittests: CertDbInitializer is not created by default (see
// `ServiceIsNULLWhileTesting()`).
class CertDbInitializerFactory : public ProfileKeyedServiceFactory {
 public:
  static CertDbInitializerFactory* GetInstance();
  static CertDbInitializer* GetForBrowserContext(
      content::BrowserContext* context);

  // Configures whether CertDbInitializer should be automatically created on
  // profile creation in browser tests.
  // Currently it is configured that in browser tests the service is not created
  // by default. Individual tests can override it when needed.
  void SetCreateWithBrowserContextForTesting(bool should_create);
  // Configures whether CertDbInitializer should be automatically created when
  // something is trying to use it. In production it is created together with
  // BrowserContext, which can make it hard for browser tests to set up
  // everything in time.
  void SetCreateOnDemandForTesting(bool should_create);

 private:
  friend class base::NoDestructor<CertDbInitializerFactory>;

  CertDbInitializerFactory();
  ~CertDbInitializerFactory() override = default;

  // BrowserStateKeyedServiceFactory
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool should_create_with_browser_context_ = true;
  bool should_create_on_demand_ = false;
};

#endif  // CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_FACTORY_H_
