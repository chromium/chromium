// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/scanner_action_handler.h"
#include "ash/scanner/scanner_command_delegate_impl.h"
#include "ash/scanner/scanner_feedback.h"
#include "ash/scanner/scanner_metrics.h"
#include "ash/scanner/scanner_session.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/feedback/feedback_constants.h"
#include "components/manta/proto/scanner.pb.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

using enum ScannerFeatureUserState;

constexpr char kScannerActionNotificationId[] = "scanner_action_notification";
constexpr char kScannerNotifierId[] = "ash.scanner";

constexpr char kScannerActionSuccessToastId[] = "scanner_action_success";
constexpr char kScannerActionFailureToastId[] = "scanner_action_failure";

// Shows an action progress notification. Note that this will remove the
// previous action notification if there is one.
void ShowActionProgressNotification(
    manta::proto::ScannerAction::ActionCase action_case) {
  message_center::RichNotificationData optional_fields;
  // Show an infinite loading progress bar.
  optional_fields.progress = -1;
  optional_fields.never_timeout = true;

  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(kScannerActionNotificationId,
                                     /*by_user=*/false);
  // TODO: crbug.com/375967525 - Finalize the action notification strings and
  // icon.
  message_center->AddNotification(CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_PROGRESS, kScannerActionNotificationId,
      action_case == manta::proto::ScannerAction::kCopyToClipboard
          ? u"Copying text..."
          : u"Creating...",
      /*message=*/u"",
      /*display_source=*/u"", GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kScannerNotifierId,
                                 NotificationCatalogName::kScannerAction),
      optional_fields, /*delegate=*/nullptr, kCaptureModeIcon,
      message_center::SystemNotificationWarningLevel::NORMAL));
}

void RecordExecutePopulatedActionTimer(
    manta::proto::ScannerAction::ActionCase action_case,
    base::TimeTicks execute_start_time) {
  // TODO(b/363101363): Add tests.
  std::string_view variant_name;
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      variant_name = kScannerFeatureTimerExecutePopulatedNewCalendarEventAction;
      break;
    case manta::proto::ScannerAction::kNewContact:
      variant_name = kScannerFeatureTimerExecutePopulatedNewContactAction;
      break;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      variant_name = kScannerFeatureTimerExecutePopulatedNewGoogleDocAction;
      break;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      variant_name = kScannerFeatureTimerExecutePopulatedNewGoogleSheetAction;
      break;
    case manta::proto::ScannerAction::kCopyToClipboard:
      variant_name =
          kScannerFeatureTimerExecutePopulatedNewCopyToClipboardAction;
      break;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      break;
  }
  if (variant_name.empty()) {
    return;
  }
  base::UmaHistogramMediumTimes(variant_name,
                                base::TimeTicks::Now() - execute_start_time);
}

void RecordPopulateActionTimer(
    manta::proto::ScannerAction::ActionCase action_case,
    base::TimeTicks request_start_time) {
  // TODO(b/363101363): Add tests.
  std::string_view variant_name;
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      variant_name = kScannerFeatureTimerPopulateNewCalendarEventAction;
      break;
    case manta::proto::ScannerAction::kNewContact:
      variant_name = kScannerFeatureTimerPopulateNewContactAction;
      break;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      variant_name = kScannerFeatureTimerPopulateNewGoogleDocAction;
      break;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      variant_name = kScannerFeatureTimerPopulateNewGoogleSheetAction;
      break;
    case manta::proto::ScannerAction::kCopyToClipboard:
      variant_name = kScannerFeatureTimerPopulateNewCopyToClipboardAction;
      break;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      break;
  }
  if (variant_name.empty()) {
    return;
  }
  base::UmaHistogramMediumTimes(variant_name,
                                base::TimeTicks::Now() - request_start_time);
}

