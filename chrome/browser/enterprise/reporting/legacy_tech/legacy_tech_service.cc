// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_service.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace enterprise_reporting {

LegacyTechService::LegacyTechService(Profile* profile,
                                     LegacyTechReportTrigger trigger)
    : url_matcher_(profile), trigger_(trigger) {
  DCHECK(trigger_);
}

LegacyTechService::~LegacyTechService() = default;

void LegacyTechService::ReportEvent(const std::string& type,
                                    const GURL& url,
                                    const std::string& filename,
                                    uint64_t line,
                                    uint64_t column) const {
  absl::optional<std::string> matched_url = url_matcher_.GetMatchedURL(url);
  VLOG(2) << "Get report for URL " << url
          << (matched_url ? " and matches a policy."
                          : " without matching any policie.");
  if (!matched_url) {
    return;
  }

  LegacyTechReportGenerator::LegacyTechData data = {
      type,
      /*timestamp=*/base::Time::Now(),
      url,
      *matched_url,
      filename,
      line,
      column};

  trigger_.Run(data);
}

// static
LegacyTechServiceFactory* LegacyTechServiceFactory::GetInstance() {
  static base::NoDestructor<LegacyTechServiceFactory> instance;
  return instance.get();
}

// static
LegacyTechService* LegacyTechServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LegacyTechService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

std::unique_ptr<KeyedService>
LegacyTechServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // Legacy tech report is always enabled and the callback must be set before
  // any report created.
  // Report uploading will be decided individually for every single report.
  // Use base::Unretained as the factory is base::NoDestructor.
  return std::make_unique<LegacyTechService>(
      profile, base::BindRepeating(&LegacyTechServiceFactory::ReportEventImpl,
                                   base::Unretained(GetInstance())));
}

void LegacyTechServiceFactory::SetReportTrigger(
    LegacyTechReportTrigger&& trigger) {
  trigger_ = std::move(trigger);
  for (auto& data : pending_data_) {
    trigger_.Run(data);
  }
  pending_data_.clear();
}

void LegacyTechServiceFactory::ReportEventImpl(
    const LegacyTechReportGenerator::LegacyTechData& data) {
  if (!trigger_) {
    // CBCM initialization is async, in case a report is triggered before.
    pending_data_.push_back(data);
    return;
  }
  trigger_.Run(data);
}

LegacyTechServiceFactory::LegacyTechServiceFactory()
    : ProfileKeyedServiceFactory(
          "LegacyTechReporting",
          ProfileSelections::BuildRedirectedInIncognito()) {}
LegacyTechServiceFactory::~LegacyTechServiceFactory() = default;

}  // namespace enterprise_reporting
