// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_controller.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
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
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/feedback/feedback_constants.h"
#include "components/manta/proto/scanner.pb.h"
#include "ui/base/l10n/l10n_util.h"
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

constexpr size_t kUserFacingStringDepthLimit = 20;
constexpr size_t kUserFacingStringOutputLimit =
    std::numeric_limits<size_t>::max();

std::u16string GetToastMessageForActionSuccess(
    manta::proto::ScannerAction::ActionCase action_case) {
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_SUCCESS_TOAST_CREATE_EVENT);
    case manta::proto::ScannerAction::kNewContact:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_SUCCESS_TOAST_CREATE_CONTACT);
    case manta::proto::ScannerAction::kNewGoogleDoc:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_SUCCESS_TOAST_CREATE_DOC);
    case manta::proto::ScannerAction::kNewGoogleSheet:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_SUCCESS_TOAST_CREATE_SHEET);
    case manta::proto::ScannerAction::kCopyToClipboard:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_SUCCESS_TOAST_COPY_TEXT_AND_FORMAT);
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      NOTREACHED();
  }
}

std::u16string GetToastMessageForActionFailure(
    manta::proto::ScannerAction::ActionCase action_case) {
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_FAILURE_TOAST_CREATE_EVENT);
    case manta::proto::ScannerAction::kNewContact:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_FAILURE_TOAST_CREATE_CONTACT);
    case manta::proto::ScannerAction::kNewGoogleDoc:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_FAILURE_TOAST_CREATE_DOC);
    case manta::proto::ScannerAction::kNewGoogleSheet:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_FAILURE_TOAST_CREATE_SHEET);
    case manta::proto::ScannerAction::kCopyToClipboard:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_FAILURE_TOAST_COPY_TEXT_AND_FORMAT);
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      NOTREACHED();
  }
}

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
void ExecutePopulatedAction(
    manta::proto::ScannerAction::ActionCase action_case,
    base::TimeTicks request_start_time,
    base::WeakPtr<ScannerCommandDelegate> delegate,
    base::OnceCallback<void(manta::proto::ScannerAction populated_action,
                            bool success)> action_finished_callback,
    manta::proto::ScannerAction populated_action) {
  RecordPopulateActionTimer(action_case, request_start_time);
  if (populated_action.action_case() ==
      manta::proto::ScannerAction::ACTION_NOT_SET) {
    RecordPopulateActionFailure(action_case);
    std::move(action_finished_callback).Run(std::move(populated_action), false);
    return;
  }

  ScannerCommandCallback record_metrics_callback = base::BindOnce(
      &RecordActionExecutionAndRun, action_case, base::TimeTicks::Now(),
      base::BindOnce(std::move(action_finished_callback), populated_action));

  HandleScannerCommand(std::move(delegate),
                       ScannerActionToCommand(std::move(populated_action)),
                       std::move(record_metrics_callback));
}

void OnFeedbackFormSendButtonClicked(const AccountId& account_id,
                                     base::Value::Dict action_dict,
                                     ScannerFeedbackInfo feedback_info,
                                     const std::string& user_description) {
  std::optional<std::string> pretty_printed_action = base::WriteJsonWithOptions(
      action_dict, base::JsonOptions::OPTIONS_PRETTY_PRINT);
  // JSON serialisation should always succeed as the depth of the Dict is fixed,
  // and no binary values should appear in the Dict.
  CHECK(pretty_printed_action.has_value());

  // Work around limitations with `feedback::RedactionTool` by prepending two
  // spaces and appending a new line to any data to be redacted.
  std::string description =
      base::StrCat({"details:  ", *pretty_printed_action,
                    "\nuser_description:  ", user_description, "\n"});

  Shell::Get()->shell_delegate()->SendSpecializedFeatureFeedback(
      account_id, feedback::kScannerFeedbackProductId, std::move(description),
      std::string(base::as_string_view(*feedback_info.screenshot)),
      /*image_mime_type=*/"image/jpeg");
}

}  // namespace

ScannerController::ScannerController(std::unique_ptr<ScannerDelegate> delegate,
                                     SessionControllerImpl& session_controller)
    : delegate_(std::move(delegate)), session_controller_(session_controller) {}

ScannerController::~ScannerController() = default;

// static
void ScannerController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kScannerFeedbackEnabled, true);
}

void ScannerController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  scanner_session_ = nullptr;
  command_delegate_ = nullptr;
}

bool ScannerController::CanShowConsentScreenEntryPoints() {
  ScannerProfileScopedDelegate* profile_scoped_delegate =
      delegate_->GetProfileScopedDelegate();

  if (profile_scoped_delegate == nullptr) {
    return false;
  }

  specialized_features::FeatureAccessFailureSet checks =
      profile_scoped_delegate->CheckFeatureAccess();

  checks.Remove(
      specialized_features::FeatureAccessFailure::kConsentNotAccepted);
  return checks.empty();
}

