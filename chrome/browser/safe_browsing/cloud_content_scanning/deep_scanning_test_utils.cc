// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::SafeBrowsingPrivateEventRouter;
using ::testing::_;

namespace safe_browsing {

EventReportValidator::EventReportValidator(
    policy::MockCloudPolicyClient* client)
    : client_(client) {}

EventReportValidator::~EventReportValidator() {
  testing::Mock::VerifyAndClearExpectations(client_);
}

void EventReportValidator::ExpectUnscannedFileEvent(
    const std::string& expected_url,
    const std::string& expected_source,
    const std::string& expected_destination,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_trigger,
    const std::string& expected_reason,
    const std::set<std::string>* expected_mimetypes,
    int64_t expected_content_size,
    const std::string& expected_result,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent;
  url_ = expected_url;
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
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectUnscannedFileEvents(
    const std::string& expected_url,
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
    const std::string& expected_profile_identifier) {
  DCHECK_EQ(expected_filenames.size(), expected_sha256s.size());
  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes_[expected_filenames[i]] = expected_sha256s[i];
    results_[expected_filenames[i]] = expected_result;
  }

  event_key_ = SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent;
  url_ = expected_url;
  source_ = expected_source;
  destination_ = expected_destination;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  unscanned_reason_ = expected_reason;
  content_size_ = expected_content_size;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .Times(expected_filenames.size())
      .WillRepeatedly(
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) { ValidateReport(&report); });
}

void EventReportValidator::ExpectDangerousDeepScanningResult(
    const std::string& expected_url,
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
    const absl::optional<std::string>& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
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
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectSensitiveDataEvent(
    const std::string& expected_url,
    const std::string& expected_source,
    const std::string& expected_destination,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_trigger,
    const enterprise_connectors::ContentAnalysisResponse::Result&
        expected_dlp_verdict,
    const std::set<std::string>* expected_mimetypes,
    int64_t expected_content_size,
    const std::string& expected_result,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier,
    const std::string& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
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
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectSensitiveDataEvents(
    const std::string& expected_url,
    const std::string& expected_source,
    const std::string& expected_destination,
    const std::vector<std::string>& expected_filenames,
    const std::vector<std::string>& expected_sha256s,
    const std::string& expected_trigger,
    const std::vector<enterprise_connectors::ContentAnalysisResponse::Result>&
        expected_dlp_verdicts,
    const std::set<std::string>* expected_mimetypes,
    int64_t expected_content_size,
    const std::vector<std::string>& expected_results,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier,
    const std::vector<std::string>& expected_scan_ids) {
  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes_[expected_filenames[i]] = expected_sha256s[i];
    dlp_verdicts_[expected_filenames[i]] = expected_dlp_verdicts[i];
    results_[expected_filenames[i]] = expected_results[i];
    scan_ids_[expected_filenames[i]] = expected_scan_ids[i];
  }

  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
  source_ = expected_source;
  destination_ = expected_destination;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;

  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .Times(expected_filenames.size())
      .WillRepeatedly(
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) { ValidateReport(&report); });
}

