// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/types/optional_util.h"
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
  if (event_result == "EVENT_RESULT_DETECTED") {
    return proto::EventResult::EVENT_RESULT_DETECTED;
  }
  if (event_result == "EVENT_RESULT_DATA_MASKED") {
    return proto::EventResult::EVENT_RESULT_DATA_MASKED;
  }
  if (event_result == "EVENT_RESULT_DATA_UNMASKED") {
    return proto::EventResult::EVENT_RESULT_DATA_UNMASKED;
  }
  if (event_result == "EVENT_RESULT_FORCED_SAVE_TO_CLOUD") {
    return proto::EventResult::EVENT_RESULT_FORCED_SAVE_TO_CLOUD;
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
              policy::CloudPolicyClient::ResultCallback callback) {
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

void EventReportValidator::ExpectUnscannedFileEvents(
    chrome::cros::reporting::proto::UnscannedFileEvent
        expected_unscanned_file_event,
    const std::vector<std::string>& expected_filenames,
    const std::vector<std::string>& expected_sha256s,
    const std::vector<std::string>& expected_scan_ids,
    const std::set<std::string>* expected_mimetypes) {
  DCHECK_EQ(expected_filenames.size(), expected_sha256s.size());
  base::flat_map<std::string, std::string> filenames_and_hashes;
  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes[expected_filenames[i]] = expected_sha256s[i];
    scan_ids_[expected_filenames[i]] = expected_scan_ids[i];
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      expected_filenames.size(),
      base::BindLambdaForTesting([this, expected_unscanned_file_event]() {
        if (expected_unscanned_file_event.unscanned_reason() ==
            proto::UnscannedFileEvent::TOO_MANY_REQUESTS) {
          // When throttled, ValidateReport will erase from scan_ids_ if the
          // file did not have a scan id.  Exactly one file should have been
          // scanned.
          EXPECT_EQ(scan_ids_.size(), 1ul);
        }
      }).Then(done_closure_ ? std::move(done_closure_) : base::DoNothing()));

  EXPECT_CALL(*client_, UploadSecurityEvent)
      .Times(expected_filenames.size())
      .WillRepeatedly(
          [this, expected_unscanned_file_event, filenames_and_hashes,
           expected_mimetypes, barrier_closure](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_unscanned_file_event());
            auto unscanned_file_event =
                request.events().Get(0).unscanned_file_event();

            EXPECT_TRUE(expected_mimetypes->contains(
                unscanned_file_event.content_type()));

            std::string filename = unscanned_file_event.file_name();
#if BUILDFLAG(IS_CHROMEOS)
            // TODO(crbug.com/40941444): To fix the tests for ChromeOS.
            // If filename is not found as expected, try the filename without
            // path.
            if (!filenames_and_hashes.contains(filename)) {
              for (const auto& fh : filenames_and_hashes) {
                if (base::FilePath(fh.first).BaseName().AsUTF8Unsafe() ==
                    filename) {
                  filename = fh.first;  // filename has full path now.
                  break;
                }
              }
            }
#endif  // BUILDFLAG(IS_CHROMEOS)

            EXPECT_EQ(filenames_and_hashes.at(filename),
                      unscanned_file_event.download_digest_sha_256());
            if (expected_unscanned_file_event.unscanned_reason() ==
                    proto::UnscannedFileEvent::TOO_MANY_REQUESTS &&
                unscanned_file_event.scan_id().empty()) {
              scan_ids_.erase(filename);
            } else {
              EXPECT_EQ(scan_ids_.at(filename), unscanned_file_event.scan_id());
            }

            // Clear the validated fields, so that the captured proto can match
            // the expected protos.
            unscanned_file_event.clear_content_type();
            unscanned_file_event.clear_file_name();
            unscanned_file_event.clear_download_digest_sha_256();
            unscanned_file_event.clear_scan_id();

            EXPECT_THAT(unscanned_file_event,
                        EqualsProto(expected_unscanned_file_event));

            barrier_closure.Run();
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
      expected_filenames.size(),
      done_closure_ ? std::move(done_closure_) : base::DoNothing());

  EXPECT_CALL(*client_, UploadSecurityEvent)
      .Times(expected_filenames.size())
      .WillRepeatedly(
          [expected_data_event, dlp_verdicts, results, filenames_and_hashes,
           scan_ids, barrier_closure](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
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

void EventReportValidator::ExpectSensitiveDataEventWarnThenBypass(
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_warn_event,
    chrome::cros::reporting::proto::DlpSensitiveDataEvent
        expected_bypass_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [expected_warn_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_sensitive_data_event());
            auto sensitive_data_event =
                request.events().Get(0).sensitive_data_event();
            EXPECT_THAT(sensitive_data_event, EqualsProto(expected_warn_event));
          })
      .WillOnce(
          [this, expected_bypass_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_sensitive_data_event());
            auto sensitive_data_event =
                request.events().Get(0).sensitive_data_event();
            EXPECT_THAT(sensitive_data_event,
                        EqualsProto(expected_bypass_event));

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
              policy::CloudPolicyClient::ResultCallback callback) mutable {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_dangerous_download_event());
            auto dangerous_download_event =
                request.events().Get(0).dangerous_download_event();

            if (expected_mimetypes) {
              EXPECT_TRUE(expected_mimetypes->contains(
                  dangerous_download_event.content_type()));
              // Reset the `content_type` field, so that we can check if the
              // rest of the fields match.
              dangerous_download_event.clear_content_type();
              expected_dangerous_download_event.clear_content_type();
            }

            EXPECT_THAT(dangerous_download_event,
                        EqualsProto(expected_dangerous_download_event));
          })
      .WillOnce(
          [this, expected_sensitive_data_event, expected_mimetypes](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) mutable {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_sensitive_data_event());
            auto sensitive_data_event =
                request.events().Get(0).sensitive_data_event();

            if (expected_mimetypes) {
              EXPECT_TRUE(expected_mimetypes->contains(
                  sensitive_data_event.content_type()));
              // Reset the `content_type` field, so that we can check if the
              // rest of the fields match.
              sensitive_data_event.clear_content_type();
              expected_sensitive_data_event.clear_content_type();
            }

            EXPECT_THAT(sensitive_data_event,
                        EqualsProto(expected_sensitive_data_event));

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
              policy::CloudPolicyClient::ResultCallback callback) mutable {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_dangerous_download_event());
            auto dangerous_download_event =
                request.events().Get(0).dangerous_download_event();

            if (expected_mimetypes) {
              EXPECT_TRUE(expected_mimetypes->contains(
                  dangerous_download_event.content_type()));
              // Reset the `content_type` field, so that we can check if the
              // rest of the fields match.
              dangerous_download_event.clear_content_type();
              expected_dangerous_download_event.clear_content_type();
            }

            EXPECT_THAT(dangerous_download_event,
                        EqualsProto(expected_dangerous_download_event));

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

std::unique_ptr<KeyedService> BuildRealtimeReportingClient(
    content::BrowserContext* context) {
  return std::make_unique<enterprise_connectors::RealtimeReportingClient>(
      context);
}

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
