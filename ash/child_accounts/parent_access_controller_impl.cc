// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/child_accounts/parent_access_controller_impl.h"

#include <string>

#include "ash/login/login_screen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Number of digits displayed in parent access code input.
constexpr int kParentAccessCodePinLength = 6;

// Base name of the histogram to log parent access code validation result.
constexpr char kUMAParentAccessCodeValidationResultBase[] =
    "Supervision.ParentAccessCode.ValidationResult";

// Suffix of the histogram to log aggregated parent access code validation
// results.
constexpr char kUMAValidationResultSuffixAll[] = "All";

// Suffix of the histogram to log parent access code validation results for
// reauth flow.
constexpr char kUMAValidationResultSuffixReauth[] = "Reauth";

// Suffix of the histogram to log parent access code validation results for add
// user flow.
constexpr char kUMAValidationResultSuffixAddUser[] = "AddUser";

// Suffix of the histogram to log parent access code validation results for time
// limits override.
constexpr char kUMAValidationResultSuffixTimeLimits[] = "TimeLimits";

// Suffix of the histogram to log parent access code validation results for
// timezone change.
constexpr char kUMAValidationResultSuffixTimezone[] = "TimezoneChange";

// Suffix of the histogram to log parent access code validation results for
// clock change.
constexpr char kUMAValidationResultSuffixClock[] = "ClockChange";

std::u16string GetTitle(SupervisedAction action) {
  int title_id;
  switch (action) {
    case SupervisedAction::kUnlockTimeLimits:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_TITLE;
      break;
    case SupervisedAction::kUpdateClock:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_CHANGE_TIME;
      break;
    case SupervisedAction::kUpdateTimezone:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_CHANGE_TIMEZONE;
      break;
    case SupervisedAction::kAddUser:
    case SupervisedAction::kReauth:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_GENERIC_TITLE;
      break;
  }
  return l10n_util::GetStringUTF16(title_id);
}

std::u16string GetDescription(SupervisedAction action) {
  int description_id;
  switch (action) {
    case SupervisedAction::kUnlockTimeLimits:
      description_id = IDS_ASH_LOGIN_PARENT_ACCESS_DESCRIPTION;
      break;
    case SupervisedAction::kUpdateClock:
    case SupervisedAction::kUpdateTimezone:
      description_id = IDS_ASH_LOGIN_PARENT_ACCESS_GENERIC_DESCRIPTION;
      break;
    case SupervisedAction::kAddUser:
      description_id = IDS_ASH_LOGIN_PARENT_ACCESS_DESCRIPTION_ADD_USER;
      break;
    case SupervisedAction::kReauth:
      description_id = IDS_ASH_LOGIN_PARENT_ACCESS_DESCRIPTION_REAUTH;
      break;
  }
  return l10n_util::GetStringUTF16(description_id);
}

std::u16string GetAccessibleTitle() {
  return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PARENT_ACCESS_DIALOG_NAME);
}

void RecordParentCodeValidationResultToHistogram(
    ParentCodeValidationResult result,
    const std::string& histogram_name) {
  DCHECK(!histogram_name.empty());
  switch (result) {
    case ParentCodeValidationResult::kValid:
      base::UmaHistogramEnumeration(
          histogram_name,
          ParentAccessControllerImpl::UMAValidationResult::kValid);
      return;
    case ParentCodeValidationResult::kInvalid:
      base::UmaHistogramEnumeration(
          histogram_name,
          ParentAccessControllerImpl::UMAValidationResult::kInvalid);
      return;
    case ParentCodeValidationResult::kNoConfig:
      base::UmaHistogramEnumeration(
          histogram_name,
          ParentAccessControllerImpl::UMAValidationResult::kNoConfig);
      return;
    case ParentCodeValidationResult::kInternalError:
      base::UmaHistogramEnumeration(
          histogram_name,
          ParentAccessControllerImpl::UMAValidationResult::kInternalError);
      return;
  }
}

void RecordParentCodeValidationResult(ParentCodeValidationResult result,
                                      SupervisedAction action) {
  // Record to the action specific histogram.
  const std::string action_result_histogram =
      ParentAccessControllerImpl::GetUMAParentCodeValidationResultHistorgam(
          action);
  RecordParentCodeValidationResultToHistogram(result, action_result_histogram);

  // Record the action to the aggregated histogram.
  const std::string all_results_histogram =
      ParentAccessControllerImpl::GetUMAParentCodeValidationResultHistorgam(
          std::nullopt);
  RecordParentCodeValidationResultToHistogram(result, all_results_histogram);
}

}  // namespace

// static
constexpr char ParentAccessControllerImpl::kUMAParentAccessCodeAction[];

// static
constexpr char ParentAccessControllerImpl::kUMAParentAccessCodeUsage[];

// static
std::string
ParentAccessControllerImpl::GetUMAParentCodeValidationResultHistorgam(
    std::optional<SupervisedAction> action) {
  const std::string separator = ".";
  if (!action) {
    return base::JoinString({kUMAParentAccessCodeValidationResultBase,
                             kUMAValidationResultSuffixAll},
                            separator);
  }

  switch (action.value()) {
    case SupervisedAction::kUnlockTimeLimits:
      return base::JoinString({kUMAParentAccessCodeValidationResultBase,
                               kUMAValidationResultSuffixTimeLimits},
                              separator);
    case SupervisedAction::kAddUser:
      return base::JoinString({kUMAParentAccessCodeValidationResultBase,
                               kUMAValidationResultSuffixAddUser},
                              separator);
    case SupervisedAction::kReauth:
      return base::JoinString({kUMAParentAccessCodeValidationResultBase,
                               kUMAValidationResultSuffixReauth},
                              separator);
    case SupervisedAction::kUpdateTimezone:
      return base::JoinString({kUMAParentAccessCodeValidationResultBase,
                               kUMAValidationResultSuffixTimezone},
                              separator);
    case SupervisedAction::kUpdateClock:
      return base::JoinString({kUMAParentAccessCodeValidationResultBase,
                               kUMAValidationResultSuffixClock},
                              separator);
  }
}

