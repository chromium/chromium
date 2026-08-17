// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_uploader_desktop.h"

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/policy/core/common/policy_logger.h"

namespace enterprise_reporting {

SaasUsageReportUploaderDesktop::SaasUsageReportUploaderDesktop()
    : helper_("browser", "SaaS usage", nullptr), profile_(nullptr) {}

SaasUsageReportUploaderDesktop::SaasUsageReportUploaderDesktop(
    Profile* profile)
    : helper_("profile", "SaaS usage", profile), profile_(profile) {
  CHECK(profile);
}

SaasUsageReportUploaderDesktop::~SaasUsageReportUploaderDesktop() = default;

void SaasUsageReportUploaderDesktop::UploadReport(
    const ::chrome::cros::reporting::proto::SaasUsageReportEvent& report,
    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
        upload_callback) {
  auto context = helper_.PrepareUpload(IsProfileReporting());
  if (!context) {
    return;
  }

  VLOG_POLICY(1, REPORTING)
      << "Sending " << helper_.reporting_scope() << " " << helper_.event_name()
      << " report with " << report.domain_metrics_size() << " domain metrics.";

  ::chrome::cros::reporting::proto::Event event;
  *event.mutable_saas_usage_report_event() = report;

  context->client->ReportSaasUsageEvent(event, context->per_profile,
                                        context->dm_token,
                                        std::move(upload_callback));
}

bool SaasUsageReportUploaderDesktop::IsProfileReporting() const {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  if (!profile_) {
    return false;
  }
  // For SaaS reports, we use the browser client for affiliated profiles to
  // match the behaviour of the realtime reporting pipeline.
  // Server will use profile id from the report to distinguish between browser
  // and profile reports.
  return !enterprise_util::IsProfileAffiliated(profile_);
#endif
}

}  // namespace enterprise_reporting
