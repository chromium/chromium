// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"

#include "base/containers/contains.h"
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
  results_[expected_filename] = expected_result;
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
  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes_[expected_filenames[i]] = expected_sha256s[i];
    results_[expected_filenames[i]] = expected_result;
  }

  event_key_ = SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent;
  url_ = expected_url;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  unscanned_reason_ = expected_reason;
  content_size_ = expected_content_size;
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
    const std::string& expected_username,
    const absl::optional<std::string>& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_username;
  if (expected_scan_id.has_value())
    scan_ids_[expected_filename] = expected_scan_id.value();
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
    const std::string& expected_username,
    const std::string& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
  dlp_verdicts_[expected_filename] = expected_dlp_verdict;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_username;
  scan_ids_[expected_filename] = expected_scan_id;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ExpectSensitiveDataEvents(
    const std::string& expected_url,
    const std::vector<const std::string>& expected_filenames,
    const std::vector<const std::string>& expected_sha256s,
    const std::string& expected_trigger,
    const std::vector<enterprise_connectors::ContentAnalysisResponse::Result>&
        expected_dlp_verdicts,
    const std::set<std::string>* expected_mimetypes,
    int expected_content_size,
    const std::vector<std::string>& expected_results,
    const std::string& expected_username,
    const std::vector<std::string>& expected_scan_ids) {
  for (size_t i = 0; i < expected_filenames.size(); ++i) {
    filenames_and_hashes_[expected_filenames[i]] = expected_sha256s[i];
    dlp_verdicts_[expected_filenames[i]] = expected_dlp_verdicts[i];
    results_[expected_filenames[i]] = expected_results[i];
    scan_ids_[expected_filenames[i]] = expected_scan_ids[i];
  }

  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  username_ = expected_username;

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .Times(expected_filenames.size())
      .WillRepeatedly([this](content::BrowserContext* context,
                             bool include_device_info, base::Value& report,
                             base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
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
        const std::string& expected_username,
        const std::string& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  trigger_ = expected_trigger;
  mimetypes_ = expected_mimetypes;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_username;
  scan_ids_[expected_filename] = expected_scan_id;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
      })
      .WillOnce([this, expected_filename, expected_dlp_verdict](
                    content::BrowserContext* context, bool include_device_info,
                    base::Value& report,
                    base::OnceCallback<void(bool)>& callback) {
        event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
        threat_type_ = absl::nullopt;
        dlp_verdicts_[expected_filename] = expected_dlp_verdict;
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
        const std::string& expected_username,
        const std::string& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  trigger_ = expected_trigger;
  mimetypes_ = expected_mimetypes;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  dlp_verdicts_[expected_filename] = expected_dlp_verdict;
  username_ = expected_username;
  scan_ids_[expected_filename] = expected_scan_id;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
      })
      .WillOnce([this, expected_filename, expected_threat_type](
                    content::BrowserContext* context, bool include_device_info,
                    base::Value& report,
                    base::OnceCallback<void(bool)>& callback) {
        event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
        threat_type_ = expected_threat_type;
        dlp_verdicts_.erase(expected_filename);
        scan_ids_.erase(expected_filename);
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
    const std::string& expected_username,
    const absl::optional<std::string>& expected_scan_id) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent;
  url_ = expected_url;
  filenames_and_hashes_[expected_filename] = expected_sha256;
  threat_type_ = expected_threat_type;
  mimetypes_ = expected_mimetypes;
  trigger_ = expected_trigger;
  content_size_ = expected_content_size;
  results_[expected_filename] = expected_result;
  username_ = expected_username;
  if (expected_scan_id.has_value())
    scan_ids_[expected_filename] = expected_scan_id.value();
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ExpectLoginEvent(
    const std::string& expected_url,
    const bool expected_is_federated,
    const std::string& expected_federated_origin,
    const std::string& expected_profile_username,
    const std::u16string& expected_login_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyLoginEvent;
  url_ = expected_url;
  is_federated_ = expected_is_federated;
  federated_origin_ = expected_federated_origin;
  username_ = expected_profile_username;
  login_user_name_ = expected_login_username;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce([this](content::BrowserContext* context,
                       bool include_device_info, base::Value& report,
                       base::OnceCallback<void(bool)>& callback) {
        ValidateReport(&report);
        if (!done_closure_.is_null())
          done_closure_.Run();
      });
}

void EventReportValidator::ExpectPasswordBreachEvent(
    const std::string& expected_trigger,
    const std::vector<std::pair<std::string, std::u16string>>&
        expected_identities,
    const std::string& expected_username) {
  event_key_ = SafeBrowsingPrivateEventRouter::kKeyPasswordBreachEvent;
  trigger_ = expected_trigger;
  password_breach_identities_ = expected_identities;
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
  ValidateFilenameMappedAttributes(event);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyTrigger, trigger_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyContentSize,
                content_size_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyThreatType,
                threat_type_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyUnscannedReason,
                unscanned_reason_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyProfileUserName,
                username_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyIsFederated,
                is_federated_);
  ValidateField(event, SafeBrowsingPrivateEventRouter::kKeyLoginUserName,
                login_user_name_);
  ValidateFederatedOrigin(event);
  ValidateIdentities(event);
  ValidateMimeType(event);
}

void EventReportValidator::ValidateFederatedOrigin(base::Value* value) {
  absl::optional<bool> is_federated =
      value->FindBoolKey(SafeBrowsingPrivateEventRouter::kKeyIsFederated);
  std::string* federated_origin =
      value->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFederatedOrigin);
  if (is_federated.has_value() && is_federated.value()) {
    EXPECT_NE(nullptr, federated_origin);
    EXPECT_EQ(federated_origin_, *federated_origin);
  } else {
    EXPECT_EQ(nullptr, federated_origin);
  }
}

