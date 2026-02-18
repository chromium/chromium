// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_DESKTOP_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"

namespace enterprise_reporting {

// Base class for uploading SaaS usage reports on desktop platforms.
// Encapsulates the logic for sending reports using the RealtimeReportingClient.
// Derived classes are used to provide the appropriate client and settings for
// the reports.
class SaasUsageReportUploaderDesktop : public SaasUsageReportUploader {
 public:
  explicit SaasUsageReportUploaderDesktop(std::string_view uploader_name);
  SaasUsageReportUploaderDesktop(const SaasUsageReportUploaderDesktop&) =
      delete;
  SaasUsageReportUploaderDesktop& operator=(
      const SaasUsageReportUploaderDesktop&) = delete;
  ~SaasUsageReportUploaderDesktop() override = default;

  // SaasUsageReportUploader:
  void UploadReport(
      const ::chrome::cros::reporting::proto::SaasUsageReportEvent& report,
      base::OnceCallback<void(bool)> upload_callback) override;

 protected:
  virtual enterprise_connectors::RealtimeReportingClientBase*
  GetRealTimeReportingClient() = 0;
  virtual bool ShouldUseProfileClient() = 0;
  virtual std::optional<std::string> GetDMToken() = 0;

  const std::string uploader_name_;
};

// Uploader implementation for profile-level SaaS usage reports.
// Uses the profile's RealtimeReportingClient and determines reporting settings
// based on whether the profile is affiliated.
class SaasUsageProfileReportUploaderDesktop final
    : public SaasUsageReportUploaderDesktop {
 public:
  explicit SaasUsageProfileReportUploaderDesktop(Profile* profile);
  SaasUsageProfileReportUploaderDesktop(
      const SaasUsageProfileReportUploaderDesktop&) = delete;
  SaasUsageProfileReportUploaderDesktop& operator=(
      const SaasUsageProfileReportUploaderDesktop&) = delete;
  ~SaasUsageProfileReportUploaderDesktop() override = default;

 protected:
  // SaasUsageReportUploaderDesktop:
  enterprise_connectors::RealtimeReportingClientBase*
  GetRealTimeReportingClient() override;
  bool ShouldUseProfileClient() override;
  std::optional<std::string> GetDMToken() override;

 private:
  raw_ref<Profile> profile_;
};

// Uploader implementation for browser-level SaaS usage reports.
// Attempts to find a usable RealtimeReportingClient from any loaded profile
// and uses browser-level reporting settings.
class SaasUsageBrowserReportUploaderDesktop final
    : public SaasUsageReportUploaderDesktop {
 public:
  SaasUsageBrowserReportUploaderDesktop();
  SaasUsageBrowserReportUploaderDesktop(
      const SaasUsageBrowserReportUploaderDesktop&) = delete;
  SaasUsageBrowserReportUploaderDesktop& operator=(
      const SaasUsageBrowserReportUploaderDesktop&) = delete;
  ~SaasUsageBrowserReportUploaderDesktop() override = default;

 protected:
  // SaasUsageReportUploaderDesktop:
  enterprise_connectors::RealtimeReportingClientBase*
  GetRealTimeReportingClient() override;
  bool ShouldUseProfileClient() override;
  std::optional<std::string> GetDMToken() override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_DESKTOP_H_
