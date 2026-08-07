// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_DEEP_SCANNING_TEST_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_DEEP_SCANNING_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class Profile;

namespace policy {
class MockCloudPolicyClient;
}

namespace content {
class BrowserContext;
}

class KeyedService;

namespace enterprise_connectors::test {

// Helper class that represents a report that's expected from a test. The
// non-optional fields are expected for every kind of Deep Scanning reports.
// The optional ones are not present on every Deep Scanning event. The mimetype
// field is handled by a pointer to a set since different builds/platforms can
// return different mimetype strings for the same file.
class EventReportValidator : public EventReportValidatorBase {
 public:
  using EventReportValidatorBase::ExpectSensitiveDataEvent;

  explicit EventReportValidator(policy::MockCloudPolicyClient* client);
  ~EventReportValidator();

  void ExpectSensitiveDataEvents(
      const std::vector<chrome::cros::reporting::proto::DlpSensitiveDataEvent>
          expected_sensitive_data_events,
      const std::vector<std::string>& expected_filenames,
      const std::vector<std::string>& expected_sha256s,
      const std::vector<std::string>& expected_results,
      const std::vector<std::string>& expected_scan_ids);

  void ExpectSensitiveDataEventWarnThenBypass(
      chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_warn_event,
      chrome::cros::reporting::proto::DlpSensitiveDataEvent
          expected_bypass_event);

  void ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
      chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
          expected_dangerous_download_event,
      chrome::cros::reporting::proto::DlpSensitiveDataEvent
          expected_sensitive_data_event,
      const std::set<std::string>* expected_mimetypes);

  void ExpectUnscannedFileEvent(
      chrome::cros::reporting::proto::UnscannedFileEvent
          expected_unscanned_file_event);

  void ExpectUnscannedFileEvents(
      chrome::cros::reporting::proto::UnscannedFileEvent
          expected_unscanned_file_event,
      const std::vector<std::string>& expected_filenames,
      const std::vector<std::string>& expected_sha256s,
      const std::vector<std::string>& expected_scan_ids,
      const std::set<std::string>* expected_mimetypes);

  void ExpectDangerousDownloadEvent(
      chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
          expected_dangerous_download_event,
      const std::set<std::string>* expected_mimetypes = nullptr);

  void ExpectActiveUser(const std::string& user);
  void ExpectSourceActiveUser(const std::string& user);

  void ExpectFrameUrlChain(const std::vector<std::string>& frame_urls);

 private:

  std::string event_key_;
  std::optional<std::string> url_;
  std::optional<std::string> tab_url_;
  std::optional<std::string> source_;
  std::optional<std::string> destination_;
  std::optional<std::string> trigger_;
  std::optional<std::string> threat_type_;
  std::optional<std::string> unscanned_reason_;
  std::optional<std::string> content_transfer_method_;
  std::optional<std::u16string> user_justification_;
  std::optional<int64_t> content_size_;
  raw_ptr<const std::set<std::string>> mimetypes_ = nullptr;
  std::string username_;
  std::string profile_identifier_;
  std::optional<bool> is_federated_;
  std::optional<std::string> federated_origin_;
  std::optional<std::u16string> login_user_name_;
  std::optional<std::vector<std::pair<std::string, std::u16string>>>
      password_breach_identities_;
  std::optional<std::string> active_content_area_user_;
  std::optional<std::string> source_active_content_area_user_;
  std::optional<std::vector<std::string>> frame_urls_;

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

// Helper functions that set Connector policies for testing.
void SetAnalysisConnector(PrefService* prefs,
                          AnalysisConnector connector,
                          const std::string& pref_value,
                          bool machine_scope = true);
void ClearAnalysisConnector(PrefService* prefs, AnalysisConnector connector);

std::unique_ptr<KeyedService> BuildRealtimeReportingClient(
    content::BrowserContext* context);

#if !BUILDFLAG(IS_CHROMEOS)
// Helper function to set the profile DM token. It installs a
// MockCloudPolicyClient with |dm_token| into |profile|'s UserCloudPolicyManager
// to simulate |profile|'s DM token.
void SetProfileDMToken(Profile* profile, const std::string& dm_token);
#endif

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_DEEP_SCANNING_TEST_UTILS_H_