void EventReportValidator::
    ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
        const std::string& expected_url,
        const std::string& expected_source,
        const std::string& expected_destination,
        const std::string& expected_filename,
        const std::string& expected_sha256,
        const std::string& expected_threat_type,
        const std::string& expected_trigger,
        const enterprise_connectors::ContentAnalysisResponse::Result&
            expected_dlp_verdict,
        const std::set<std::string>* expected_mimetypes,
        int64_t expected_content_size,
        const std::string& expected_result,
        const std::string& expected_profile_username,
        const std::string& expected_profile_identifier,
        const std::string& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
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
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) { ValidateReport(&report); })
      .WillOnce([this, expected_filename, expected_dlp_verdict](
                    content::BrowserContext* context, bool include_device_info,
                    base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
        threat_type_ = absl::nullopt;
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
        const std::string& expected_source,
        const std::string& expected_destination,
        const std::string& expected_filename,
        const std::string& expected_sha256,
        const std::string& expected_threat_type,
        const std::string& expected_trigger,
        const enterprise_connectors::ContentAnalysisResponse::Result&
            expected_dlp_verdict,
        const std::set<std::string>* expected_mimetypes,
        int64_t expected_content_size,
        const std::string& expected_result,
        const std::string& expected_profile_username,
        const std::string& expected_profile_identifier,
        const std::string& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
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
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) { ValidateReport(&report); })
      .WillOnce([this, expected_filename, expected_threat_type](
                    content::BrowserContext* context, bool include_device_info,
                    base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
        threat_type_ = expected_threat_type;
        dlp_verdicts_.erase(expected_filename);
        ValidateReport(&report);
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidator::ExpectDangerousDownloadEvent(
    const std::string& expected_url,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_threat_type,
    const std::string& expected_trigger,
    const std::set<std::string>* expected_mimetypes,
    int64_t expected_content_size,
    const std::string& expected_result,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
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
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectLoginEvent(
    const std::string& expected_url,
    const bool expected_is_federated,
    const std::string& expected_federated_origin,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier,
    const std::u16string& expected_login_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyLoginEvent;
  url_ = expected_url;
  is_federated_ = expected_is_federated;
  federated_origin_ = expected_federated_origin;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  login_user_name_ = expected_login_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidator::ExpectPasswordBreachEvent(
    const std::string& expected_trigger,
    const std::vector<std::pair<std::string, std::u16string>>&
        expected_identities,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyPasswordBreachEvent;
  trigger_ = expected_trigger;
  password_breach_identities_ = expected_identities;
  username_ = expected_profile_username;
  profile_identifier_ = expected_profile_identifier;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(
          [this](content::BrowserContext* context, bool include_device_info,
                 base::Value::Dict report,
                 base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                     callback) {
            ValidateReport(&report);
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
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
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyUrl, url_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeySource, source_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyDestination,
                destination_);
  ValidateFilenameMappedAttributes(event);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyTrigger, trigger_);
  // `content_size_` needs a conversion since int64 are strings in base::Value.
  absl::optional<std::string> size =
      content_size_.has_value()
          ? absl::optional<std::string>(base::NumberToString(*content_size_))
          : absl::nullopt;
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyContentSize, size);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyThreatType,
                threat_type_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyUnscannedReason,
                unscanned_reason_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyProfileUserName,
                username_);
  ValidateField(
      event,
      enterprise_connectors::RealtimeReportingClient::kKeyProfileIdentifier,
      profile_identifier_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyIsFederated,
                is_federated_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyLoginUserName,
                login_user_name_);
  ValidateFederatedOrigin(event);
  ValidateIdentities(event);
  ValidateMimeType(event);
}

void EventReportValidator::ValidateFederatedOrigin(
    const base::Value::Dict* value) {
  absl::optional<bool> is_federated =
      value->FindBool(SafeBrowsingPrivateEventRouter::kKeyIsFederated);
  const std::string* federated_origin =
      value->FindString(SafeBrowsingPrivateEventRouter::kKeyFederatedOrigin);
  if (is_federated.has_value() && is_federated.value()) {
    EXPECT_NE(nullptr, federated_origin);
    EXPECT_EQ(federated_origin_, *federated_origin);
  } else {
    EXPECT_EQ(nullptr, federated_origin);
  }
}

void EventReportValidator::ValidateIdentities(const base::Value::Dict* value) {
  const base::Value::List* identities = value->FindList(
      SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentities);
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
        const std::string* url = actual_identity_dict.FindString(
            SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentitiesUrl);
        const std::string* actual_username = actual_identity_dict.FindString(
            SafeBrowsingPrivateEventRouter::
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
  const std::string* type =
      value->FindString(SafeBrowsingPrivateEventRouter::kKeyContentType);
  if (mimetypes_) {
    EXPECT_TRUE(base::Contains(*mimetypes_, *type))
        << *type << " is not an expected mimetype";
  } else {
    EXPECT_EQ(nullptr, type);
  }
}

void EventReportValidator::ValidateDlpVerdict(
    const base::Value::Dict* value,
    const enterprise_connectors::ContentAnalysisResponse::Result& result) {
  const base::Value::List* triggered_rules =
      value->FindList(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
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
    const enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule&
        expected_rule) {
  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
                expected_rule.rule_name());
  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
                expected_rule.rule_id());
}

void EventReportValidator::ValidateFilenameMappedAttributes(
    const base::Value::Dict* value) {
  if (filenames_and_hashes_.empty()) {
    ASSERT_FALSE(value->contains(SafeBrowsingPrivateEventRouter::kKeyFileName));
  } else {
    const std::string* filename =
        value->FindString(SafeBrowsingPrivateEventRouter::kKeyFileName);
    ASSERT_TRUE(filename);
    ASSERT_TRUE(base::Contains(filenames_and_hashes_, *filename))
        << "Mismatch in field " << SafeBrowsingPrivateEventRouter::kKeyFileName;
    ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyEventResult,
                  results_[*filename]);
    ValidateField(value,
                  SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256,
                  filenames_and_hashes_[*filename]);
    if (scan_ids_.count(*filename)) {
      ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyScanId,
                    scan_ids_[*filename]);
    } else {
      ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyScanId,
                    absl::optional<std::string>());
    }
    if (dlp_verdicts_.count(*filename)) {
      ValidateDlpVerdict(value, dlp_verdicts_[*filename]);
    }
  }
}

void EventReportValidator::ValidateField(
    const base::Value::Dict* value,
    const std::string& field_key,
    const absl::optional<std::string>& expected_value) {
  if (expected_value.has_value()) {
    ASSERT_EQ(*value->FindString(field_key), expected_value.value())
        << "Mismatch in field " << field_key;
  } else {
    ASSERT_EQ(nullptr, value->FindString(field_key))
        << "Field " << field_key << " should not be populated. It has value "
        << *value->FindString(field_key);
  }
}

