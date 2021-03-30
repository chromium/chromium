// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}

// An infobar delegate to be used for requesting missing Android runtime
// permissions for previously allowed ContentSettingsTypes.
class PermissionUpdateInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using PermissionUpdatedCallback = base::OnceCallback<void(bool)>;

  // Creates an infobar to resolve conflicts in Android runtime permissions.
  // The necessary runtime permissions are generated based on the list of
  // ContentSettingsTypes passed in. Returns the infobar if it was successfully
  // added.
  //
  // This function can only be called with one of
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
      PermissionUpdatedCallback callback);

  // Creates an infobar to resolve conflicts in Android runtime permissions.
  // Returns the infobar if it was successfully added.
  //
  // The |callback| will not be triggered if this is deleted.
  static infobars::InfoBar* Create(
      content::WebContents* web_contents,
      const std::vector<std::string>& android_permissions,
      int permission_msg_id,
      PermissionUpdatedCallback callback);

  void OnPermissionResult(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jboolean all_permissions_granted);

 private:
  static infobars::InfoBar* Create(
      content::WebContents* web_contents,
      const std::vector<std::string>& android_permissions,
      const std::vector<ContentSettingsType> content_settings_types,
      int permission_msg_id,
      PermissionUpdatedCallback callback);

  PermissionUpdateInfoBarDelegate(
      content::WebContents* web_contents,
      const std::vector<std::string>& android_permissions,
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

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  std::vector<std::string> android_permissions_;
  std::vector<ContentSettingsType> content_settings_types_;
  int permission_msg_id_;
  PermissionUpdatedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PermissionUpdateInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_
