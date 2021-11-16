// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notification_blocked_message_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/permissions/android/permission_prompt_android.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

NotificationBlockedMessageDelegate::NotificationBlockedMessageDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<Delegate> delegate)
    : web_contents_(web_contents), delegate_(std::move(delegate)) {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::NOTIFICATION_BLOCKED,
      base::BindOnce(
          &NotificationBlockedMessageDelegate::HandlePrimaryActionClick,
          base::Unretained(this)),
      base::BindOnce(&NotificationBlockedMessageDelegate::HandleDismissCallback,
                     base::Unretained(this)));
  message_->SetTitle(l10n_util::GetStringUTF16(
      IDS_NOTIFICATION_QUIET_PERMISSION_INFOBAR_TITLE));

  // IDS_OK: notification will still be blocked if primary button is clicked.
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));
  message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDROID_INFOBAR_NOTIFICATIONS_OFF));
  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  message_->SetSecondaryButtonMenuText(
      l10n_util::GetStringUTF16(IDS_NOTIFICATION_BUTTON_MANAGE));

  message_->SetSecondaryActionCallback(
      base::BindOnce(&NotificationBlockedMessageDelegate::HandleManageClick,
                     base::Unretained(this)));
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

NotificationBlockedMessageDelegate::~NotificationBlockedMessageDelegate() {
  DismissInternal();
}

void NotificationBlockedMessageDelegate::HandlePrimaryActionClick() {
  if (!delegate_ || delegate_->IsPromptDestroyed())
    return;

  DCHECK(delegate_->ShouldUseQuietUI());
  delegate_->Deny();
}

void NotificationBlockedMessageDelegate::HandleManageClick() {
  // TODO(crbug.com/1230927): Implement the flow of showing dialogs
}

void NotificationBlockedMessageDelegate::HandleDismissCallback(
    messages::DismissReason reason) {
  // When message is dismissed by secondary action, |permission_prompt_| should
  // be reset when the dialog is dismissed.
  if (reason != messages::DismissReason::SECONDARY_ACTION && delegate_ &&
      !delegate_->IsPromptDestroyed()) {
    delegate_->Closing();
  }
  delegate_.reset();
  message_.reset();
}

void NotificationBlockedMessageDelegate::DismissInternal() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void NotificationBlockedMessageDelegate::Delegate::Accept() {}

void NotificationBlockedMessageDelegate::Delegate::Deny() {
  if (!permission_prompt_)
    return;
  permission_prompt_->Deny();
}

void NotificationBlockedMessageDelegate::Delegate::Closing() {
  if (!permission_prompt_)
    return;
  permission_prompt_->Closing();
  permission_prompt_.reset();
}

bool NotificationBlockedMessageDelegate::Delegate::IsPromptDestroyed() {
  return !permission_prompt_;
}

bool NotificationBlockedMessageDelegate::Delegate::ShouldUseQuietUI() {
  return permission_prompt_->ShouldCurrentRequestUseQuietUI();
}

NotificationBlockedMessageDelegate::Delegate::~Delegate() {
  Closing();
}

NotificationBlockedMessageDelegate::Delegate::Delegate() {}

NotificationBlockedMessageDelegate::Delegate::Delegate(
    const base::WeakPtr<permissions::PermissionPromptAndroid>&
        permission_prompt)
    : permission_prompt_(permission_prompt) {}
