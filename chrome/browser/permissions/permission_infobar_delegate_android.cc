// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/ui/android/infobars/permission_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
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

PermissionInfoBarDelegate::~PermissionInfoBarDelegate() {
  permissions::PermissionUmaUtil::RecordInfobarDetailsExpanded(
      details_expanded_);
}

// static
infobars::InfoBar* PermissionInfoBarDelegate::Create(
    const base::WeakPtr<permissions::PermissionPromptAndroid>&
        permission_prompt,
    infobars::ContentInfoBarManager* infobar_manager) {
  // WrapUnique needs to be used because the constructor is private.
  return infobar_manager->AddInfoBar(
      std::make_unique<PermissionInfoBar>(base::WrapUnique(
          new PermissionInfoBarDelegate(permission_prompt, infobar_manager))));
}

size_t PermissionInfoBarDelegate::PermissionCount() const {
  return permission_prompt_->PermissionCount();
}

ContentSettingsType PermissionInfoBarDelegate::GetContentSettingType(
    size_t position) const {
  return permission_prompt_->GetContentSettingType(position);
}

std::u16string PermissionInfoBarDelegate::GetCompactMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_NOTIFICATION_QUIET_PERMISSION_MINI_INFOBAR_MESSAGE);
}

std::u16string PermissionInfoBarDelegate::GetCompactLinkText() const {
  switch (QuietNotificationPermissionUiConfig::GetMiniInfobarExpandLinkText()) {
    case QuietNotificationPermissionUiConfig::InfobarLinkTextVariation::kManage:
      return l10n_util::GetStringUTF16(IDS_NOTIFICATION_BUTTON_MANAGE);
    case QuietNotificationPermissionUiConfig::InfobarLinkTextVariation::
        kDetails:
      return l10n_util::GetStringUTF16(
          IDS_NOTIFICATION_QUIET_PERMISSION_MINI_INFOBAR_DETAILS_LINK);
  }

  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

// TODO(crbug.com/40131069): Many methods of this class switches on the quiet UI
// reason. Refactor this into separate subclasses instead.
std::u16string PermissionInfoBarDelegate::GetDescriptionText() const {
  return prompt_model_.description;
}

bool PermissionInfoBarDelegate::ShouldSecondaryButtonOpenSettings() const {
  return prompt_model_.secondary_button_behavior ==
         SecondaryButtonBehavior::kShowSettings;
}

int PermissionInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_NOTIFICATIONS_OFF;
}

std::u16string PermissionInfoBarDelegate::GetLinkText() const {
  // This will be used as the text of the link in the expanded state.
  return prompt_model_.learn_more_text;
}

GURL PermissionInfoBarDelegate::GetLinkURL() const {
  return GetNotificationBlockedLearnMoreUrl();
}

bool PermissionInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
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

void PermissionInfoBarDelegate::InfoBarDismissed() {
  if (permission_prompt_)
    permission_prompt_->Closing();
}

std::u16string PermissionInfoBarDelegate::GetMessageText() const {
  return prompt_model_.title;
}

bool PermissionInfoBarDelegate::Accept() {
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

bool PermissionInfoBarDelegate::Cancel() {
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
  NOTREACHED_IN_MIGRATION();
  return true;
}

PermissionInfoBarDelegate::PermissionInfoBarDelegate(
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
  prompt_model_ = GetQuietPermissionPromptModel(
      *quiet_ui_reason, ContentSettingsType::NOTIFICATIONS);
}

infobars::InfoBarDelegate::InfoBarIdentifier
PermissionInfoBarDelegate::GetIdentifier() const {
  return PERMISSION_INFOBAR_DELEGATE_ANDROID;
}

int PermissionInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::u16string PermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return (button == BUTTON_OK) ? prompt_model_.primary_button_label
                               : prompt_model_.secondary_button_label;
}

bool PermissionInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  // The PermissionRequestManager doesn't create duplicate infobars so a pointer
  // equality check is sufficient.
  return this == delegate;
}
