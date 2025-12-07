// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/protobuf_matchers.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace enterprise_connectors::test {

namespace {

// Namespace alias to reduce verbosity when using event protos.
namespace proto = ::chrome::cros::reporting::proto;

constexpr char kKeyProfileUserName[] = "profileUserName";

proto::EventResult GetEventResultProto(const std::string& event_result) {
  if (event_result == "EVENT_RESULT_UNKNOWN") {
    return proto::EventResult::EVENT_RESULT_UNSPECIFIED;
  }
  if (event_result == "EVENT_RESULT_ALLOWED") {
    return proto::EventResult::EVENT_RESULT_ALLOWED;
  }
  if (event_result == "EVENT_RESULT_WARNED") {
    return proto::EventResult::EVENT_RESULT_WARNED;
  }
  if (event_result == "EVENT_RESULT_BLOCKED") {
    return proto::EventResult::EVENT_RESULT_BLOCKED;
  }
  if (event_result == "EVENT_RESULT_BYPASSED") {
    return proto::EventResult::EVENT_RESULT_BYPASSED;
  }
  NOTREACHED();
}

}  // namespace

using base::test::EqualsProto;

EventReportValidator::EventReportValidator(
    policy::MockCloudPolicyClient* client)
    : EventReportValidatorBase(client) {}

EventReportValidator::~EventReportValidator() = default;

