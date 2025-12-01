// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_blocked_message_delegate_android.h"

#include "base/metrics/histogram_functions.h"
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
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
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
  if (delegate_->ShouldUseQuietUI()) {
    InitializeQuietUI();
  } else {
    InitializeLoudUI();
  }
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

void PermissionBlockedMessageDelegate::InitializeLoudUI() {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::PERMISSION_PROMPT_LOUD,
      base::BindOnce(
          &PermissionBlockedMessageDelegate::HandleLoudPrimaryActionClick,
          base::Unretained(this)),
      base::BindOnce(
          &PermissionBlockedMessageDelegate::HandleLoudDismissCallback,
          base::Unretained(this)));

  message_->SetTitle(
      l10n_util::GetStringUTF16(IDS_NOTIFICATION_TITLE_MESSAGE_UI));

  const std::vector<base::WeakPtr<permissions::PermissionRequest>>& requests =
      delegate_->permission_prompt()->Requests();

  std::u16string requesting_origin_string_formatted =
      url_formatter::FormatUrlForSecurityDisplay(
          requests[0].get()->requesting_origin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  message_->SetDescription(
      l10n_util::GetStringFUTF16(IDS_NOTIFICATION_DESCRIPTION_MESSAGE_UI,
                                 requesting_origin_string_formatted));
  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_NOTIFICATION_CTA_MESSAGE_UI));

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

void PermissionBlockedMessageDelegate::InitializeQuietUI() {
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::PERMISSION_BLOCKED,
      base::BindOnce(
          &PermissionBlockedMessageDelegate::HandleQuietPrimaryActionClick,
          base::Unretained(this)),
      base::BindOnce(
          &PermissionBlockedMessageDelegate::HandleQuietDismissCallback,
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
    case ContentSettingsType::GEOLOCATION_WITH_OPTIONS:
      title = IDS_LOCATION_QUIET_PERMISSION_MESSAGE_UI_TITLE;
      icon = IDR_ANDROID_MESSAGE_LOCATION_OFF;
      break;
    default:
      NOTREACHED();
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

void PermissionBlockedMessageDelegate::HandleQuietPrimaryActionClick() {
  delegate_->Deny();
}

void PermissionBlockedMessageDelegate::HandleManageClick() {
  if (!dialog_controller_) {
    dialog_controller_ = std::make_unique<PermissionBlockedDialogController>(
        this, web_contents_);
  }
  dialog_controller_->ShowDialog(*delegate_->ReasonForUsingQuietUi());
  messages::MessageDispatcherBridge::Get()->DismissMessage(
      message_.get(), messages::DismissReason::SECONDARY_ACTION);
}

void PermissionBlockedMessageDelegate::HandleQuietDismissCallback(
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

void PermissionBlockedMessageDelegate::HandleLoudPrimaryActionClick() {
  if (!dialog_controller_) {
    dialog_controller_ = std::make_unique<PermissionBlockedDialogController>(
        this, web_contents_);
  }

  base::UmaHistogramBoolean("Permissions.ClapperLoud.MessageUI.Manage", true);

  dialog_controller_->ShowPageInfo();
  messages::MessageDispatcherBridge::Get()->DismissMessage(
      message_.get(), messages::DismissReason::PRIMARY_ACTION);
}

void PermissionBlockedMessageDelegate::HandleLoudDismissCallback(
    messages::DismissReason reason) {
  message_.reset();

  // There is no secondary action for the message UI for loud prompts.
  // There is only the primary "Manage" action.
  if (reason == messages::DismissReason::SECONDARY_ACTION) {
    NOTREACHED();
  }

  dialog_controller_.reset();

  if (reason == messages::DismissReason::GESTURE) {
    // TODO(crbug.com/458351800): Add tests for the loud prompts via the
    // message UI.

    // If the use has intentionally dismissed the dialog, it is processed as the
    // deny action. Hence the separate histogram is needed to differentiate
    // between deny and dismiss actions.
    base::UmaHistogramBoolean("Permissions.ClapperLoud.MessageUI.Dismiss",
                              true);

    delegate_->Deny();
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

PermissionBlockedMessageDelegate::Delegate::Delegate() = default;

PermissionBlockedMessageDelegate::Delegate::Delegate(
    const base::WeakPtr<permissions::PermissionPromptAndroid>&
        permission_prompt)
    : permission_prompt_(permission_prompt) {}
