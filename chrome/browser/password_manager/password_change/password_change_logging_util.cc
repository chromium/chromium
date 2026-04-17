// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_logging_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_change {

namespace {

std::unique_ptr<password_manager::BrowserSavePasswordProgressLogger>
GetLoggerIfAvailable(password_manager::PasswordManagerClient* client) {
  if (!client) {
    return nullptr;
  }

  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<
        password_manager::BrowserSavePasswordProgressLogger>(log_manager);
  }

  return nullptr;
}
std::string PageTypeToString(
    optimization_guide::proto::OpenFormResponseData::PageType type) {
  switch (type) {
    case optimization_guide::proto::OpenFormResponseData::UNSPECIFIED_PAGE:
      return "UNSPECIFIED_PAGE";
    case optimization_guide::proto::OpenFormResponseData::SETTINGS_PAGE:
      return "SETTINGS_PAGE";
    case optimization_guide::proto::OpenFormResponseData::
        CHANGE_PASSWORD_FORM_PAGE:
      return "CHANGE_PASSWORD_FORM_PAGE";
    case optimization_guide::proto::OpenFormResponseData::LOG_IN_PAGE:
      return "LOG_IN_PAGE";
    case optimization_guide::proto::OpenFormResponseData::
        USER_INTERVENTION_NEEDED_PAGE:
      return "USER_INTERVENTION_NEEDED_PAGE";
    default:
      return "UNKNOWN(" + base::NumberToString(type) + ")";
  }
}

std::string OutcomeToString(
    optimization_guide::proto::PasswordChangeSubmissionData::
        PasswordChangeOutcome outcome) {
  switch (outcome) {
    case optimization_guide::proto::PasswordChangeSubmissionData::
        UNKNOWN_OUTCOME:
      return "UNKNOWN_OUTCOME";
    case optimization_guide::proto::PasswordChangeSubmissionData::
        SUCCESSFUL_OUTCOME:
      return "SUCCESSFUL_OUTCOME";
    case optimization_guide::proto::PasswordChangeSubmissionData::
        UNSUCCESSFUL_OUTCOME:
      return "UNSUCCESSFUL_OUTCOME";
    case optimization_guide::proto::PasswordChangeSubmissionData::
        USER_INTERVENTION_NEEDED:
      return "USER_INTERVENTION_NEEDED";
    default:
      return "UNKNOWN(" + base::NumberToString(outcome) + ")";
  }
}

base::DictValue GetResponseDict(
    const optimization_guide::proto::PasswordChangeResponse& response) {
  base::DictValue dict;
  if (response.has_is_logged_in_data()) {
    const auto& data = response.is_logged_in_data();
    base::DictValue sub_dict;
    sub_dict.Set("is_logged_in", data.is_logged_in());
    sub_dict.Set("error_case", static_cast<int>(data.error_case()));
    sub_dict.Set("thought", data.thought());
    dict.Set("is_logged_in_data", base::Value(std::move(sub_dict)));
  }
  if (response.has_open_form_data()) {
    const auto& data = response.open_form_data();
    base::DictValue sub_dict;
    sub_dict.Set("dom_node_id_to_click",
                 static_cast<int>(data.dom_node_id_to_click()));
    sub_dict.Set("page_type", PageTypeToString(data.page_type()));
    sub_dict.Set("thought", data.thought());
    dict.Set("open_form_data", base::Value(std::move(sub_dict)));
  }
  if (response.has_submit_form_data()) {
    const auto& data = response.submit_form_data();
    base::DictValue sub_dict;
    sub_dict.Set("dom_node_id_to_click",
                 static_cast<int>(data.dom_node_id_to_click()));
    sub_dict.Set("is_user_intervention_needed",
                 data.is_user_intervention_needed());
    sub_dict.Set("thought", data.thought());
    dict.Set("submit_form_data", base::Value(std::move(sub_dict)));
  }
  if (response.has_outcome_data()) {
    const auto& data = response.outcome_data();
    base::DictValue sub_dict;
    sub_dict.Set("submission_outcome",
                 OutcomeToString(data.submission_outcome()));
    sub_dict.Set("submission_outcome_message",
                 data.submission_outcome_message());
    dict.Set("outcome_data", base::Value(std::move(sub_dict)));
  }
  return dict;
}

}  // namespace

void LogMessage(password_manager::PasswordManagerClient* client,
                autofill::SavePasswordProgressLogger::StringID message_id) {
  if (auto logger = GetLoggerIfAvailable(client)) {
    logger->LogMessage(message_id);
  }
}

void LogBoolean(password_manager::PasswordManagerClient* client,
                autofill::SavePasswordProgressLogger::StringID message_id,
                bool value) {
  if (auto logger = GetLoggerIfAvailable(client)) {
    logger->LogBoolean(message_id, value);
  }
}

void LogNumber(password_manager::PasswordManagerClient* client,
               autofill::SavePasswordProgressLogger::StringID message_id,
               int value) {
  if (auto logger = GetLoggerIfAvailable(client)) {
    logger->LogNumber(message_id, value);
  }
}

void LogString(password_manager::PasswordManagerClient* client,
               autofill::SavePasswordProgressLogger::StringID message_id,
               const std::string& value) {
  if (auto logger = GetLoggerIfAvailable(client)) {
    logger->LogString(message_id, value);
  }
}

void LogResponse(
    password_manager::PasswordManagerClient* client,
    autofill::SavePasswordProgressLogger::StringID message_id,
    const optimization_guide::proto::PasswordChangeResponse& response) {
  if (auto logger = GetLoggerIfAvailable(client)) {
    logger->LogValue(message_id, base::Value(GetResponseDict(response)));
  }
}

}  // namespace password_change
