// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/ui/android/infobars/grouped_permission_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

using PrimaryButtonBehavior =
    QuietPermissionPromptModelAndroid::PrimaryButtonBehavior;
using SecondaryButtonBehavior =
    QuietPermissionPromptModelAndroid::SecondaryButtonBehavior;

}  // namespace

GroupedPermissionInfoBarDelegate::~GroupedPermissionInfoBarDelegate() {
  permissions::PermissionUmaUtil::RecordInfobarDetailsExpanded(
      details_expanded_);
}

// static
infobars::InfoBar* GroupedPermissionInfoBarDelegate::Create(
    const base::WeakPtr<permissions::PermissionPromptAndroid>&
        permission_prompt,
    infobars::ContentInfoBarManager* infobar_manager) {
  // WrapUnique needs to be used because the constructor is private.
  return infobar_manager->AddInfoBar(std::make_unique<GroupedPermissionInfoBar>(
      base::WrapUnique(new GroupedPermissionInfoBarDelegate(permission_prompt,
                                                            infobar_manager))));
}

size_t GroupedPermissionInfoBarDelegate::PermissionCount() const {
  return permission_prompt_->PermissionCount();
}

ContentSettingsType GroupedPermissionInfoBarDelegate::GetContentSettingType(
    size_t position) const {
  return permission_prompt_->GetContentSettingType(position);
}

std::u16string GroupedPermissionInfoBarDelegate::GetCompactMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_NOTIFICATION_QUIET_PERMISSION_MINI_INFOBAR_MESSAGE);
}

std::u16string GroupedPermissionInfoBarDelegate::GetCompactLinkText() const {
  switch (QuietNotificationPermissionUiConfig::GetMiniInfobarExpandLinkText()) {
    case QuietNotificationPermissionUiConfig::InfobarLinkTextVariation::kManage:
      return l10n_util::GetStringUTF16(IDS_NOTIFICATION_BUTTON_MANAGE);
    case QuietNotificationPermissionUiConfig::InfobarLinkTextVariation::
        kDetails:
      return l10n_util::GetStringUTF16(
          IDS_NOTIFICATION_QUIET_PERMISSION_MINI_INFOBAR_DETAILS_LINK);
  }

  NOTREACHED();
  return std::u16string();
}

// TODO(crbug.com/1082737): Many methods of this class switches on the quiet UI
// reason. Refactor this into separate subclasses instead.
std::u16string GroupedPermissionInfoBarDelegate::GetDescriptionText() const {
  return prompt_model_.description;
}

bool GroupedPermissionInfoBarDelegate::ShouldSecondaryButtonOpenSettings()
    const {
  return prompt_model_.secondary_button_behavior ==
         SecondaryButtonBehavior::kShowSettings;
}

int GroupedPermissionInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_NOTIFICATIONS_OFF;
}

std::u16string GroupedPermissionInfoBarDelegate::GetLinkText() const {
  // This will be used as the text of the link in the expanded state.
  return prompt_model_.learn_more_text;
}

GURL GroupedPermissionInfoBarDelegate::GetLinkURL() const {
  return GetNotificationBlockedLearnMoreUrl();
}

bool GroupedPermissionInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  // The link shown in the compact state should expand the infobar, and do
  // nothing more. This is handled entirely in PermissionInfoBar.java.
  if (!details_expanded_) {
    details_expanded_ = true;
    return false;
  }

  permission_prompt_->SetLearnMoreClicked();
  // The link shown in the expanded state is a `Learn more` link. Let the base
  // class handle opening the URL returned by GetLinkURL().
  return ConfirmInfoBarDelegate::LinkClicked(disposition);
}

void GroupedPermissionInfoBarDelegate::InfoBarDismissed() {
  if (permission_prompt_)
    permission_prompt_->Closing();
}

std::u16string GroupedPermissionInfoBarDelegate::GetMessageText() const {
  return prompt_model_.title;
}

bool GroupedPermissionInfoBarDelegate::Accept() {
  if (!permission_prompt_)
    return true;

  switch (prompt_model_.primary_button_behavior) {
    case PrimaryButtonBehavior::kAllowForThisSite:
      permission_prompt_->Accept();
      break;
    case PrimaryButtonBehavior::kContinueBlocking:
      permission_prompt_->Deny();
      break;
  }
  return true;
}

bool GroupedPermissionInfoBarDelegate::Cancel() {
  if (!permission_prompt_)
    return true;
  switch (prompt_model_.secondary_button_behavior) {
    case SecondaryButtonBehavior::kShowSettings:
      // The infobar needs to be kept open after the "Manage" button is clicked.
      permission_prompt_->SetManageClicked();
      return false;
    case SecondaryButtonBehavior::kAllowForThisSite:
      permission_prompt_->Accept();
      return true;
  }
  NOTREACHED();
  return true;
}

GroupedPermissionInfoBarDelegate::GroupedPermissionInfoBarDelegate(
    const base::WeakPtr<permissions::PermissionPromptAndroid>&
        permission_prompt,
    infobars::ContentInfoBarManager* infobar_manager)
    : permission_prompt_(permission_prompt),
      infobar_manager_(infobar_manager),
      details_expanded_(false) {
  DCHECK(permission_prompt_);
  DCHECK(infobar_manager_);

  // Infobars are only used for NOTIFICATIONS right now, therefore strings can
  // be hardcoded for that type.
  DCHECK_EQ(permission_prompt_->GetContentSettingType(0u),
            ContentSettingsType::NOTIFICATIONS);

  auto* manager = permissions::PermissionRequestManager::FromWebContents(
      permission_prompt_->web_contents());

  auto quiet_ui_reason = manager->ReasonForUsingQuietUi();
  DCHECK(quiet_ui_reason);
  prompt_model_ = GetQuietNotificationPermissionPromptModel(*quiet_ui_reason);
}

infobars::InfoBarDelegate::InfoBarIdentifier
GroupedPermissionInfoBarDelegate::GetIdentifier() const {
  return GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID;
}

int GroupedPermissionInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::u16string GroupedPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return (button == BUTTON_OK) ? prompt_model_.primary_button_label
                               : prompt_model_.secondary_button_label;
}

bool GroupedPermissionInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  // The PermissionRequestManager doesn't create duplicate infobars so a pointer
  // equality check is sufficient.
  return this == delegate;
}
