// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_blocked_message_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/permissions/quiet_permission_prompt_model_android.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

PermissionBlockedMessageDelegate::PermissionBlockedMessageDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<Delegate> delegate)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      delegate_(std::move(delegate)) {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::PERMISSION_BLOCKED,
      base::BindOnce(
          &PermissionBlockedMessageDelegate::HandlePrimaryActionClick,
          base::Unretained(this)),
      base::BindOnce(&PermissionBlockedMessageDelegate::HandleDismissCallback,
                     base::Unretained(this)));
  const ContentSettingsType content_setting_type =
      delegate_->GetContentSettingsType();
  int title = 0;
  int icon = 0;
  switch (content_setting_type) {
    case ContentSettingsType::NOTIFICATIONS:
      title = IDS_NOTIFICATION_QUIET_PERMISSION_INFOBAR_TITLE;
      icon = IDR_ANDROID_INFOBAR_NOTIFICATIONS_OFF;
      break;
    case ContentSettingsType::GEOLOCATION:
      title = IDS_LOCATION_QUIET_PERMISSION_MESSAGE_UI_TITLE;
      icon = IDR_ANDROID_MESSAGE_LOCATION_OFF;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  message_->SetTitle(l10n_util::GetStringUTF16(title));

  // IDS_OK: notification will still be blocked if primary button is clicked.
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));
  message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(icon));
  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));

  message_->SetSecondaryActionCallback(
      base::BindRepeating(&PermissionBlockedMessageDelegate::HandleManageClick,
                          base::Unretained(this)));
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

PermissionBlockedMessageDelegate::~PermissionBlockedMessageDelegate() {
  DismissInternal();
}

void PermissionBlockedMessageDelegate::OnContinueBlocking() {
  has_interacted_with_dialog_ = true;
  dialog_controller_.reset();
  delegate_->Deny();
}

void PermissionBlockedMessageDelegate::OnAllowForThisSite() {
  has_interacted_with_dialog_ = true;
  dialog_controller_.reset();
  delegate_->Accept();
}

void PermissionBlockedMessageDelegate::OnLearnMoreClicked() {
  should_reshow_dialog_on_focus_ = true;
  dialog_controller_->DismissDialog();
  delegate_->SetLearnMoreClicked();
  web_contents_->OpenURL(
      content::OpenURLParams(GetNotificationBlockedLearnMoreUrl(),
                             content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

void PermissionBlockedMessageDelegate::OnOpenedSettings() {
  should_reshow_dialog_on_focus_ = true;
  delegate_->SetManageClicked();
}

void PermissionBlockedMessageDelegate::OnDialogDismissed() {
  if (!dialog_controller_) {
    // Dismissed by clicking on dialog buttons.
    return;
  }
  if (should_reshow_dialog_on_focus_) {
    // When the dialog has been dismissed due to the user clicking on
    // 'Learn more', do not clean up the dialog instance as the dialog
    // will be restored when the user navigates back to the original tab.
    return;
  }
  dialog_controller_.reset();
  // If |has_interacted_with_dialog_| is true, |Allow| or |Deny| should be
  // recorded instead.
  if (!has_interacted_with_dialog_) {
    // call Closing destroys the current object.
    delegate_->Closing();
  }
}

ContentSettingsType PermissionBlockedMessageDelegate::GetContentSettingsType() {
  return delegate_->GetContentSettingsType();
}

void PermissionBlockedMessageDelegate::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (should_reshow_dialog_on_focus_ && dialog_controller_) {
    // This will be true only if the user has been redirected to
    // a new tab by clicking on 'Learn more' on the dialog.
    // Upon returning to the original tab from the redirected tab,
    // the dialog will be restored.
    should_reshow_dialog_on_focus_ = false;
    // If the page is navigated to another url, |this| will be destroyed
    // by the PermissionRequestManager, thereby causing message to be
    // dismissed and dialog_controller to dismiss the dialog.
    dialog_controller_->ShowDialog(*delegate_->ReasonForUsingQuietUi());
    // TODO(crbug.com/40818674): add browser tests to test if
    // webcontents have been navigated to another page in the meantime.
  }
}

void PermissionBlockedMessageDelegate::HandlePrimaryActionClick() {
  DCHECK(delegate_->ShouldUseQuietUI());
  delegate_->Deny();
}

void PermissionBlockedMessageDelegate::HandleManageClick() {
  DCHECK(!dialog_controller_);
  dialog_controller_ =
      std::make_unique<PermissionBlockedDialogController>(this, web_contents_);
  dialog_controller_->ShowDialog(*delegate_->ReasonForUsingQuietUi());
  messages::MessageDispatcherBridge::Get()->DismissMessage(
      message_.get(), messages::DismissReason::SECONDARY_ACTION);
}

void PermissionBlockedMessageDelegate::HandleDismissCallback(
    messages::DismissReason reason) {
  message_.reset();

  // When message is dismissed by secondary action, |permission_prompt_| should
  // be reset when the dialog is dismissed.
  if (reason == messages::DismissReason::SECONDARY_ACTION) {
    return;
  }

  dialog_controller_.reset();

  if (reason == messages::DismissReason::GESTURE) {
    delegate_->Closing();
  }
  // Other un-tracked actions will be recorded as "Ignored" by
  // |permission_prompt_|.
}

void PermissionBlockedMessageDelegate::DismissInternal() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void PermissionBlockedMessageDelegate::Delegate::Accept() {
  if (!permission_prompt_) {
    return;
  }
  permission_prompt_->Accept();
}

void PermissionBlockedMessageDelegate::Delegate::Deny() {
  if (!permission_prompt_) {
    return;
  }
  permission_prompt_->Deny();
}

void PermissionBlockedMessageDelegate::Delegate::Closing() {
  if (!permission_prompt_) {
    return;
  }
  permission_prompt_->Closing();
}

void PermissionBlockedMessageDelegate::Delegate::SetManageClicked() {
  if (!permission_prompt_) {
    return;
  }
  permission_prompt_->SetManageClicked();
}

void PermissionBlockedMessageDelegate::Delegate::SetLearnMoreClicked() {
  if (!permission_prompt_) {
    return;
  }
  permission_prompt_->SetLearnMoreClicked();
}

bool PermissionBlockedMessageDelegate::Delegate::ShouldUseQuietUI() {
  return permission_prompt_->ShouldCurrentRequestUseQuietUI();
}

std::optional<permissions::PermissionUiSelector::QuietUiReason>
PermissionBlockedMessageDelegate::Delegate::ReasonForUsingQuietUi() {
  return permission_prompt_->ReasonForUsingQuietUi();
}

ContentSettingsType
PermissionBlockedMessageDelegate::Delegate::GetContentSettingsType() {
  // QuietUI is only supported for notifications and geolocation so there will
  // be only one ContentSettingsType in the queue.
  return permission_prompt_->GetContentSettingType(0);
}
PermissionBlockedMessageDelegate::Delegate::~Delegate() {
  Closing();
}

PermissionBlockedMessageDelegate::Delegate::Delegate() {}

PermissionBlockedMessageDelegate::Delegate::Delegate(
    const base::WeakPtr<permissions::PermissionPromptAndroid>&
        permission_prompt)
    : permission_prompt_(permission_prompt) {}