void EventReportValidator::ValidateIdentities(base::Value* value) {
  base::Value* v = value->FindListKey(
      SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentities);
  if (!password_breach_identities_) {
    EXPECT_EQ(nullptr, v);
  } else {
    EXPECT_NE(nullptr, v);

    EXPECT_TRUE(v->is_list());
    const auto& identities = v->GetList();
    EXPECT_EQ(password_breach_identities_->size(), identities.size());
    for (const auto& expected_identity : *password_breach_identities_) {
      bool matched = false;
      for (const auto& actual_identity : identities) {
        const std::string* url = actual_identity.FindStringKey(
            SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentitiesUrl);
        std::u16string username;
        EXPECT_TRUE(actual_identity
                        .FindPath(SafeBrowsingPrivateEventRouter::
                                      kKeyPasswordBreachIdentitiesUsername)
                        ->GetAsString(&username));
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

void EventReportValidator::ValidateMimeType(base::Value* value) {
  std::string* type =
      value->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyContentType);
  if (mimetypes_)
    EXPECT_TRUE(base::Contains(*mimetypes_, *type))
        << *type << " is not an expected mimetype";
  else
    EXPECT_EQ(nullptr, type);
}

void EventReportValidator::ValidateDlpVerdict(
    base::Value* value,
    const enterprise_connectors::ContentAnalysisResponse::Result& result) {
  base::Value* triggered_rules =
      value->FindListKey(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rules);
  ASSERT_EQ(base::Value::Type::LIST, triggered_rules->type());
  base::Value::ListView rules_list = triggered_rules->GetList();
  int rules_size = rules_list.size();
  ASSERT_EQ(rules_size, result.triggered_rules_size());
  for (int i = 0; i < rules_size; ++i) {
    base::Value* rule = &rules_list[i];
    ASSERT_EQ(base::Value::Type::DICTIONARY, rule->type());
    ValidateDlpRule(rule, result.triggered_rules(i));
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

void EventReportValidator::ValidateFilenameMappedAttributes(
    base::Value* value) {
  if (filenames_and_hashes_.empty()) {
    ASSERT_FALSE(value->FindKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  } else {
    const std::string* filename =
        value->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName);
    ASSERT_TRUE(filename);
    ASSERT_TRUE(filenames_and_hashes_.count(*filename))
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
    base::Value* value,
    const std::string& field_key,
    const absl::optional<std::string>& expected_value) {
  if (expected_value.has_value()) {
    ASSERT_EQ(*value->FindStringKey(field_key), expected_value.value())
        << "Mismatch in field " << field_key;
  } else {
    ASSERT_EQ(nullptr, value->FindStringKey(field_key))
        << "Field " << field_key << " should not be populated";
  }
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const absl::optional<std::u16string>& expected_value) {
  base::Value* v = value->FindPath(field_key);
  if (expected_value.has_value()) {
    std::u16string actual_string_value;
    ASSERT_TRUE(v->GetAsString(&actual_string_value));
    ASSERT_EQ(actual_string_value, expected_value.value())
        << "Mismatch in field " << field_key;
  } else {
    ASSERT_EQ(nullptr, v) << "Field " << field_key
                          << " should not be populated";
  }
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const absl::optional<int>& expected_value) {
  ASSERT_EQ(value->FindIntKey(field_key), expected_value)
      << "Mismatch in field " << field_key;
}

void EventReportValidator::ValidateField(
    base::Value* value,
    const std::string& field_key,
    const absl::optional<bool>& expected_value) {
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
  if (!settings_list->GetList().empty())
    settings_list->ClearList();

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
    if (settings_list->GetList().empty()) {
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
  settings_list->ClearList();
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
