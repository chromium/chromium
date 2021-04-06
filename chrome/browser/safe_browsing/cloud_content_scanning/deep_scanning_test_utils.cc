// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"

#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
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
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_trigger,
    const std::string& expected_reason,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size,
    const std::string& expected_result,
    const std::string& expected_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  unscanned_reason_ = expected_reason;
  content_size_ = expected_content_size;
  result_ = expected_result;
  username_ = expected_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ExpectUnscannedFileEvents(
    const std::string& expected_url,
    const std::vector<const std::string>& expected_filenames,
    const std::vector<const std::string>& expected_sha256s,
    const std::string& expected_trigger,
    const std::string& expected_reason,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size,
    const std::string& expected_result,
    const std::string& expected_username) {
  DCHECK_EQ(expected_filenames.size(), expected_sha256s.size());
  for (size_t i = 0; i < expected_filenames.size(); ++i)
    filenames_and_hashes_[expected_filenames[i]] = expected_sha256s[i];

  event_key_ = SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent;
  url_ = expected_url;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  unscanned_reason_ = expected_reason;
  content_size_ = expected_content_size;
  result_ = expected_result;
  username_ = expected_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .Times(expected_filenames.size())
      .WillRepeatedly([this](content::BrowserContext* context,
                             bool include_device_info, base::Value& report,
                             base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
      });
}

void EventReportValidator::ExpectDangerousDeepScanningResult(
    const std::string& expected_url,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_threat_type,
    const std::string& expected_trigger,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size,
    const std::string& expected_result,
    const std::string& expected_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  result_ = expected_result;
  username_ = expected_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ExpectSensitiveDataEvent(
    const std::string& expected_url,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_trigger,
    const enterprise_connectors::ContentAnalysisResponse::Result&
        expected_dlp_verdict,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size,
    const std::string& expected_result,
    const std::string& expected_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
  dlp_verdict_ = expected_dlp_verdict;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  result_ = expected_result;
  username_ = expected_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::
    ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
        const std::string& expected_url,
        const std::string& expected_filename,
        const std::string& expected_sha256,
        const std::string& expected_threat_type,
        const std::string& expected_trigger,
        const enterprise_connectors::ContentAnalysisResponse::Result&
            expected_dlp_verdict,
        const std::set<std::string>* expected_mimetypes,
        int expected_content_size,
        const std::string& expected_result,
        const std::string& expected_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  trigger_ = expected_trigger;
  mimetypes_ = expected_mimetypes;
  content_size_ = expected_content_size;
  result_ = expected_result;
  username_ = expected_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
      })
      .WillOnce([this, expected_dlp_verdict](
                    content::BrowserContext* context, bool include_device_info,
                    base::Value& report,
                    base::OnceCallback<void(bool)>& callback) {
        event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
        threat_type_ = base::nullopt;
        dlp_verdict_ = expected_dlp_verdict;
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::
    ExpectSensitiveDataEventAndDangerousDeepScanningResult(
        const std::string& expected_url,
        const std::string& expected_filename,
        const std::string& expected_sha256,
        const std::string& expected_threat_type,
        const std::string& expected_trigger,
        const enterprise_connectors::ContentAnalysisResponse::Result&
            expected_dlp_verdict,
        const std::set<std::string>* expected_mimetypes,
        int expected_content_size,
        const std::string& expected_result,
        const std::string& expected_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  trigger_ = expected_trigger;
  mimetypes_ = expected_mimetypes;
  content_size_ = expected_content_size;
  result_ = expected_result;
  dlp_verdict_ = expected_dlp_verdict;
  username_ = expected_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
      })
      .WillOnce([this, expected_threat_type](
                    content::BrowserContext* context, bool include_device_info,
                    base::Value& report,
                    base::OnceCallback<void(bool)>& callback) {
        event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
        threat_type_ = expected_threat_type;
        dlp_verdict_ = base::nullopt;
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ExpectDangerousDownloadEvent(
    const std::string& expected_url,
    const std::string& expected_filename,
    const std::string& expected_sha256,
    const std::string& expected_threat_type,
    const std::string& expected_trigger,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size,
    const std::string& expected_result,
    const std::string& expected_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  result_ = expected_result;
  username_ = expected_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ValidateReport(base::Value* report) {
  DCHECK(report);

  // Extract the event list.
  base::Value* event_list =
      report->FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  const base::Value::ListView mutable_list = event_list->GetList();

  // There should only be 1 event per test.
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(event_key_);
  ASSERT_NE(nullptr, event);
  ASSERT_EQ(base::Value::Type::DICTIONARY, event->type());

  // The event should match the expected values.
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyUrl, url_);
  ValidateFilenameAndHash(event);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyTrigger, trigger_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyContentSize,
                content_size_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyEventResult,
                result_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyThreatType,
                threat_type_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyUnscannedReason,
                unscanned_reason_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyProfileUserName,
                username_);
  ValidateMimeType(event);
  ValidateDlpVerdict(event);
}

void EventReportValidator::ValidateMimeType(base::Value* value) {
  std::string* type =
      value->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyContentType);
  if (mimetypes_)
    EXPECT_TRUE(base::Contains(*mimetypes_, *type));
  else
    EXPECT_EQ(nullptr, type);
}

void EventReportValidator::ValidateDlpVerdict(base::Value* value) {
  if (!dlp_verdict_.has_value())
    return;

  base::Value* triggered_rules =
      value->FindListKey(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rules);
  ASSERT_EQ(base::Value::Type::LIST, triggered_rules->type());
  base::Value::ListView rules_list = triggered_rules->GetList();
  int rules_size = rules_list.size();
  ASSERT_EQ(rules_size, dlp_verdict_.value().triggered_rules_size());
  for (int i = 0; i < rules_size; ++i) {
    base::Value* rule = &rules_list[i];
    ASSERT_EQ(base::Value::Type::DICTIONARY, rule->type());
    ValidateDlpRule(rule, dlp_verdict_.value().triggered_rules(i));
  }
}

void EventReportValidator::ValidateDlpRule(
    base::Value* value,
    const enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule&
        expected_rule) {
  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
                expected_rule.rule_name());
  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
                expected_rule.rule_id());
}

