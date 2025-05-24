// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIALS_KEYED_SERVICE_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIALS_KEYED_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

class Profile;
class OptimizationGuideKeyedService;

namespace digital_credentials {

// Service to handle Digital Credentials API specific initializations.
class DigitalCredentialsKeyedService : public KeyedService {
 public:
  explicit DigitalCredentialsKeyedService(
      OptimizationGuideKeyedService& optimization_guide_service);
  ~DigitalCredentialsKeyedService() override;

  DigitalCredentialsKeyedService(const DigitalCredentialsKeyedService&) =
      delete;
  DigitalCredentialsKeyedService& operator=(
      const DigitalCredentialsKeyedService&) = delete;

  // Checks if the given URL is considered low friction.
  bool IsLowFrictionUrl(const GURL& to_check) const;

 private:
  const raw_ref<OptimizationGuideKeyedService> optimization_guide_service_;
};

// Factory for creating DigitalCredentialsKeyedService.
class DigitalCredentialsKeyedServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static DigitalCredentialsKeyedService* GetForProfile(Profile* profile);

  static DigitalCredentialsKeyedServiceFactory* GetInstance();

  DigitalCredentialsKeyedServiceFactory(
      const DigitalCredentialsKeyedServiceFactory&) = delete;
  DigitalCredentialsKeyedServiceFactory& operator=(
      const DigitalCredentialsKeyedServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<DigitalCredentialsKeyedServiceFactory>;

  DigitalCredentialsKeyedServiceFactory();
  ~DigitalCredentialsKeyedServiceFactory() override;

  // ProfileKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace digital_credentials

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIALS_KEYED_SERVICE_H_
