// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_MESSAGE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_MESSAGE_DELEGATE_ANDROID_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/permissions/permission_update_requester_android.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"

namespace content {
class WebContents;
}

// A message delegate to be used for requesting missing Android runtime
// permissions for previously allowed ContentSettingsTypes.
// Message UI is an alternative ui to infobars, which are implemented by
// |PermissionUpdateInfoBarDelegate|.
class PermissionUpdateMessageDelegate {
 public:
  // `delete_callback_` will be invoked by `this` when the message has been
  // concluded (regardless of outcome). It is safe for the caller to delete
  // `this` from the callback.
  PermissionUpdateMessageDelegate(
      content::WebContents* web_contents_,
      const std::vector<std::string> required_android_permissions,
      const std::vector<std::string> optional_android_permissions,
      const std::vector<ContentSettingsType> content_settings_types,
      int icon_id,
      int title_id,
      int description_id,
      PermissionUpdatedCallback callback,
      base::OnceCallback<void(PermissionUpdateMessageDelegate*)>
          delete_callback_);
  ~PermissionUpdateMessageDelegate();

  void OnPermissionResult(bool all_permissions_granted);
  int GetTitleId();
  void AttachAdditionalCallback(PermissionUpdatedCallback callback);

 private:
  friend class PermissionUpdateMessageControllerAndroidTest;

  void HandlePrimaryActionCallback();
  void HandleDismissCallback(messages::DismissReason dismiss_reason);
  void DismissInternal();
  void RunCallbacks(bool all_permissions_granted);

  std::vector<ContentSettingsType> content_settings_types_;
  std::vector<PermissionUpdatedCallback> callbacks_;
  base::OnceCallback<void(PermissionUpdateMessageDelegate*)> delete_callback_;
  std::unique_ptr<PermissionUpdateRequester> permission_update_requester_;
  std::unique_ptr<messages::MessageWrapper> message_;
  int title_id_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_MESSAGE_DELEGATE_ANDROID_H_
