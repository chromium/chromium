// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_SERVICE_H_

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_url_matcher.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace enterprise_reporting {

using LegacyTechReportTrigger = base::RepeatingCallback<void(
    const LegacyTechReportGenerator::LegacyTechData& data)>;

// A `KeyedService` provides an API that allows content layor to upload report.
// It will trigger a report if the event URL matches the policy setting.
class LegacyTechService : public KeyedService {
 public:
  LegacyTechService(Profile* profile, LegacyTechReportTrigger trigger);
  LegacyTechService(const LegacyTechService&) = delete;
  LegacyTechService& operator=(const LegacyTechService&) = delete;
  ~LegacyTechService() override;

  void ReportEvent(const std::string& type,
                   const GURL& url,
                   const std::string& filename,
                   uint64_t line,
                   uint64_t column) const;

 private:
  LegacyTechURLMatcher url_matcher_;
  LegacyTechReportTrigger trigger_;
};

class LegacyTechServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static LegacyTechServiceFactory* GetInstance();
  static LegacyTechService* GetForProfile(Profile* profile);

  LegacyTechServiceFactory(const LegacyTechServiceFactory&) = delete;
  LegacyTechServiceFactory& operator=(const LegacyTechServiceFactory&) = delete;

  void SetReportTrigger(LegacyTechReportTrigger&& trigger);

 protected:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

 private:
  void ReportEventImpl(const LegacyTechReportGenerator::LegacyTechData& data);

  friend base::NoDestructor<LegacyTechServiceFactory>;

  LegacyTechServiceFactory();
  ~LegacyTechServiceFactory() override;

  LegacyTechReportTrigger trigger_;

  std::vector<LegacyTechReportGenerator::LegacyTechData> pending_data_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_LEGACY_TECH_LEGACY_TECH_SERVICE_H_
