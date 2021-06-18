// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_array.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/PermissionUpdateInfoBarDelegate_jni.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/permission_uma_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;

// static
infobars::InfoBar* PermissionUpdateInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    PermissionUpdatedCallback callback) {
  DCHECK_EQ(permissions::ShouldRepromptUserForPermissions(
                web_contents, content_settings_types),
            permissions::PermissionRepromptState::kShow)
      << "Caller should check ShouldShowPermissionInfobar before creating the "
      << "infobar.";

  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();

  std::vector<std::string> required_permissions;
  std::vector<std::string> optional_permissions;
  int message_id = -1;

  for (ContentSettingsType content_settings_type : content_settings_types) {
    if (permissions::HasRequiredAndroidPermissionsForContentSetting(
            window_android, content_settings_type)) {
      continue;
    }

    permissions::AppendRequiredAndroidPermissionsForContentSetting(
        content_settings_type, &required_permissions);
    permissions::AppendOptionalAndroidPermissionsForContentSetting(
        content_settings_type, &optional_permissions);

    if (message_id == -1) {
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
    } else if (message_id == IDS_INFOBAR_MISSING_CAMERA_PERMISSION_TEXT) {
      DCHECK(content_settings_type == ContentSettingsType::MEDIASTREAM_MIC);
      message_id = IDS_INFOBAR_MISSING_MICROPHONE_CAMERA_PERMISSIONS_TEXT;
    } else if (message_id == IDS_INFOBAR_MISSING_MICROPHONE_PERMISSION_TEXT) {
      DCHECK(content_settings_type == ContentSettingsType::MEDIASTREAM_CAMERA);
      message_id = IDS_INFOBAR_MISSING_MICROPHONE_CAMERA_PERMISSIONS_TEXT;
    } else {
      NOTREACHED();
    }
  }

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
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean all_permissions_granted) {
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
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(Java_PermissionUpdateInfoBarDelegate_create(
      env, reinterpret_cast<intptr_t>(this), web_contents->GetJavaWebContents(),
      base::android::ToJavaArrayOfStrings(env, required_android_permissions),
      base::android::ToJavaArrayOfStrings(env, optional_android_permissions)));
}

PermissionUpdateInfoBarDelegate::~PermissionUpdateInfoBarDelegate() {
  Java_PermissionUpdateInfoBarDelegate_onNativeDestroyed(
      base::android::AttachCurrentThread(), java_delegate_);
}

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
  Java_PermissionUpdateInfoBarDelegate_requestPermissions(
      base::android::AttachCurrentThread(), java_delegate_);
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