bool ScannerController::CanShowFeatureSettingsToggle() {
  ScannerProfileScopedDelegate* profile_scoped_delegate =
      delegate_->GetProfileScopedDelegate();

  if (profile_scoped_delegate == nullptr) {
    return false;
  }

  specialized_features::FeatureAccessFailureSet checks =
      profile_scoped_delegate->CheckFeatureAccess();

  // Show settings toggle even if the setting is disabled or consent not
  // accepted.
  // Hence we ignore these checks if they have failed.
  checks.Remove(
      specialized_features::FeatureAccessFailure::kDisabledInSettings);
  checks.Remove(
      specialized_features::FeatureAccessFailure::kConsentNotAccepted);
  return checks.empty();
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
    scanner_session_ =
        std::make_unique<ScannerSession>(delegate_->GetProfileScopedDelegate());
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
  // Keep the existing `command_delegate_` if there is one, to allow commands
  // from previous sessions to continue in the background if needed.
  if (!command_delegate_) {
    command_delegate_ = std::make_unique<ScannerCommandDelegateImpl>(
        delegate_->GetProfileScopedDelegate());
  }
  const manta::proto::ScannerAction::ActionCase action_case =
      scanner_action.GetActionCase();
  scanner_session_->PopulateAction(
      scanner_action.downscaled_jpeg_bytes(),
      scanner_action.unpopulated_action(),
      base::BindOnce(&ExecutePopulatedAction, action_case,
                     base::TimeTicks::Now(), command_delegate_->GetWeakPtr(),
                     base::BindOnce(&ScannerController::OnActionFinished,
                                    weak_ptr_factory_.GetWeakPtr(), action_case,
                                    scanner_action.downscaled_jpeg_bytes())));
  ShowActionProgressNotification(action_case);
}

void ScannerController::OpenFeedbackDialog(
    const AccountId& account_id,
    manta::proto::ScannerAction action,
    scoped_refptr<base::RefCountedMemory> screenshot) {
  base::Value::Dict action_dict = ScannerActionToDict(std::move(action));

  std::optional<std::string> user_facing_string = ValueToUserFacingString(
      action_dict, kUserFacingStringDepthLimit, kUserFacingStringOutputLimit);
  // `user_facing_string` can only be nullopt if:
  // - `ScannerActionToDict` output a binary value, which is impossible,
  // - `ScannerActionToDict` output a more-than-twenty nested value, which is
  //   impossible (all returned values are at most three-nested)
  // - the excessively large output limit is hit, which should be impossible.
  CHECK(user_facing_string.has_value());

  delegate_->OpenFeedbackDialog(
      account_id,
      ScannerFeedbackInfo(std::move(*user_facing_string),
                          std::move(screenshot)),
      base::BindOnce(&OnFeedbackFormSendButtonClicked, account_id,
                     std::move(action_dict)));
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
    scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes,
    manta::proto::ScannerAction populated_action,
    bool success) {
  // Remove the action progress notification.
  message_center::MessageCenter::Get()->RemoveNotification(
      kScannerActionNotificationId,
      /*by_user=*/false);

  if (success) {
    ToastData toast_data(kScannerActionSuccessToastId,
                         ToastCatalogName::kScannerActionSuccess,
                         GetToastMessageForActionSuccess(action_case));

    // TODO: b/367882164 - Pass in the account ID to this method to ensure that
    // the feedback form is shown for the same account that performed the
    // action.
    const AccountId& account_id = session_controller_->GetActiveAccountId();
    PrefService* prefs =
        session_controller_->GetUserPrefServiceForUser(account_id);

    if (prefs && prefs->GetBoolean(prefs::kScannerFeedbackEnabled)) {
      toast_data.button_type = ToastData::ButtonType::kIconButton;
      toast_data.button_text = l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_TOAST_FEEDBACK_ICON_ACCESSIBLE_NAME);
      toast_data.button_icon = &kFeedbackIcon;
      // TODO: b/259100049 - Change this to be `BindOnce` once
      // `ToastData::button_callback` is migrated to be a `OnceClosure`.
      toast_data.button_callback = base::BindRepeating(
          &ScannerController::OpenFeedbackDialog,
          weak_ptr_factory_.GetWeakPtr(), account_id,
          std::move(populated_action), std::move(downscaled_jpeg_bytes));
    }

    ToastManager::Get()->Show(std::move(toast_data));
  } else {
    ToastManager::Get()->Show(ToastData(
        kScannerActionFailureToastId, ToastCatalogName::kScannerActionFailure,
        GetToastMessageForActionFailure(action_case)));
  }

  if (!on_action_finished_for_testing_.is_null()) {
    CHECK_IS_TEST();
    std::move(on_action_finished_for_testing_).Run(success);
  }
}

}  // namespace ash
