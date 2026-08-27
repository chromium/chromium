// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace password_manager {

class OnDeviceEncryptionMetricsReporter;

class OnDeviceEncryptionMetricsReporterFactory
    : public ProfileKeyedServiceFactory {
 public:
  static OnDeviceEncryptionMetricsReporter* GetForProfile(Profile* profile);
  static OnDeviceEncryptionMetricsReporterFactory* GetInstance();

  OnDeviceEncryptionMetricsReporterFactory(
      const OnDeviceEncryptionMetricsReporterFactory&) = delete;
  OnDeviceEncryptionMetricsReporterFactory& operator=(
      const OnDeviceEncryptionMetricsReporterFactory&) = delete;

 private:
  friend class base::NoDestructor<OnDeviceEncryptionMetricsReporterFactory>;

  OnDeviceEncryptionMetricsReporterFactory();
  ~OnDeviceEncryptionMetricsReporterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_