void EventReportValidator::ExpectUnscannedFileEvent(
    chrome::cros::reporting::proto::UnscannedFileEvent
        expected_unscanned_file_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_unscanned_file_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                  callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_unscanned_file_event());
            auto unscanned_file_event =
                request.events().Get(0).unscanned_file_event();
            EXPECT_THAT(unscanned_file_event,
                        EqualsProto(expected_unscanned_file_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectUnscannedFileEvent(
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
    const std::optional<std::string>& expected_content_transfer_method) {
  event_key_ = kKeyUnscannedFileEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  source_ = expected_source;
  destination_ = expected_destination;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  unscanned_reason_ = expected_reason;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  content_transfer_method_ = expected_content_transfer_method;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectUnscannedFileEvents(
    chrome::cros::reporting::proto::UnscannedFileEvent
        expected_unscanned_file_event,
    const std::vector<std::string>& expected_filenames,
    const std::vector<std::string>& expected_sha256s,
    const std::set<std::string>* expected_mimetypes) {
  DCHECK_EQ(expected_filenames.size(), expected_sha256s.size());
  base::flat_map<std::string, std::string> filenames_and_hashes;
  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes[expected_filenames[i]] = expected_sha256s[i];
  }

  EXPECT_CALL(*client_, UploadSecurityEvent)
      .Times(expected_filenames.size())
      .WillRepeatedly(
          [this, expected_unscanned_file_event, filenames_and_hashes,
           expected_mimetypes](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                  callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_unscanned_file_event());
            auto unscanned_file_event =
                request.events().Get(0).unscanned_file_event();

            EXPECT_TRUE(base::Contains(*expected_mimetypes,
                                       unscanned_file_event.content_type()));
            EXPECT_EQ(filenames_and_hashes.at(unscanned_file_event.file_name()),
                      unscanned_file_event.download_digest_sha_256());

            // Clear the validated fields, so that the captured proto can match
            // the expected protos.
            unscanned_file_event.clear_content_type();
            unscanned_file_event.clear_file_name();
            unscanned_file_event.clear_download_digest_sha_256();

            EXPECT_THAT(unscanned_file_event,
                        EqualsProto(expected_unscanned_file_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectUnscannedFileEvents(
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
    const std::optional<std::string>& expected_content_transfer_method) {
  DCHECK_EQ(expected_filenames.size(), expected_sha256s.size());
  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes_[expected_filenames[i]] = expected_sha256s[i];
    results_[expected_filenames[i]] = expected_result;
  }

  event_key_ = kKeyUnscannedFileEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  source_ = expected_source;
  destination_ = expected_destination;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  unscanned_reason_ = expected_reason;
  content_size_ = expected_content_size;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  content_transfer_method_ = expected_content_transfer_method;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .Times(expected_filenames.size())
      .WillRepeatedly(
          [this](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) { ValidateReport(&report); });
}

void EventReportValidator::ExpectDangerousDeepScanningResult(
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
    const std::optional<std::string>& expected_scan_id) {
  event_key_ = kKeyDangerousDownloadEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  source_ = expected_source;
  destination_ = expected_destination;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  if (expected_scan_id.has_value()) {
    scan_ids_[expected_filename] = expected_scan_id.value();
  }
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectSensitiveDataEvents(
    const std::vector<chrome::cros::reporting::proto::DlpSensitiveDataEvent>
        expected_sensitive_data_events,
    const std::vector<std::string>& expected_filenames,
    const std::vector<std::string>& expected_sha256s,
    const std::vector<std::string>& expected_results,
    const std::vector<std::string>& expected_scan_ids) {
  base::flat_map<std::string, ContentAnalysisResponse::Result> dlp_verdicts;
  base::flat_map<std::string, std::string> results;
  base::flat_map<std::string, std::string> filenames_and_hashes;
  base::flat_map<std::string, std::string> scan_ids;
  base::flat_map<std::string,
                 chrome::cros::reporting::proto::DlpSensitiveDataEvent>
      expected_data_event;

  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes[expected_filenames[i]] = expected_sha256s[i];
    results[expected_filenames[i]] = expected_results[i];
    scan_ids[expected_filenames[i]] = expected_scan_ids[i];
    expected_data_event[expected_filenames[i]] =
        expected_sensitive_data_events[i];
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      expected_filenames.size(), base::BindOnce(
                                     [](base::RepeatingClosure closure) {
                                       if (!closure.is_null()) {
                                         closure.Run();
                                       }
                                     },
                                     std::move(done_closure_)));

  EXPECT_CALL(*client_, UploadSecurityEvent)
      .Times(expected_filenames.size())
      .WillRepeatedly(
          [expected_data_event, dlp_verdicts, results, filenames_and_hashes,
           scan_ids, barrier_closure](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                  callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_sensitive_data_event());
            auto sensitive_data_event =
                request.events().Get(0).sensitive_data_event();

            const auto filename = sensitive_data_event.file_name();
            EXPECT_EQ(filenames_and_hashes.at(filename),
                      sensitive_data_event.download_digest_sha_256());
            EXPECT_EQ(scan_ids.at(filename), sensitive_data_event.scan_id());
            EXPECT_EQ(GetEventResultProto(results.at(filename)),
                      sensitive_data_event.event_result());

            // Clear the validated fields, so that the captured proto can match
            // the expected protos
            sensitive_data_event.clear_file_name();
            sensitive_data_event.clear_scan_id();
            sensitive_data_event.clear_event_result();
            sensitive_data_event.clear_download_digest_sha_256();

            EXPECT_THAT(sensitive_data_event,
                        EqualsProto(expected_data_event.at(filename)));

            barrier_closure.Run();
          });
}

void EventReportValidator::ExpectSensitiveDataEvent(
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
    const std::optional<std::u16string>& expected_user_justification) {
  event_key_ = kKeySensitiveDataEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  source_ = expected_source;
  destination_ = expected_destination;
  dlp_verdicts_[expected_filename] = expected_dlp_verdict;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  scan_ids_[expected_filename] = expected_scan_id;
  content_transfer_method_ = expected_content_transfer_method;
  user_justification_ = expected_user_justification;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void EventReportValidator::ExpectDataMaskingEvent(
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier,
    extensions::api::enterprise_reporting_private::DataMaskingEvent
        expected_event) {
  event_key_ = kKeySensitiveDataEvent;
  url_ = expected_event.url;
  tab_url_ = expected_event.url;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  expected_data_masking_rules_builder_ = base::BindRepeating(
      [](const extensions::api::enterprise_reporting_private::DataMaskingEvent&
             event) { return event.Clone(); },
      std::move(expected_event));
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void EventReportValidator::ExpectSensitiveDataEvents(
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
    const std::optional<std::u16string>& expected_user_justification) {
  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes_[expected_filenames[i]] = expected_sha256s[i];
    dlp_verdicts_[expected_filenames[i]] = expected_dlp_verdicts[i];
    results_[expected_filenames[i]] = expected_results[i];
    scan_ids_[expected_filenames[i]] = expected_scan_ids[i];
  }

  event_key_ = kKeySensitiveDataEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  source_ = expected_source;
  destination_ = expected_destination;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  content_transfer_method_ = expected_content_transfer_method;
  user_justification_ = expected_user_justification;

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      expected_filenames.size(), base::BindOnce(
                                     [](base::RepeatingClosure closure) {
                                       if (!closure.is_null()) {
                                         closure.Run();
                                       }
                                     },
                                     std::move(done_closure_)));

  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .Times(expected_filenames.size())
      .WillRepeatedly(
          [this, barrier_closure](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            barrier_closure.Run();
          });
}

void EventReportValidator::ExpectSensitiveDataEventWarnThenBypass(
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
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier,
    const std::string& expected_scan_id,
    const std::optional<std::string>& expected_content_transfer_method,
    const std::vector<std::optional<std::u16string>>&
        expected_user_justifications) {
  event_key_ = kKeySensitiveDataEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  source_ = expected_source;
  destination_ = expected_destination;
  dlp_verdicts_[expected_filename] = expected_dlp_verdict;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  results_[expected_filename] = EventResultToString(EventResult::WARNED);
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  scan_ids_[expected_filename] = expected_scan_id;
  content_transfer_method_ = expected_content_transfer_method;
  user_justification_ = expected_user_justifications[0];
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_filename](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) { ValidateReport(&report); })
      .WillOnce([this, expected_filename, expected_user_justifications](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        results_[expected_filename] =
            EventResultToString(EventResult::BYPASSED);
        user_justification_ = expected_user_justifications[1];
        ValidateReport(&report);
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidator::
    ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
        chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
            expected_dangerous_download_event,
        chrome::cros::reporting::proto::DlpSensitiveDataEvent
            expected_sensitive_data_event,
        const std::set<std::string>* expected_mimetypes) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [expected_dangerous_download_event, expected_mimetypes](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                  callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_dangerous_download_event());
            auto dangerous_download_event =
                request.events().Get(0).dangerous_download_event();

            if (expected_mimetypes) {
              EXPECT_TRUE(
                  base::Contains(*expected_mimetypes,
                                 dangerous_download_event.content_type()));
              // Reset the `content_type` field, so that we can check if the
              // rest of the fields match.
              dangerous_download_event.clear_content_type();
            }

            EXPECT_THAT(dangerous_download_event,
                        EqualsProto(expected_dangerous_download_event));
          })
      .WillOnce(
          [this, expected_sensitive_data_event, expected_mimetypes](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                  callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_sensitive_data_event());
            auto sensitive_data_event =
                request.events().Get(0).sensitive_data_event();

            if (expected_mimetypes) {
              EXPECT_TRUE(base::Contains(*expected_mimetypes,
                                         sensitive_data_event.content_type()));
              // Reset the `content_type` field, so that we can check if the
              // rest of the fields match.
              sensitive_data_event.clear_content_type();
            }

            EXPECT_THAT(sensitive_data_event,
                        EqualsProto(expected_sensitive_data_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::
    ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
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
        const std::optional<std::string>& expected_content_transfer_method) {
  event_key_ = kKeyDangerousDownloadEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  source_ = expected_source;
  destination_ = expected_destination;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  trigger_ = expected_trigger;
  mimetypes_ = expected_mimetypes;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  scan_ids_[expected_filename] = expected_scan_id;
  content_transfer_method_ = expected_content_transfer_method;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) { ValidateReport(&report); })
      .WillOnce([this, expected_filename, expected_dlp_verdict](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        event_key_ = kKeySensitiveDataEvent;
        threat_type_ = std::nullopt;
        dlp_verdicts_[expected_filename] = expected_dlp_verdict;
        ValidateReport(&report);
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidator::
    ExpectSensitiveDataEventAndDangerousDeepScanningResult(
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
        const std::string& expected_scan_id) {
  event_key_ = kKeySensitiveDataEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  source_ = expected_source;
  destination_ = expected_destination;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  trigger_ = expected_trigger;
  mimetypes_ = expected_mimetypes;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  dlp_verdicts_[expected_filename] = expected_dlp_verdict;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  scan_ids_[expected_filename] = expected_scan_id;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) { ValidateReport(&report); })
      .WillOnce([this, expected_filename, expected_threat_type](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        event_key_ = kKeyDangerousDownloadEvent;
        threat_type_ = expected_threat_type;
        dlp_verdicts_.erase(expected_filename);
        ValidateReport(&report);
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidator::ExpectDangerousDownloadEvent(
    chrome::cros::reporting::proto::SafeBrowsingDangerousDownloadEvent
        expected_dangerous_download_event,
    const std::set<std::string>* expected_mimetypes) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_dangerous_download_event, expected_mimetypes](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                  callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_dangerous_download_event());
            auto dangerous_download_event =
                request.events().Get(0).dangerous_download_event();

            if (expected_mimetypes) {
              EXPECT_TRUE(
                  base::Contains(*expected_mimetypes,
                                 dangerous_download_event.content_type()));
              // Reset the `content_type` field, so that we can check if the
              // rest of the fields match.
              dangerous_download_event.clear_content_type();
            }

            EXPECT_THAT(dangerous_download_event,
                        EqualsProto(expected_dangerous_download_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectDangerousDownloadEvent(
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
    const std::string& expected_profile_identifier) {
  event_key_ = kKeyDangerousDownloadEvent;
  url_ = expected_url;
  tab_url_ = expected_tab_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](bool include_device_info, base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectActiveUser(const std::string& user) {
  active_content_area_user_ = user;
}

void EventReportValidator::ExpectSourceActiveUser(const std::string& user) {
  source_active_content_area_user_ = user;
}

void EventReportValidator::ExpectFrameUrlChain(
    const std::vector<std::string>& frame_urls) {
  frame_urls_ = frame_urls;
}

void EventReportValidator::ValidateReport(const base::Value::Dict* report) {
  DCHECK(report);

  // Extract the event list.
  const base::Value::List* event_list = report->FindList(
      policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);

  // There should only be 1 event per test.
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event = wrapper.FindDict(event_key_);
  ASSERT_NE(nullptr, event);

  // The event should match the expected values.
  ValidateField(event, kKeyUrl, url_);
  ValidateField(event, kKeyTabUrl, tab_url_);
  ValidateField(event, kKeySource, source_);
  ValidateField(event, kKeyDestination, destination_);
  ValidateFilenameMappedAttributes(event);
  ValidateField(event, kKeyTrigger, trigger_);
  // `content_size_` needs a conversion since int64 are strings in base::Value.
  std::optional<std::string> size =
      content_size_.has_value()
          ? std::optional<std::string>(base::NumberToString(*content_size_))
          : std::nullopt;
  ValidateField(event, kKeyContentSize, size);
  ValidateField(event, kKeyThreatType, threat_type_);
  ValidateField(event, kKeyUnscannedReason, unscanned_reason_);
  ValidateField(event, kKeyContentTransferMethod, content_transfer_method_);
  ValidateField(event, kKeyUserJustification, user_justification_);
  ValidateField(event, kKeyProfileUserName, username_);
  ValidateField(event, RealtimeReportingClient::kKeyProfileIdentifier,
                profile_identifier_);
  ValidateField(event, kKeyIsFederated, is_federated_);
  ValidateField(event, kKeyLoginUserName, login_user_name_);
  ValidateField(event, kKeyWebAppSignedInAccount, active_content_area_user_);
  ValidateField(event, kKeySourceWebAppSignedInAccount,
                source_active_content_area_user_);
  ValidateFederatedOrigin(event);
  ValidateIdentities(event);
  ValidateMimeType(event);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ValidateDataMaskingAttributes(event);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  ValidateFrameUrlChain(event);
}

void EventReportValidator::ValidateFederatedOrigin(
    const base::Value::Dict* value) {
  std::optional<bool> is_federated = value->FindBool(kKeyIsFederated);
  const std::string* federated_origin = value->FindString(kKeyFederatedOrigin);
  if (is_federated.has_value() && is_federated.value()) {
    EXPECT_NE(nullptr, federated_origin);
    EXPECT_EQ(federated_origin_, *federated_origin);
  } else {
    EXPECT_EQ(nullptr, federated_origin);
  }
}

void EventReportValidator::ValidateIdentities(const base::Value::Dict* value) {
  const base::Value::List* identities =
      value->FindList(kKeyPasswordBreachIdentities);
  if (!password_breach_identities_) {
    EXPECT_EQ(nullptr, identities);
  } else {
    EXPECT_NE(nullptr, identities);

    EXPECT_EQ(password_breach_identities_->size(), identities->size());
    for (const auto& expected_identity : *password_breach_identities_) {
      bool matched = false;
      for (const auto& actual_identity : *identities) {
        const base::Value::Dict& actual_identity_dict =
            actual_identity.GetDict();
        const std::string* url =
            actual_identity_dict.FindString(kKeyPasswordBreachIdentitiesUrl);
        const std::string* actual_username = actual_identity_dict.FindString(
            kKeyPasswordBreachIdentitiesUsername);
        EXPECT_NE(nullptr, actual_username);
        const std::u16string username = base::UTF8ToUTF16(*actual_username);
        EXPECT_NE(nullptr, url);
        if (expected_identity.first == *url &&
            expected_identity.second == username) {
          matched = true;
          break;
        }
      }
      EXPECT_TRUE(matched);
    }
  }
}

void EventReportValidator::ValidateMimeType(const base::Value::Dict* value) {
  const std::string* type = value->FindString(kKeyContentType);
  if (mimetypes_) {
    EXPECT_TRUE(base::Contains(*mimetypes_, *type))
        << *type << " is not an expected mimetype";
  } else {
    EXPECT_EQ(nullptr, type);
  }
}

void EventReportValidator::ValidateDlpVerdict(
    const base::Value::Dict* value,
    const ContentAnalysisResponse::Result& result) {
  const base::Value::List* triggered_rules =
      value->FindList(kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rules);
  ASSERT_EQ(base::checked_cast<size_t>(result.triggered_rules_size()),
            triggered_rules->size());
  for (size_t i = 0; i < triggered_rules->size(); ++i) {
    const base::Value::Dict& rule = (*triggered_rules)[i].GetDict();
    ValidateDlpRule(&rule, result.triggered_rules(i));
  }
}

void EventReportValidator::ValidateDlpRule(
    const base::Value::Dict* value,
    const ContentAnalysisResponse::Result::TriggeredRule& expected_rule) {
  ValidateField(value, kKeyTriggeredRuleName, expected_rule.rule_name());
  if (expected_rule.rule_id().empty()) {
    ValidateField(value, kKeyTriggeredRuleId, std::optional<int>());
  } else {
    int expected_rule_id = 0;
    ASSERT_TRUE(base::StringToInt(expected_rule.rule_id(), &expected_rule_id));
    ValidateField(value, kKeyTriggeredRuleId,
                  std::optional<int>(expected_rule_id));
  }
}

void EventReportValidator::ValidateFilenameMappedAttributes(
    const base::Value::Dict* value) {
  if (filenames_and_hashes_.empty()) {
    ASSERT_FALSE(value->contains(kKeyFileName))
        << "Expected no file name but found "
        << *value->FindString(kKeyFileName);
  } else {
    ASSERT_TRUE(value->FindString(kKeyFileName));

    std::string filename = *(value->FindString(kKeyFileName));
    std::string filenames;
    for (const auto& fh : filenames_and_hashes_) {
      filenames += fh.first + "; ";
    }
#if BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/40941444): To fix the tests for ChromeOS.
    // If filename is not found as expected, try the filename without path.
    if (!base::Contains(filenames_and_hashes_, filename)) {
      for (const auto& fh : filenames_and_hashes_) {
        filenames += fh.first + "; ";
        if (base::FilePath(fh.first).BaseName().AsUTF8Unsafe() == filename) {
          filename = fh.first;  // filename has full path now.
        }
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS)

    ASSERT_TRUE(base::Contains(filenames_and_hashes_, filename))
        << "Mismatch in field " << kKeyFileName
        << "\nActual filename: " << filename << "\nExpected one filename in: { "
        << filenames << "}";
    ValidateField(value, kKeyEventResult, results_[filename]);
    ValidateField(value, kKeyDownloadDigestSha256,
                  filenames_and_hashes_[filename]);
    if (scan_ids_.count(filename)) {
      ValidateField(value, kKeyScanId, scan_ids_[filename]);
    } else {
      ValidateField(value, kKeyScanId, std::optional<std::string>());
    }
    if (dlp_verdicts_.count(filename)) {
      ValidateDlpVerdict(value, dlp_verdicts_[filename]);
    }
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void EventReportValidator::ValidateDataMaskingAttributes(
    const base::Value::Dict* event) {
  if (expected_data_masking_rules_builder_) {
    auto data_masking_rules = std::move(expected_data_masking_rules_builder_)
                                  .Run()
                                  .triggered_rule_info;
    const base::Value::List* triggered_rules =
        event->FindList(kKeyTriggeredRuleInfo);
    ASSERT_TRUE(triggered_rules);
    ASSERT_EQ(data_masking_rules.size(), triggered_rules->size());
    size_t rule_index = 0;
    for (const base::Value& rule : *triggered_rules) {
      ASSERT_EQ(rule.GetDict(), data_masking_rules[rule_index].ToValue());
      ++rule_index;
    }
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void EventReportValidator::ValidateFrameUrlChain(
    const base::Value::Dict* value) {
  const base::Value::List* frame_urls = value->FindList(kKeyIframeUrls);
  if (!frame_urls_.has_value()) {
    EXPECT_TRUE(!frame_urls || frame_urls->empty());
    return;
  }

  ASSERT_NE(nullptr, frame_urls);
  std::vector<std::string> actual_urls;
  for (const auto& url_value : *frame_urls) {
    actual_urls.push_back(url_value.GetString());
  }

  EXPECT_THAT(actual_urls, testing::ElementsAreArray(frame_urls_.value()));
}

EventReportValidatorHelper::EventReportValidatorHelper(Profile* profile,
                                                       bool browser_test)
    : profile_(profile),
      client_(std::make_unique<policy::MockCloudPolicyClient>()) {
  DCHECK(profile);

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  client_->SetDMToken("dm_token");

  if (!browser_test) {
    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile, base::BindRepeating([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              new RealtimeReportingClient(context));
        }));
  }

  RealtimeReportingClientFactory::GetForProfile(profile)
      ->SetBrowserCloudPolicyClientForTesting(client_.get());
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test-user@chromium.org", signin::ConsentLevel::kSignin);
  RealtimeReportingClientFactory::GetForProfile(profile)
      ->SetIdentityManagerForTesting(
          identity_test_environment_.identity_manager());
  SetOnSecurityEventReporting(profile->GetPrefs(), true);
}

EventReportValidatorHelper::~EventReportValidatorHelper() {
  RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetBrowserCloudPolicyClientForTesting(nullptr);
  policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
}

EventReportValidator EventReportValidatorHelper::CreateValidator() {
  return EventReportValidator(client_.get());
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void SetAnalysisConnector(PrefService* prefs,
                          AnalysisConnector connector,
                          const std::string& pref_value,
                          bool machine_scope) {
  ScopedListPrefUpdate settings_list(prefs, AnalysisConnectorPref(connector));
  if (!settings_list->empty()) {
    settings_list->clear();
  }

  settings_list->Append(*base::JSONReader::Read(
      pref_value, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  prefs->SetInteger(
      AnalysisConnectorScopePref(connector),
      machine_scope ? policy::POLICY_SCOPE_MACHINE : policy::POLICY_SCOPE_USER);
}

void ClearAnalysisConnector(PrefService* prefs, AnalysisConnector connector) {
  ScopedListPrefUpdate settings_list(prefs, AnalysisConnectorPref(connector));
  settings_list->clear();
  prefs->ClearPref(AnalysisConnectorScopePref(connector));
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if !BUILDFLAG(IS_CHROMEOS)
void SetProfileDMToken(Profile* profile, const std::string& dm_token) {
  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_request_token(dm_token);
  profile->GetCloudPolicyManager()
      ->core()
      ->store()
      ->set_policy_data_for_testing(std::move(policy_data));

  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  client->SetDMToken(dm_token);

  profile->GetUserCloudPolicyManager()->Connect(
      g_browser_process->local_state(), std::move(client));
}
#endif

}  // namespace enterprise_connectors::test