ParentAccessControllerImpl::ParentAccessControllerImpl() = default;

ParentAccessControllerImpl::~ParentAccessControllerImpl() = default;

void RecordParentAccessAction(ParentAccessControllerImpl::UMAAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      ParentAccessControllerImpl::kUMAParentAccessCodeAction, action);
}

void RecordParentAccessUsage(const AccountId& child_account_id,
                             SupervisedAction action) {
  switch (action) {
    case SupervisedAction::kUnlockTimeLimits: {
      UMA_HISTOGRAM_ENUMERATION(
          ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
          ParentAccessControllerImpl::UMAUsage::kTimeLimits);
      return;
    }
    case SupervisedAction::kUpdateClock: {
      bool is_login = Shell::Get()->session_controller()->GetSessionState() ==
                      session_manager::SessionState::LOGIN_PRIMARY;
      UMA_HISTOGRAM_ENUMERATION(
          ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
          is_login
              ? ParentAccessControllerImpl::UMAUsage::kTimeChangeLoginScreen
              : ParentAccessControllerImpl::UMAUsage::kTimeChangeInSession);
      return;
    }
    case SupervisedAction::kUpdateTimezone: {
      UMA_HISTOGRAM_ENUMERATION(
          ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
          ParentAccessControllerImpl::UMAUsage::kTimezoneChange);
      return;
    }
    case SupervisedAction::kAddUser:
      UMA_HISTOGRAM_ENUMERATION(
          ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
          ParentAccessControllerImpl::UMAUsage::kAddUserLoginScreen);
      return;
    case SupervisedAction::kReauth:
      UMA_HISTOGRAM_ENUMERATION(
          ParentAccessControllerImpl::kUMAParentAccessCodeUsage,
          ParentAccessControllerImpl::UMAUsage::kReauhLoginScreen);
      return;
  }
  NOTREACHED() << "Unknown SupervisedAction";
}

PinRequestView::SubmissionResult ParentAccessControllerImpl::OnPinSubmitted(
    const std::string& pin) {
  ParentCodeValidationResult pin_validation_result =
      Shell::Get()->login_screen_controller()->ValidateParentAccessCode(
          account_id_, validation_time_, pin);
  RecordParentCodeValidationResult(pin_validation_result, action_);

  if (pin_validation_result == ParentCodeValidationResult::kValid) {
    VLOG(1) << "Parent access code successfully validated";
    RecordParentAccessAction(
        ParentAccessControllerImpl::UMAAction::kValidationSuccess);
    return PinRequestView::SubmissionResult::kPinAccepted;
  }

  VLOG(1) << "Invalid parent access code entered";
  RecordParentAccessAction(
      ParentAccessControllerImpl::UMAAction::kValidationError);
  PinRequestWidget::Get()->UpdateState(
      PinRequestViewState::kError,
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_ERROR),
      GetDescription(action_));
  return PinRequestView::SubmissionResult::kPinError;
}

void ParentAccessControllerImpl::OnBack() {
  RecordParentAccessAction(
      ParentAccessControllerImpl::UMAAction::kCanceledByUser);
}

void ParentAccessControllerImpl::OnHelp() {
  RecordParentAccessAction(ParentAccessControllerImpl::UMAAction::kGetHelp);
  // TODO(crbug.com/40642787): Remove this when handling touch
  // cancellation is fixed for system modal windows.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        Shell::Get()->login_screen_controller()->ShowParentAccessHelpApp();
      }));
}

bool ParentAccessControllerImpl::ShowWidget(
    const AccountId& child_account_id,
    PinRequest::OnPinRequestDone on_exit_callback,
    SupervisedAction action,
    bool extra_dimmer,
    base::Time validation_time) {
  if (PinRequestWidget::Get())
    return false;

  // When there is no logged in user we should accept parent access code for any
  // of child account added to the device.
  const auto session_state =
      Shell::Get()->session_controller()->GetSessionState();
  const bool user_in_session =
      session_state == session_manager::SessionState::LOGGED_IN_NOT_ACTIVE ||
      session_state == session_manager::SessionState::ACTIVE ||
      session_state == session_manager::SessionState::LOCKED;
  DCHECK(user_in_session || child_account_id.empty());

  account_id_ = child_account_id;
  action_ = action;
  validation_time_ = validation_time;
  PinRequest request;
  request.on_pin_request_done = std::move(on_exit_callback);
  request.help_button_enabled = true;
  request.extra_dimmer = extra_dimmer;
  request.pin_length = kParentAccessCodePinLength;
  request.obscure_pin = false;
  request.title = GetTitle(action);
  request.description = GetDescription(action);
  request.accessible_title = GetAccessibleTitle();
  PinRequestWidget::Show(std::move(request), this);
  RecordParentAccessUsage(account_id_, action);
  return true;
}

}  // namespace ash
