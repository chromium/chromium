// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_DEEP_SCANNING_TEST_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_DEEP_SCANNING_TEST_UTILS_H_

#include <optional>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace policy {
class MockCloudPolicyClient;
}

namespace enterprise_connectors::test {

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
      const std::string& expected_tab_url,
      const std::string& expected_source,
      const std::string& expected_destination,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::optional<std::string>& expected_scan_id);

  void ExpectSensitiveDataEvent(
      const std::string& expected_url,
      const std::string& expected_tab_url,
      const std::string& expected_source,
      const std::string& expected_destination,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_trigger,
      const ContentAnalysisResponse::Result& expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      std::optional<int64_t> expected_content_size,
      const std::string& expected_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::string& expected_scan_id,
      const std::optional<std::string>& expected_content_transfer_method,
      const std::optional<std::u16string>& expected_user_justification);

  void ExpectDataControlsSensitiveDataEvent(
      const std::string& expected_url,
      const std::string& expected_tab_url,
      const std::string& expected_source,
      const std::string& expected_destination,
      const std::set<std::string>* expected_mimetypes,
      const std::string& expected_trigger,
      const data_controls::Verdict::TriggeredRules& triggered_rules,
      const std::string& expected_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      int64_t expected_content_size);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void ExpectDataMaskingEvent(
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      extensions::api::enterprise_reporting_private::DataMaskingEvent
          expected_event);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  void ExpectSensitiveDataEvents(
      const std::string& expected_url,
      const std::string& expected_tab_url,
      const std::string& expected_source,
      const std::string& expected_destination,
      const std::vector<std::string>& expected_filenames,
      const std::vector<std::string>& expected_sha256s,
      const std::string& expected_trigger,
      const std::vector<ContentAnalysisResponse::Result>& expected_dlp_verdicts,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::vector<std::string>& expected_results,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::vector<std::string>& expected_scan_ids,
      const std::optional<std::string>& expected_content_transfer_method,
      const std::optional<std::u16string>& expected_user_justification);

  void ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
      const std::string& expected_url,
      const std::string& expected_tab_url,
      const std::string& expected_source,
      const std::string& expected_destination,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const ContentAnalysisResponse::Result& expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::string& expected_scan_id,
      const std::optional<std::string>& expected_content_transfer_method);

  void ExpectSensitiveDataEventAndDangerousDeepScanningResult(
      const std::string& expected_url,
      const std::string& expected_tab_url,
      const std::string& expected_source,
      const std::string& expected_destination,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const ContentAnalysisResponse::Result& expected_dlp_verdict,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::string& expected_scan_id);

  void ExpectUnscannedFileEvent(
      const std::string& expected_url,
      const std::string& expected_tab_url,
      const std::string& expected_source,
      const std::string& expected_destination,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_trigger,
      const std::string& expected_reason,
      const std::set<std::string>* expected_mimetypes,
      std::optional<int64_t> expected_content_size,
      const std::string& expected_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::optional<std::string>& expected_content_transfer_method);

  void ExpectUnscannedFileEvents(
      const std::string& expected_url,
      const std::string& expected_tab_url,
      const std::string& expected_source,
      const std::string& expected_destination,
      const std::vector<std::string>& expected_filenames,
      const std::vector<std::string>& expected_sha256s,
      const std::string& expected_trigger,
      const std::string& expected_reason,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::optional<std::string>& expected_content_transfer_method);

  void ExpectDangerousDownloadEvent(
      const std::string& expected_url,
      const std::string& expected_tab_url,
      const std::string& expected_filename,
      const std::string& expected_sha256,
      const std::string& expected_threat_type,
      const std::string& expected_trigger,
      const std::set<std::string>* expected_mimetypes,
      int64_t expected_content_size,
      const std::string& expected_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier);

  void ExpectLoginEvent(const std::string& expected_url,
                        bool expected_is_federated,
                        const std::string& expected_federated_origin,
                        const std::string& expected_profile_username,
                        const std::string& expected_profile_identifier,
                        const std::u16string& expected_login_username);

  void ExpectPasswordBreachEvent(
      const std::string& expected_trigger,
      const std::vector<std::pair<std::string, std::u16string>>&
          expected_identities,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier);

  void ExpectURLFilteringInterstitialEvent(
      const std::string& expected_url,
      const std::string& expected_event_result,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      safe_browsing::RTLookupResponse expected_rt_lookup_response);

  void ExpectNoReport();

  // Closure to run once all expected events are validated.
  void SetDoneClosure(base::RepeatingClosure closure);

 private:
  void ValidateReport(const base::Value::Dict* report);
  void ValidateFederatedOrigin(const base::Value::Dict* value);
  void ValidateIdentities(const base::Value::Dict* value);
  void ValidateMimeType(const base::Value::Dict* value);
  void ValidateDlpVerdict(const base::Value::Dict* value,
                          const ContentAnalysisResponse::Result& result);
  void ValidateDlpRule(
      const base::Value::Dict* value,
      const ContentAnalysisResponse::Result::TriggeredRule& expected_rule);
  void ValidateRTLookupResponse(const base::Value::Dict* value);
  void ValidateThreatInfo(
      const base::Value::Dict* value,
      const safe_browsing::RTLookupResponse::ThreatInfo& expected_threat_info);
  void ValidateFilenameMappedAttributes(const base::Value::Dict* value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const std::optional<std::string>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const std::optional<std::u16string>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const std::optional<int>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const std::optional<bool>& expected_value);
  void ValidateDataControlsAttributes(const base::Value::Dict* event);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  void ValidateDataMaskingAttributes(const base::Value::Dict* event);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  raw_ptr<policy::MockCloudPolicyClient> client_;

  std::string event_key_;
  std::optional<std::string> url_;
  std::optional<std::string> tab_url_;
  std::optional<std::string> source_;
  std::optional<std::string> destination_;
  std::optional<std::string> trigger_ = std::nullopt;
  std::optional<std::string> threat_type_ = std::nullopt;
  std::optional<std::string> unscanned_reason_ = std::nullopt;
  std::optional<std::string> content_transfer_method_ = std::nullopt;
  std::optional<std::u16string> user_justification_ = std::nullopt;
  std::optional<int64_t> content_size_ = std::nullopt;
  raw_ptr<const std::set<std::string>> mimetypes_ = nullptr;
  std::string username_;
  std::string profile_identifier_;
  std::optional<bool> is_federated_ = std::nullopt;
  std::optional<std::string> federated_origin_ = std::nullopt;
  std::optional<std::u16string> login_user_name_ = std::nullopt;
  std::optional<std::vector<std::pair<std::string, std::u16string>>>
      password_breach_identities_ = std::nullopt;
  std::optional<std::string> url_filtering_event_result_ = std::nullopt;
  std::optional<safe_browsing::RTLookupResponse> rt_lookup_response_ =
      std::nullopt;
  std::optional<std::string> data_controls_result_ = std::nullopt;
  data_controls::Verdict::TriggeredRules data_controls_triggered_rules_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // `DataMaskingEvent`'s copy constructor is deleted, so to keep
  // `EventReportValidator` copyable a lazy builder is used to store its
  // expected value.
  base::RepeatingCallback<
      extensions::api::enterprise_reporting_private::DataMaskingEvent()>
      expected_data_masking_rules_builder_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // When multiple files generate events, we don't necessarily know in which
  // order they will be reported. As such, we use maps to ensure all of them
  // are called as expected.
  base::flat_map<std::string, ContentAnalysisResponse::Result> dlp_verdicts_;
  base::flat_map<std::string, std::string> results_;
  base::flat_map<std::string, std::string> filenames_and_hashes_;
  base::flat_map<std::string, std::string> scan_ids_;

  base::RepeatingClosure done_closure_;
};

// Helper class to set up tests to use `EventReportValidator`.
class EventReportValidatorHelper {
 public:
  explicit EventReportValidatorHelper(Profile* profile,
                                      bool browser_test = false);
  ~EventReportValidatorHelper();

  EventReportValidator CreateValidator();

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
// Helper functions that set Connector policies for testing.
void SetAnalysisConnector(PrefService* prefs,
                          AnalysisConnector connector,
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
void ClearAnalysisConnector(PrefService* prefs, AnalysisConnector connector);
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Helper function to set the profile DM token. It installs a
// MockCloudPolicyClient with |dm_token| into |profile|'s UserCloudPolicyManager
// to simulate |profile|'s DM token.
void SetProfileDMToken(Profile* profile, const std::string& dm_token);
#endif

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_DEEP_SCANNING_TEST_UTILS_H_
