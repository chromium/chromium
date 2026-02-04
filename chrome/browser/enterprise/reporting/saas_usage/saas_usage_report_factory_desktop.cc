// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_factory_desktop.h"

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"

namespace enterprise_reporting {

SaasUsageReportFactoryDesktop::SaasUsageReportFactoryDesktop(Profile* profile)
    : profile_(profile) {}

std::optional<std::string> SaasUsageReportFactoryDesktop::GetProfileId() {
  if (!profile_) {
    return std::nullopt;
  }
  auto* profile_id_service =
      enterprise::ProfileIdServiceFactory::GetForProfile(profile_);
  CHECK(profile_id_service)
      << "Saas usage report is only produced for regular profiles"
         ", so profile id service should always be present.";

  return profile_id_service->GetProfileId();
}

bool SaasUsageReportFactoryDesktop::IsProfileAffiliated() {
  if (!profile_) {
    return false;
  }
  return enterprise_util::IsProfileAffiliated(profile_);
}

}  // namespace enterprise_reporting
