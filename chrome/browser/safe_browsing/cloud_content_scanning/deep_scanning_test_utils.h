// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_TEST_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
      const absl::optional<std::string>& expected_source,
      const absl::optional<std::string>& expected_destination,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_username,
      const absl::optional<std::string>& expected_scan_id);

  void ExpectSensitiveDataEvent(
      const std::string& expected_url,
      const absl::optional<std::string>& expected_source,
      const absl::optional<std::string>& expected_destination,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_trigger,
      const enterprise_connectors::ContentAnalysisResponse::Result&
          expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_username,
      const std::string& expected_scan_id);

  void ExpectSensitiveDataEvents(
      const std::string& expected_url,
      const absl::optional<std::string>& expected_source,
      const absl::optional<std::string>& expected_destination,
      const std::vector<std::string>& expected_filenames,
      const std::vector<std::string>& expected_sha256s,
      const std::string& expected_trigger,
      const std::vector<enterprise_connectors::ContentAnalysisResponse::Result>&
          expected_dlp_verdicts,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::vector<std::string>& expected_results,
      const std::string& expected_username,
      const std::vector<std::string>& expected_scan_ids);

  void ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const enterprise_connectors::ContentAnalysisResponse::Result&
          expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_username,
      const std::string& expected_scan_id);

  void ExpectSensitiveDataEventAndDangerousDeepScanningResult(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const enterprise_connectors::ContentAnalysisResponse::Result&
          expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_username,
      const std::string& expected_scan_id);

  void ExpectUnscannedFileEvent(
      const std::string& expected_url,
      const absl::optional<std::string>& expected_source,
      const absl::optional<std::string>& expected_destination,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_trigger,
      const std::string& expected_reason,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_username);

  void ExpectUnscannedFileEvents(
      const std::string& expected_url,
      const std::vector<std::string>& expected_filenames,
      const std::vector<std::string>& expected_sha256s,
      const std::string& expected_trigger,
      const std::string& expected_reason,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_username);

  void ExpectDangerousDownloadEvent(
      const std::string& expected_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_username,
      const absl::optional<std::string>& expected_scan_id);

  void ExpectLoginEvent(const std::string& expected_url,
                        bool expected_is_federated,
                        const std::string& expected_federated_origin,
                        const std::string& expected_profile_username,
                        const std::u16string& expected_login_username);

  void ExpectPasswordBreachEvent(
      const std::string& expected_trigger,
      const std::vector<std::pair<std::string, std::u16string>>&
          expected_identities,
      const std::string& expected_username);

  void ExpectNoReport();

  // Closure to run once all expected events are validated.
  void SetDoneClosure(base::RepeatingClosure closure);

 private:
  void ValidateReport(const base::Value::Dict* report);
  void ValidateFederatedOrigin(const base::Value::Dict* value);
  void ValidateIdentities(const base::Value::Dict* value);
  void ValidateMimeType(const base::Value::Dict* value);
  void ValidateDlpVerdict(
      const base::Value::Dict* value,
      const enterprise_connectors::ContentAnalysisResponse::Result& result);
  void ValidateDlpRule(const base::Value::Dict* value,
                       const enterprise_connectors::ContentAnalysisResponse::
                           Result::TriggeredRule& expected_rule);
  void ValidateFilenameMappedAttributes(const base::Value::Dict* value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const absl::optional<std::string>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const absl::optional<std::u16string>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const absl::optional<int>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const absl::optional<bool>& expected_value);

  raw_ptr<policy::MockCloudPolicyClient> client_;

  std::string event_key_;
  absl::optional<std::string> url_;
  absl::optional<std::string> source_;
  absl::optional<std::string> destination_;
  absl::optional<std::string> trigger_ = absl::nullopt;
  absl::optional<std::string> threat_type_ = absl::nullopt;
  absl::optional<std::string> unscanned_reason_ = absl::nullopt;
  absl::optional<int64_t> content_size_ = absl::nullopt;
  raw_ptr<const std::set<std::string>> mimetypes_ = nullptr;
  std::string username_;
  absl::optional<bool> is_federated_ = absl::nullopt;
  absl::optional<std::string> federated_origin_ = absl::nullopt;
  absl::optional<std::u16string> login_user_name_ = absl::nullopt;
  absl::optional<std::vector<std::pair<std::string, std::u16string>>>
      password_breach_identities_ = absl::nullopt;

  // When multiple files generate events, we don't necessarily know in which
  // order they will be reported. As such, we use maps to ensure all of them
  // are called as expected.
  base::flat_map<std::string,
                 enterprise_connectors::ContentAnalysisResponse::Result>
      dlp_verdicts_;
  base::flat_map<std::string, std::string> results_;
  base::flat_map<std::string, std::string> filenames_and_hashes_;
  base::flat_map<std::string, std::string> scan_ids_;

  base::RepeatingClosure done_closure_;
};

// Helper functions that set Connector policies for testing.
void SetAnalysisConnector(PrefService* prefs,
                          enterprise_connectors::AnalysisConnector connector,
                          const std::string& pref_value,
                          bool machine_scope = true);
void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const std::set<std::string>& enabled_event_names = std::set<std::string>(),
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events =
            std::map<std::string, std::vector<std::string>>(),
    bool machine_scope = true);
void ClearAnalysisConnector(PrefService* prefs,
                            enterprise_connectors::AnalysisConnector connector);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Helper function to set the profile DM token. It installs a
// MockCloudPolicyClient with |dm_token| into |profile|'s UserCloudPolicyManager
// to simulate |profile|'s DM token.
void SetProfileDMToken(Profile* profile, const std::string& dm_token);
#endif

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_TEST_UTILS_H_
