// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_REPORTING_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_REPORTING_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace data_controls {

// Keyed service that provides an interface to report Data Control events.
class ReportingService : public KeyedService {
 public:
  ~ReportingService() override;

 protected:
  friend class ReportingServiceFactory;

  explicit ReportingService(content::BrowserContext& browser_context);

 private:
  // `profile_` is initialized with the browser_context passed in the
  // constructor.
  const raw_ref<Profile> profile_;
};

class ReportingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static ReportingServiceFactory* GetInstance();

  ReportingServiceFactory(const ReportingServiceFactory&) = delete;
  ReportingServiceFactory& operator=(const ReportingServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ReportingServiceFactory>;

  ReportingServiceFactory();
  ~ReportingServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_REPORTING_SERVICE_H_
