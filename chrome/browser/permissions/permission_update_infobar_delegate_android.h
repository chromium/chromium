// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "chrome/browser/permissions/permission_update_requester_android.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}

// An infobar delegate to be used for requesting missing Android runtime
// permissions for previously allowed ContentSettingsTypes.
class PermissionUpdateInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an infobar to resolve conflicts in Android runtime permissions.
  // Returns the infobar if it was successfully added.
  //
  // This function can only be called with |content_settings_types| as follow:
  // ContentSettingsType::MEDIASTREAM_MIC,
  // ContentSettingsType::MEDIASTREAM_CAMERA,
  // ContentSettingsType::GEOLOCATION, or
  // ContentSettingsType::AR or with both
  // ContentSettingsType::MEDIASTREAM_MIC and
  // ContentSettingsType::MEDIASTREAM_CAMERA.
  //
  // The |callback| will not be triggered if this is deleted.
  static infobars::InfoBar* Create(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types,
      const std::vector<ContentSettingsType>& filtered_content_settings_types,
      const std::vector<std::string>& required_permissions,
      const std::vector<std::string>& optional_permissions,
      PermissionUpdatedCallback callback);

  // Creates an infobar to resolve conflicts in Android runtime permissions.
  // Returns the infobar if it was successfully added.
  //
  // The |callback| will not be triggered if this is deleted.
  static infobars::InfoBar* Create(
      content::WebContents* web_contents,
      const std::vector<std::string>& required_android_permissions,
      int permission_msg_id,
      PermissionUpdatedCallback callback);

  PermissionUpdateInfoBarDelegate(const PermissionUpdateInfoBarDelegate&) =
      delete;
  PermissionUpdateInfoBarDelegate& operator=(
      const PermissionUpdateInfoBarDelegate&) = delete;

  void OnPermissionResult(bool all_permissions_granted);

 private:
  static infobars::InfoBar* Create(
      content::WebContents* web_contents,
      const std::vector<std::string>& required_android_permissions,
      const std::vector<std::string>& optional_android_permissions,
      const std::vector<ContentSettingsType> content_settings_types,
      int permission_msg_id,
      PermissionUpdatedCallback callback);

  static int GetPermissionUpdateUiTitleId(
      const std::vector<ContentSettingsType>& content_settings_types);

  PermissionUpdateInfoBarDelegate(
      content::WebContents* web_contents,
      const std::vector<std::string>& required_android_permissions,
      const std::vector<std::string>& optional_android_permissions,
      const std::vector<ContentSettingsType>& content_settings_types,
      int permission_msg_id,
      PermissionUpdatedCallback callback);
  ~PermissionUpdateInfoBarDelegate() override;

  // InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;

  // PermissionInfoBarDelegate:
  int GetIconId() const override;
  std::u16string GetMessageText() const override;

  // ConfirmInfoBarDelegate:
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // InfoBarDelegate:
  void InfoBarDismissed() override;

  std::unique_ptr<PermissionUpdateRequester> permission_update_requester_;
  std::vector<ContentSettingsType> content_settings_types_;
  int permission_msg_id_;
  PermissionUpdatedCallback callback_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_