void EventReportValidator::ValidateField(
    const base::Value::Dict* value,
    const std::string& field_key,
    const absl::optional<std::u16string>& expected_value) {
  const std::string* s = value->FindString(field_key);
  if (expected_value.has_value()) {
    const std::u16string actual_string_value = base::UTF8ToUTF16(*s);
    ASSERT_EQ(actual_string_value, expected_value.value())
        << "Mismatch in field " << field_key;
  } else {
    ASSERT_EQ(nullptr, s) << "Field " << field_key
                          << " should not be populated. It has value "
                          << *value->FindString(field_key);
  }
}

void EventReportValidator::ValidateField(
    const base::Value::Dict* value,
    const std::string& field_key,
    const absl::optional<int>& expected_value) {
  ASSERT_EQ(value->FindInt(field_key), expected_value)
      << "Mismatch in field " << field_key;
}

void EventReportValidator::ValidateField(
    const base::Value::Dict* value,
    const std::string& field_key,
    const absl::optional<bool>& expected_value) {
  ASSERT_EQ(value->FindBool(field_key), expected_value)
      << "Mismatch in field " << field_key;
}

void EventReportValidator::ExpectNoReport() {
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);
}

void EventReportValidator::SetDoneClosure(base::RepeatingClosure closure) {
  done_closure_ = std::move(closure);
}

void SetAnalysisConnector(PrefService* prefs,
                          enterprise_connectors::AnalysisConnector connector,
                          const std::string& pref_value,
                          bool machine_scope) {
  ScopedListPrefUpdate settings_list(prefs, ConnectorPref(connector));
  if (!settings_list->empty()) {
    settings_list->clear();
  }

  settings_list->Append(*base::JSONReader::Read(pref_value));
  prefs->SetInteger(
      ConnectorScopePref(connector),
      machine_scope ? policy::POLICY_SCOPE_MACHINE : policy::POLICY_SCOPE_USER);
}

base::Value::List CreateOptInEventsList(
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
  base::Value::List enabled_opt_in_events_list;
  for (const auto& enabled_opt_in_event : enabled_opt_in_events) {
    base::Value::Dict event_value;
    event_value.Set(enterprise_connectors::kKeyOptInEventName,
                    enabled_opt_in_event.first);

    base::Value::List url_patterns_list;
    for (const auto& url_pattern : enabled_opt_in_event.second) {
      url_patterns_list.Append(url_pattern);
    }
    event_value.Set(enterprise_connectors::kKeyOptInEventUrlPatterns,
                    std::move(url_patterns_list));

    enabled_opt_in_events_list.Append(std::move(event_value));
  }
  return enabled_opt_in_events_list;
}

void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const std::set<std::string>& enabled_event_names,
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events,
    bool machine_scope) {
  ScopedListPrefUpdate settings_list(
      prefs, enterprise_connectors::kOnSecurityEventPref);
  if (!enabled) {
    settings_list->clear();
    prefs->ClearPref(enterprise_connectors::kOnSecurityEventScopePref);
    return;
  }

  if (settings_list->empty()) {
    base::Value::Dict settings;

    settings.Set(enterprise_connectors::kKeyServiceProvider,
                 base::Value("google"));
    if (!enabled_event_names.empty()) {
      base::Value::List enabled_event_name_list;
      for (const auto& enabled_event_name : enabled_event_names) {
        enabled_event_name_list.Append(enabled_event_name);
      }
      settings.Set(enterprise_connectors::kKeyEnabledEventNames,
                   std::move(enabled_event_name_list));
    }

    if (!enabled_opt_in_events.empty()) {
      settings.Set(enterprise_connectors::kKeyEnabledOptInEvents,
                   CreateOptInEventsList(enabled_opt_in_events));
    }

    settings_list->Append(std::move(settings));
  }
  prefs->SetInteger(
      enterprise_connectors::kOnSecurityEventScopePref,
      machine_scope ? policy::POLICY_SCOPE_MACHINE : policy::POLICY_SCOPE_USER);
}

void ClearAnalysisConnector(
    PrefService* prefs,
    enterprise_connectors::AnalysisConnector connector) {
  ScopedListPrefUpdate settings_list(prefs, ConnectorPref(connector));
  settings_list->clear();
  prefs->ClearPref(ConnectorScopePref(connector));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SetProfileDMToken(Profile* profile, const std::string& dm_token) {
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  client->SetDMToken(dm_token);

// crbug.com/1230268 The main profile in Lacros doesn't have a
// CloudPolicyManager, but we might want to apply the code if it's a secondary
// profile.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  profile->GetUserCloudPolicyManager()->Connect(
      g_browser_process->local_state(), std::move(client));
#endif
}
#endif

}  // namespace safe_browsing
