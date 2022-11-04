// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* PermissionUpdateInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    const std::vector<ContentSettingsType>& filtered_content_settings_types,
    const std::vector<std::string>& required_permissions,
    const std::vector<std::string>& optional_permissions,
    PermissionUpdatedCallback callback) {
  DCHECK_EQ(permissions::ShouldRepromptUserForPermissions(
                web_contents, content_settings_types),
            permissions::PermissionRepromptState::kShow)
      << "Caller should check ShouldRepromptUserForPermissions before creating "
         "the infobar.";

  const int message_id =
      GetPermissionUpdateUiTitleId(filtered_content_settings_types);
  return PermissionUpdateInfoBarDelegate::Create(
      web_contents, required_permissions, optional_permissions,
      content_settings_types, message_id, std::move(callback));
}

// static
infobars::InfoBar* PermissionUpdateInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const std::vector<std::string>& required_android_permissions,
    int permission_msg_id,
    PermissionUpdatedCallback callback) {
  std::vector<ContentSettingsType> empty_content_settings_types;
  std::vector<std::string> empty_optional_android_permissions;
  return PermissionUpdateInfoBarDelegate::Create(
      web_contents, required_android_permissions,
      empty_optional_android_permissions, empty_content_settings_types,
      permission_msg_id, std::move(callback));
}

void PermissionUpdateInfoBarDelegate::OnPermissionResult(
    bool all_permissions_granted) {
  permissions::PermissionUmaUtil::RecordMissingPermissionInfobarAction(
      permissions::PermissionAction::GRANTED, content_settings_types_);
  std::move(callback_).Run(all_permissions_granted);
  infobar()->RemoveSelf();
}

// static
infobars::InfoBar* PermissionUpdateInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const std::vector<std::string>& required_android_permissions,
    const std::vector<std::string>& optional_android_permissions,
    const std::vector<ContentSettingsType> content_settings_types,
    int permission_msg_id,
    PermissionUpdatedCallback callback) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  if (!infobar_manager) {
    std::move(callback).Run(false);
    return nullptr;
  }

  return infobar_manager->AddInfoBar(std::make_unique<infobars::ConfirmInfoBar>(
      // Using WrapUnique as the PermissionUpdateInfoBarDelegate ctor is
      // private.
      base::WrapUnique<ConfirmInfoBarDelegate>(
          new PermissionUpdateInfoBarDelegate(
              web_contents, required_android_permissions,
              optional_android_permissions, content_settings_types,
              permission_msg_id, std::move(callback)))));
}

// static
int PermissionUpdateInfoBarDelegate::GetPermissionUpdateUiTitleId(
    const std::vector<ContentSettingsType>& content_settings_types) {
  // Decided which title to return.
  int message_id = -1;
  for (ContentSettingsType content_settings_type : content_settings_types) {
    switch (message_id) {
      case -1:
        if (content_settings_type == ContentSettingsType::GEOLOCATION) {
          message_id = IDS_INFOBAR_MISSING_LOCATION_PERMISSION_TEXT;
        } else if (content_settings_type ==
                   ContentSettingsType::MEDIASTREAM_MIC) {
          message_id = IDS_INFOBAR_MISSING_MICROPHONE_PERMISSION_TEXT;
        } else if (content_settings_type ==
                   ContentSettingsType::MEDIASTREAM_CAMERA) {
          message_id = IDS_INFOBAR_MISSING_CAMERA_PERMISSION_TEXT;
        } else if (content_settings_type == ContentSettingsType::AR) {
          message_id = IDS_INFOBAR_MISSING_AR_CAMERA_PERMISSION_TEXT;
        } else {
          NOTREACHED();
        }
        break;
      case IDS_INFOBAR_MISSING_CAMERA_PERMISSION_TEXT:
        DCHECK(content_settings_type == ContentSettingsType::MEDIASTREAM_MIC);
        return IDS_INFOBAR_MISSING_MICROPHONE_CAMERA_PERMISSIONS_TEXT;
      case IDS_INFOBAR_MISSING_MICROPHONE_PERMISSION_TEXT:
        DCHECK(content_settings_type ==
               ContentSettingsType::MEDIASTREAM_CAMERA);
        return IDS_INFOBAR_MISSING_MICROPHONE_CAMERA_PERMISSIONS_TEXT;
      default:
        NOTREACHED();
        break;
    }
  }
  return message_id;
}

PermissionUpdateInfoBarDelegate::PermissionUpdateInfoBarDelegate(
    content::WebContents* web_contents,
    const std::vector<std::string>& required_android_permissions,
    const std::vector<std::string>& optional_android_permissions,
    const std::vector<ContentSettingsType>& content_settings_types,
    int permission_msg_id,
    PermissionUpdatedCallback callback)
    : ConfirmInfoBarDelegate(),
      content_settings_types_(content_settings_types),
      permission_msg_id_(permission_msg_id),
      callback_(std::move(callback)) {
  permission_update_requester_ = std::make_unique<PermissionUpdateRequester>(
      web_contents, required_android_permissions, optional_android_permissions,
      base::BindOnce(&PermissionUpdateInfoBarDelegate::OnPermissionResult,
                     base::Unretained(this)));
}

PermissionUpdateInfoBarDelegate::~PermissionUpdateInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
PermissionUpdateInfoBarDelegate::GetIdentifier() const {
  return PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID;
}

int PermissionUpdateInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_WARNING;
}

std::u16string PermissionUpdateInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(permission_msg_id_);
}

int PermissionUpdateInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string PermissionUpdateInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(IDS_INFOBAR_UPDATE_PERMISSIONS_BUTTON_TEXT);
}

bool PermissionUpdateInfoBarDelegate::Accept() {
  permission_update_requester_->RequestPermissions();
  return false;
}

bool PermissionUpdateInfoBarDelegate::Cancel() {
  std::move(callback_).Run(false);
  permissions::PermissionUmaUtil::RecordMissingPermissionInfobarAction(
      permissions::PermissionAction::DENIED, content_settings_types_);
  return true;
}

void PermissionUpdateInfoBarDelegate::InfoBarDismissed() {
  permissions::PermissionUmaUtil::RecordMissingPermissionInfobarAction(
      permissions::PermissionAction::DISMISSED, content_settings_types_);
  std::move(callback_).Run(false);
}