void RecordPopulateActionFailure(
    manta::proto::ScannerAction::ActionCase action_case) {
  // TODO(b/363101363): Add tests.
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      RecordScannerFeatureUserState(kNewCalendarEventActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::kNewContact:
      RecordScannerFeatureUserState(kNewContactActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      RecordScannerFeatureUserState(kNewGoogleDocActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      RecordScannerFeatureUserState(kNewGoogleSheetActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::kCopyToClipboard:
      RecordScannerFeatureUserState(kCopyToClipboardActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      return;
  }
}

void RecordActionExecutionAndRun(
    manta::proto::ScannerAction::ActionCase action_case,
    base::TimeTicks execute_start_time,
    ScannerCommandCallback action_finished_callback,
    bool success) {
  // TODO(b/363101363): Add tests.
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      RecordScannerFeatureUserState(
          success ? kNewCalendarEventActionFinishedSuccessfully
                  : kNewCalendarEventPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::kNewContact:
      RecordScannerFeatureUserState(
          success ? kNewContactActionFinishedSuccessfully
                  : kNewContactPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      RecordScannerFeatureUserState(
          success ? kNewGoogleDocActionFinishedSuccessfully
                  : kNewGoogleDocPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      RecordScannerFeatureUserState(
          success ? kNewGoogleSheetActionFinishedSuccessfully
                  : kNewGoogleSheetPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::kCopyToClipboard:
      RecordScannerFeatureUserState(
          success ? kCopyToClipboardActionFinishedSuccessfully
                  : kCopyToClipboardPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      break;
  }
  RecordExecutePopulatedActionTimer(action_case, execute_start_time);
  std::move(action_finished_callback).Run(success);
}

// Executes the populated action, if it exists, calling
// `action_finished_callback` with the result of the execution.
void ExecutePopulatedAction(manta::proto::ScannerAction::ActionCase action_case,
                            base::TimeTicks request_start_time,
                            base::WeakPtr<ScannerCommandDelegate> delegate,
                            ScannerCommandCallback action_finished_callback,
                            manta::proto::ScannerAction populated_action) {
  RecordPopulateActionTimer(action_case, request_start_time);
  if (populated_action.action_case() ==
      manta::proto::ScannerAction::ACTION_NOT_SET) {
    RecordPopulateActionFailure(action_case);
    std::move(action_finished_callback).Run(false);
    return;
  }

  ScannerCommandCallback record_metrics_callback = base::BindOnce(
      &RecordActionExecutionAndRun, action_case, base::TimeTicks::Now(),
      std::move(action_finished_callback));

  HandleScannerCommand(std::move(delegate),
                       ScannerActionToCommand(std::move(populated_action)),
                       std::move(record_metrics_callback));
}

void OnFeedbackFormSendButtonClicked(const AccountId& account_id,
                                     ScannerFeedbackInfo feedback_info,
                                     const std::string& user_description) {
  // Work around limitations with `feedback::RedactionTool` by prepending two
  // spaces and appending a new line to any data to be redacted.
  std::string description =
      base::StrCat({"details:  ", feedback_info.action_details,
                    "\nuser_description:  ", user_description, "\n"});

  Shell::Get()->shell_delegate()->SendSpecializedFeatureFeedback(
      account_id, feedback::kScannerFeedbackProductId, std::move(description),
      std::string(base::as_string_view(*feedback_info.screenshot)),
      /*image_mime_type=*/"image/jpeg");
}

}  // namespace

ScannerController::ScannerController(std::unique_ptr<ScannerDelegate> delegate)
    : delegate_(std::move(delegate)) {}

ScannerController::~ScannerController() = default;

void ScannerController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  scanner_session_ = nullptr;
  command_delegate_ = nullptr;
}

bool ScannerController::CanStartSession() {
  ScannerProfileScopedDelegate* profile_scoped_delegate =
      delegate_->GetProfileScopedDelegate();

  if (profile_scoped_delegate == nullptr) {
    return false;
  }

  return profile_scoped_delegate->CheckFeatureAccess().empty();
}

ScannerSession* ScannerController::StartNewSession() {
  // Reset the current session if there is one. We do this here to ensure that
  // the old session is destroyed before attempting to create the new session
  // (to avoid subtle issues from having simultaneously existing sessions).
  scanner_session_ = nullptr;
  if (CanStartSession()) {
    ScannerProfileScopedDelegate* profile_scoped_delegate =
        delegate_->GetProfileScopedDelegate();
    // Keep the existing `command_delegate_` if there is one, to allow commands
    // from previous sessions to continue in the background if needed.
    if (command_delegate_ == nullptr) {
      command_delegate_ =
          std::make_unique<ScannerCommandDelegateImpl>(profile_scoped_delegate);
    }
    scanner_session_ = std::make_unique<ScannerSession>(
        profile_scoped_delegate, command_delegate_.get());
  }
  return scanner_session_.get();
}

void ScannerController::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    ScannerSession::FetchActionsCallback callback) {
  if (!scanner_session_) {
    std::move(callback).Run({});
    return;
  }
  scanner_session_->FetchActionsForImage(jpeg_bytes, std::move(callback));
}

void ScannerController::OnSessionUIClosed() {
  scanner_session_ = nullptr;
}

void ScannerController::ExecuteAction(
    const ScannerActionViewModel& scanner_action) {
  if (!scanner_session_) {
    return;
  }
  const manta::proto::ScannerAction::ActionCase action_case =
      scanner_action.GetActionCase();
  scanner_session_->PopulateAction(
      scanner_action.downscaled_jpeg_bytes(),
      scanner_action.unpopulated_action(),
      base::BindOnce(
          &ExecutePopulatedAction, action_case, base::TimeTicks::Now(),
          scanner_action.delegate(),
          base::BindOnce(&ScannerController::OnActionFinished,
                         weak_ptr_factory_.GetWeakPtr(), action_case)));
  ShowActionProgressNotification(action_case);
}

void ScannerController::OpenFeedbackDialog(
    manta::proto::ScannerAction action,
    scoped_refptr<base::RefCountedMemory> screenshot) {
  // TODO: b/367882164 - Pass in the account ID to this method to ensure that
  // the feedback form is shown for the same account that performed the action.
  const AccountId& account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();

  base::Value::Dict action_dict = ScannerActionToDict(std::move(action));
  std::optional<std::string> pretty_printed_action = base::WriteJsonWithOptions(
      action_dict, base::JsonOptions::OPTIONS_PRETTY_PRINT);
  // JSON serialisation should always succeed as the depth of the Dict is fixed,
  // and no binary values should appear in the Dict.
  CHECK(pretty_printed_action.has_value());

  delegate_->OpenFeedbackDialog(
      account_id,
      ScannerFeedbackInfo(std::move(*pretty_printed_action),
                          std::move(screenshot)),
      base::BindOnce(&OnFeedbackFormSendButtonClicked, account_id));
}

void ScannerController::SetOnActionFinishedForTesting(
    OnActionFinishedCallback callback) {
  on_action_finished_for_testing_ = std::move(callback);
}

bool ScannerController::HasActiveSessionForTesting() const {
  return !!scanner_session_;
}

void ScannerController::OnActionFinished(
    manta::proto::ScannerAction::ActionCase action_case,
    bool success) {
  // Remove the action progress notification.
  message_center::MessageCenter::Get()->RemoveNotification(
      kScannerActionNotificationId,
      /*by_user=*/false);

  if (success) {
    // TODO: crbug.com/375967525 - Finalize the action toast string.
    if (action_case == manta::proto::ScannerAction::kCopyToClipboard) {
      ToastManager::Get()->Show(ToastData(
          kScannerActionSuccessToastId, ToastCatalogName::kScannerActionSuccess,
          u"Text copied to clipboard"));
    }
    // TODO: crbug.com/383925780 - We should also show a toast for other action
    // cases once the feedback mechanism is ready.
  } else {
    // TODO: crbug.com/383926250 - The action failure text should depend on the
    // type of action attempted.
    constexpr char16_t kPlaceholderActionFailureText[] = u"Action Failed";
    ToastManager::Get()->Show(ToastData(kScannerActionFailureToastId,
                                        ToastCatalogName::kScannerActionFailure,
                                        kPlaceholderActionFailureText));
  }

  if (!on_action_finished_for_testing_.is_null()) {
    CHECK_IS_TEST();
    std::move(on_action_finished_for_testing_).Run(success);
  }
}

}  // namespace ash