void EventReportValidator::ValidateFilenameAndHash(base::Value* value) {
  const std::string* filename =
      value->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName);
  ASSERT_TRUE(filename);
  ASSERT_TRUE(filenames_and_hashes_.count(*filename))
      << "Mismatch in field " << SafeBrowsingPrivateEventRouter::kKeyFileName;
  ValidateField(value, SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256,
                filenames_and_hashes_[*filename]);
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const base::Optional<std::string>& expected_value) {
  if (expected_value.has_value()) {
    ASSERT_EQ(*value->FindStringKey(field_key), expected_value.value())
        << "Mismatch in field " << field_key;
  } else {
    ASSERT_EQ(nullptr, value->FindStringKey(field_key))
        << "Field " << field_key << "should not be populated";
  }
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const base::Optional<int>& expected_value) {
  ASSERT_EQ(value->FindIntKey(field_key), expected_value)
      << "Mismatch in field " << field_key;
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const base::Optional<bool>& expected_value) {
  ASSERT_EQ(value->FindBoolKey(field_key), expected_value)
      << "Mismatch in field " << field_key;
}

void EventReportValidator::ExpectNoReport() {
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);
}

void EventReportValidator::SetDoneClosure(base::RepeatingClosure closure) {
  done_closure_ = std::move(closure);
}

void SetAnalysisConnector(PrefService* prefs,
                          enterprise_connectors::AnalysisConnector connector,
                          const std::string& pref_value,
                          bool machine_scope) {
  ListPrefUpdate settings_list(prefs, ConnectorPref(connector));
  DCHECK(settings_list.Get());
  if (!settings_list->empty())
    settings_list->Clear();

  settings_list->Append(*base::JSONReader::Read(pref_value));
  prefs->SetInteger(
      ConnectorScopePref(connector),
      machine_scope ? policy::POLICY_SCOPE_MACHINE : policy::POLICY_SCOPE_USER);
}

void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const std::set<std::string>& enabled_event_names,
    bool machine_scope) {
  ListPrefUpdate settings_list(prefs,
                               enterprise_connectors::kOnSecurityEventPref);
  DCHECK(settings_list.Get());
  if (enabled) {
    if (settings_list->empty()) {
      base::Value settings(base::Value::Type::DICTIONARY);

      settings.SetKey(enterprise_connectors::kKeyServiceProvider,
                      base::Value("google"));
      if (!enabled_event_names.empty()) {
        base::Value enabled_event_name_list(base::Value::Type::LIST);
        for (const auto& enabled_event_name : enabled_event_names) {
          enabled_event_name_list.Append(enabled_event_name);
        }
        settings.SetKey(enterprise_connectors::kKeyEnabledEventNames,
                        std::move(enabled_event_name_list));
      }
      settings_list->Append(std::move(settings));
    }
    prefs->SetInteger(enterprise_connectors::kOnSecurityEventScopePref,
                      machine_scope ? policy::POLICY_SCOPE_MACHINE
                                    : policy::POLICY_SCOPE_USER);
  } else {
    settings_list->ClearList();
    prefs->ClearPref(enterprise_connectors::kOnSecurityEventScopePref);
  }
}

void ClearAnalysisConnector(
    PrefService* prefs,
    enterprise_connectors::AnalysisConnector connector) {
  ListPrefUpdate settings_list(prefs, ConnectorPref(connector));
  DCHECK(settings_list.Get());
  settings_list->Clear();
  prefs->ClearPref(ConnectorScopePref(connector));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SetProfileDMToken(Profile* profile, const std::string& dm_token) {
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  client->SetDMToken(dm_token);
  profile->GetUserCloudPolicyManager()->Connect(
      g_browser_process->local_state(), std::move(client));
}
#endif

}  // namespace safe_browsing
