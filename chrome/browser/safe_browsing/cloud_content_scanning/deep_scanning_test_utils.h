// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_TEST_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace base {
class Value;
}

namespace policy {
class MockCloudPolicyClient;
}

namespace safe_browsing {

// Helper class that represents a report that's expected from a test. The
// non-optional fields are expected for every kind of Deep Scanning reports.
// The optional ones are not present on every Deep Scanning event. The mimetype
// field is handled by a pointer to a set since different builds/platforms can
// return different mimetype strings for the same file.
class EventReportValidator {
 public:
  explicit EventReportValidator(policy::MockCloudPolicyClient* client);
  ~EventReportValidator();

  void ExpectDangerousDeepScanningResult(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const std::set<std::string>* expected_mimetypes,
      int expected_content_size,
      const std::string& expected_result);

  void ExpectSensitiveDataEvent(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_trigger,
      const enterprise_connectors::ContentAnalysisResponse::Result&
          expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      int expected_content_size,
      const std::string& expected_result);

  void ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const enterprise_connectors::ContentAnalysisResponse::Result&
          expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      int expected_content_size,
      const std::string& expected_result);

  void ExpectSensitiveDataEventAndDangerousDeepScanningResult(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const enterprise_connectors::ContentAnalysisResponse::Result&
          expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      int expected_content_size,
      const std::string& expected_result);

  void ExpectUnscannedFileEvent(const std::string& expected_url,
                                const std::string& expected_filename,
                                const std::string& expected_sha256,
                                const std::string& expected_trigger,
                                const std::string& expected_reason,
                                const std::set<std::string>* expected_mimetypes,
                                int expected_content_size,
                                const std::string& expected_result);

  void ExpectDangerousDownloadEvent(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const std::set<std::string>* expected_mimetypes,
      int expected_content_size,
      const std::string& expected_result);

  // Closure to run once all expected events are validated.
  void SetDoneClosure(base::RepeatingClosure closure);

 private:
  void ValidateReport(base::Value* report);
  void ValidateMimeType(base::Value* value);
  void ValidateDlpVerdict(base::Value* value);
  void ValidateDlpRule(base::Value* value,
                       const enterprise_connectors::ContentAnalysisResponse::
                           Result::TriggeredRule& expected_rule);
  void ValidateField(base::Value* value,
                     const std::string& field_key,
                     const base::Optional<std::string>& expected_value);
  void ValidateField(base::Value* value,
                     const std::string& field_key,
                     const base::Optional<int>& expected_value);
  void ValidateField(base::Value* value,
                     const std::string& field_key,
                     const base::Optional<bool>& expected_value);

  policy::MockCloudPolicyClient* client_;

  std::string event_key_;
  std::string url_;
  std::string filename_;
  std::string sha256_;
  std::string trigger_;
  base::Optional<enterprise_connectors::ContentAnalysisResponse::Result>
      dlp_verdict_ = base::nullopt;
  base::Optional<std::string> threat_type_ = base::nullopt;
  base::Optional<std::string> unscanned_reason_ = base::nullopt;
  base::Optional<int> content_size_ = base::nullopt;
  const std::set<std::string>* mimetypes_ = nullptr;
  base::Optional<std::string> result_ = base::nullopt;

  base::RepeatingClosure done_closure_;
};

// Helper functions that set matching connector policies values from legacy
// policy values.
void SetDlpPolicyForConnectors(CheckContentComplianceValues state);
void SetMalwarePolicyForConnectors(SendFilesForMalwareCheckValues state);
void SetDelayDeliveryUntilVerdictPolicyForConnectors(
    DelayDeliveryUntilVerdictValues state);
void SetAllowPasswordProtectedFilesPolicyForConnectors(
    AllowPasswordProtectedFilesValues state);
void SetBlockUnsupportedFileTypesPolicyForConnectors(
    BlockUnsupportedFiletypesValues state);
void SetBlockLargeFileTransferPolicyForConnectors(
    BlockLargeFileTransferValues state);
void AddUrlsToCheckComplianceOfDownloadsForConnectors(
    const std::vector<std::string>& urls);
void AddUrlsToNotCheckComplianceOfUploadsForConnectors(
    const std::vector<std::string>& urls);
void AddUrlsToCheckForMalwareOfUploadsForConnectors(
    const std::vector<std::string>& urls);
void AddUrlsToNotCheckForMalwareOfDownloadsForConnectors(
    const std::vector<std::string>& urls);
void AddUrlToListForConnectors(const char* pref_name, const std::string& url);
void ClearUrlsToCheckComplianceOfUploadsForConnectors();
void ClearUrlsToCheckForMalwareOfUploadsForConnectors();
void ClearUrlsToCheckComplianceOfDownloadsForConnectors();
void ClearUrlsToCheckForMalwareOfDownloadsForConnectors();
void SetOnSecurityEventReporting(bool enabled);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_TEST_UTILS_H_
