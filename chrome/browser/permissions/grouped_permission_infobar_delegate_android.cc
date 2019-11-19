// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/adaptive_notification_permission_ui_selector.h"
#include "chrome/browser/permissions/permission_features.h"
#include "chrome/browser/permissions/permission_prompt_android.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/android/infobars/grouped_permission_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

GroupedPermissionInfoBarDelegate::~GroupedPermissionInfoBarDelegate() {
  PermissionUmaUtil::RecordInfobarDetailsExpanded(details_expanded_);
}

// static
infobars::InfoBar* GroupedPermissionInfoBarDelegate::Create(
    const base::WeakPtr<PermissionPromptAndroid>& permission_prompt,
    InfoBarService* infobar_service) {
  // WrapUnique needs to be used because the constructor is private.
  return infobar_service->AddInfoBar(std::make_unique<GroupedPermissionInfoBar>(
      base::WrapUnique(new GroupedPermissionInfoBarDelegate(permission_prompt,
                                                            infobar_service))));
}

size_t GroupedPermissionInfoBarDelegate::PermissionCount() const {
  return permission_prompt_->PermissionCount();
}

ContentSettingsType GroupedPermissionInfoBarDelegate::GetContentSettingType(
    size_t position) const {
  return permission_prompt_->GetContentSettingType(position);
}

base::string16 GroupedPermissionInfoBarDelegate::GetCompactMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_NOTIFICATION_QUIET_PERMISSION_MINI_INFOBAR_MESSAGE);
}

base::string16 GroupedPermissionInfoBarDelegate::GetCompactLinkText() const {
  return l10n_util::GetStringUTF16(
      IDS_NOTIFICATION_QUIET_PERMISSION_MINI_INFOBAR_DETAILS_LINK);
}

base::string16 GroupedPermissionInfoBarDelegate::GetDescriptionText() const {
  return l10n_util::GetStringUTF16(
      IDS_NOTIFICATION_QUIET_PERMISSION_PROMPT_MESSAGE);
}

int GroupedPermissionInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_NOTIFICATIONS_OFF;
}

base::string16 GroupedPermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_NOTIFICATION_QUIET_PERMISSION_INFOBAR_TITLE);
}

bool GroupedPermissionInfoBarDelegate::Accept() {
  if (permission_prompt_)
    permission_prompt_->Accept();
  return true;
}

bool GroupedPermissionInfoBarDelegate::Cancel() {
  // The infobar needs to be kept open after the "Manage" button is clicked.
  return false;
}

void GroupedPermissionInfoBarDelegate::InfoBarDismissed() {
  if (permission_prompt_)
    permission_prompt_->Closing();
}

bool GroupedPermissionInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  details_expanded_ = true;
  return false;
}

// static
bool GroupedPermissionInfoBarDelegate::ShouldShowMiniInfobar(
    Profile* profile,
    ContentSettingsType type) {
  auto* permission_ui_selector =
      AdaptiveNotificationPermissionUiSelector::GetForProfile(profile);
  return type == ContentSettingsType::NOTIFICATIONS &&
         permission_ui_selector->ShouldShowQuietUi() &&
         QuietNotificationsPromptConfig::UIFlavorToUse() ==
             QuietNotificationsPromptConfig::UIFlavor::MINI_INFOBAR;
}

GroupedPermissionInfoBarDelegate::GroupedPermissionInfoBarDelegate(
    const base::WeakPtr<PermissionPromptAndroid>& permission_prompt,
    InfoBarService* infobar_service)
    : permission_prompt_(permission_prompt),
      infobar_service_(infobar_service),
      details_expanded_(false) {
  DCHECK(permission_prompt_);
  DCHECK(infobar_service_);

  // Infobars are only used for NOTIFICATIONS right now, therefore strings can
  // be hardcoded for that type.
  DCHECK_EQ(permission_prompt_->GetContentSettingType(0u),
            ContentSettingsType::NOTIFICATIONS);
}

infobars::InfoBarDelegate::InfoBarIdentifier
GroupedPermissionInfoBarDelegate::GetIdentifier() const {
  return GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID;
}

int GroupedPermissionInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 GroupedPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK)
          ? IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON
          : IDS_NOTIFICATION_BUTTON_MANAGE);
}

bool GroupedPermissionInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  // The PermissionRequestManager doesn't create duplicate infobars so a pointer
  // equality check is sufficient.
  return this == delegate;
}
