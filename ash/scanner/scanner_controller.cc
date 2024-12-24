// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/scanner_command_delegate_impl.h"
#include "ash/scanner/scanner_session.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/account_id/account_id.h"
#include "components/manta/proto/scanner.pb.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

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

// Should be called when an action finishes execution.
void OnActionFinished(manta::proto::ScannerAction::ActionCase action_case,
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

  return profile_scoped_delegate->GetSystemState().status ==
         ScannerStatus::kEnabled;
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
  const manta::proto::ScannerAction::ActionCase action_case =
      scanner_action.GetActionCase();
  scanner_action.ExecuteAction(base::BindOnce(&OnActionFinished, action_case));
  ShowActionProgressNotification(action_case);
}

bool ScannerController::HasActiveSessionForTesting() const {
  return !!scanner_session_;
}

}  // namespace ash
