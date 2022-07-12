// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_update_message_controller_android.h"

#include <memory>
#include <utility>

#include "base/containers/cxx20_erase.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

void PermissionUpdateMessageController::ShowMessage(
    const std::vector<ContentSettingsType>& content_settings_types,
    PermissionUpdatedCallback callback) {
  DCHECK_EQ(permissions::ShouldRepromptUserForPermissions(
                &GetWebContents(), content_settings_types),
            permissions::PermissionRepromptState::kShow)
      << "Caller should check ShouldShowRepromptUserForPermissions before "
         "creating the message ui.";
  std::vector<std::string> required_permissions;
  std::vector<std::string> optional_permissions;
  ui::WindowAndroid* window_android =
      GetWebContents().GetNativeView()->GetWindowAndroid();
  const std::vector<ContentSettingsType> filtered_types =
      GetContentSettingsWithMissingRequiredAndroidPermissions(
          content_settings_types, window_android);
  AppendRequiredAndOptionalAndroidPermissionsForContentSettings(
      filtered_types, required_permissions, optional_permissions);
  std::tuple<int, int, int> res = GetPermissionUpdateUiResourcesId(
      filtered_types, required_permissions, optional_permissions);
  ShowMessageInternal(required_permissions, optional_permissions,
                      content_settings_types, std::get<0>(res),
                      std::get<1>(res), std::get<2>(res), std::move(callback));
}

void PermissionUpdateMessageController::ShowMessage(
    const std::vector<std::string>& required_android_permissions,
    int icon_id,
    int title_id,
    int description_id,
    PermissionUpdatedCallback callback) {
  std::vector<ContentSettingsType> empty_content_settings_types;
  std::vector<std::string> empty_optional_android_permissions;
  ShowMessageInternal(required_android_permissions,
                      empty_optional_android_permissions,
                      empty_content_settings_types, icon_id, title_id,
                      description_id, std::move(callback));
}

void PermissionUpdateMessageController::ShowMessageInternal(
    const std::vector<std::string>& required_android_permissions,
    const std::vector<std::string>& optional_android_permissions,
    const std::vector<ContentSettingsType> content_settings_types,
    int icon_id,
    int title_id,
    int description_id,
    PermissionUpdatedCallback callback) {
  // crbug.com/1319880: Some permission clients tend to request to re-prompt
  // more than expected.
  for (auto& delegate : message_delegates_) {
    if (delegate->GetTitleId() == title_id) {
      delegate->AttachAdditionalCallback(std::move(callback));
      return;
    }
  }
  auto delegate = std::make_unique<PermissionUpdateMessageDelegate>(
      &GetWebContents(), required_android_permissions,
      optional_android_permissions, content_settings_types, icon_id, title_id,
      description_id, std::move(callback),
      base::BindOnce(&PermissionUpdateMessageController::DeleteMessage,
                     base::Unretained(this)));
  message_delegates_.push_back(std::move(delegate));
}

void PermissionUpdateMessageController::DeleteMessage(
    PermissionUpdateMessageDelegate* delegate) {
  base::EraseIf(
      message_delegates_,
      [delegate](const std::unique_ptr<PermissionUpdateMessageDelegate>& d) {
        return delegate == d.get();
      });
}

std::tuple<int, int, int>
PermissionUpdateMessageController::GetPermissionUpdateUiResourcesId(
    const std::vector<ContentSettingsType>& content_settings_types,
    std::vector<std::string>& required_permissions,
    std::vector<std::string>& optional_permissions) {
  int message_id = -1;
  for (ContentSettingsType content_settings_type : content_settings_types) {
    switch (message_id) {
      case -1:
        if (content_settings_type == ContentSettingsType::GEOLOCATION) {
          message_id = IDS_MESSAGE_MISSING_LOCATION_PERMISSION_TEXT;
        } else if (content_settings_type ==
                   ContentSettingsType::MEDIASTREAM_MIC) {
          message_id = IDS_MESSAGE_MISSING_MICROPHONE_PERMISSION_TEXT;
        } else if (content_settings_type ==
                   ContentSettingsType::MEDIASTREAM_CAMERA) {
          message_id = IDS_MESSAGE_MISSING_CAMERA_PERMISSION_TEXT;
        } else if (content_settings_type == ContentSettingsType::AR) {
          message_id = IDS_MESSAGE_MISSING_AR_CAMERA_PERMISSION_TEXT;
        } else {
          NOTREACHED();
        }
        break;
      case IDS_MESSAGE_MISSING_CAMERA_PERMISSION_TEXT:
        DCHECK(content_settings_type == ContentSettingsType::MEDIASTREAM_MIC);
        return std::make_tuple(
            IDR_ANDORID_MESSAGE_PERMISSION_VIDEOCAM,
            IDS_MESSAGE_MISSING_MICROPHONE_CAMERA_PERMISSION_TITLE,
            IDS_MESSAGE_MISSING_MICROPHONE_CAMERA_PERMISSIONS_TEXT);
      case IDS_MESSAGE_MISSING_MICROPHONE_PERMISSION_TEXT:
        DCHECK(content_settings_type ==
               ContentSettingsType::MEDIASTREAM_CAMERA);
        return std::make_tuple(
            IDR_ANDORID_MESSAGE_PERMISSION_VIDEOCAM,
            IDS_MESSAGE_MISSING_MICROPHONE_CAMERA_PERMISSION_TITLE,
            IDS_MESSAGE_MISSING_MICROPHONE_CAMERA_PERMISSIONS_TEXT);
      default:
        NOTREACHED();
        break;
    }
  }

  switch (message_id) {
    case IDS_MESSAGE_MISSING_LOCATION_PERMISSION_TEXT:
      return std::make_tuple(IDR_ANDROID_INFOBAR_GEOLOCATION,
                             IDS_MESSAGE_MISSING_LOCATION_PERMISSION_TITLE,
                             IDS_MESSAGE_MISSING_LOCATION_PERMISSION_TEXT);
    case IDS_MESSAGE_MISSING_MICROPHONE_PERMISSION_TEXT:
      return std::make_tuple(IDR_ANDROID_INFOBAR_MEDIA_STREAM_MIC,
                             IDS_MESSAGE_MISSING_MICROPHONE_PERMISSION_TITLE,
                             IDS_MESSAGE_MISSING_MICROPHONE_PERMISSION_TEXT);
    case IDS_MESSAGE_MISSING_CAMERA_PERMISSION_TEXT:
      return std::make_tuple(IDR_ANDORID_MESSAGE_PERMISSION_CAMERA,
                             IDS_MESSAGE_MISSING_CAMERA_PERMISSION_TITLE,
                             IDS_MESSAGE_MISSING_CAMERA_PERMISSION_TEXT);
    case IDS_MESSAGE_MISSING_AR_CAMERA_PERMISSION_TEXT:
      return std::make_tuple(IDR_ANDORID_MESSAGE_PERMISSION_CAMERA,
                             IDS_MESSAGE_MISSING_CAMERA_PERMISSION_TITLE,
                             IDS_MESSAGE_MISSING_AR_CAMERA_PERMISSION_TEXT);
    default:
      NOTREACHED();
      break;
  }
  return std::make_tuple(-1, -1, -1);
}

PermissionUpdateMessageController::PermissionUpdateMessageController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PermissionUpdateMessageController>(
          *web_contents) {}

PermissionUpdateMessageController::~PermissionUpdateMessageController() {
  message_delegates_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PermissionUpdateMessageController);
