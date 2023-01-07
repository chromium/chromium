// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_REQUESTER_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_REQUESTER_ANDROID_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/android/window_android.h"

namespace content {
class WebContents;
}

using PermissionUpdatedCallback = base::OnceCallback<void(bool)>;

// The native-side counterpart to
// org.chromium.chrome.browser.permissions.PermissionUpdateRequester to
// triggers the Android runtime permission prompt UI to request missing Chrome
// app-level permission(s) after the user expressed interest in either
// the permission update infobar/message.
class PermissionUpdateRequester {
 public:
  // |callback| will be called when permission is granted or declined.
  // It is safe to delete `this` instance from the callback.
  PermissionUpdateRequester(
      content::WebContents* web_contents,
      const std::vector<std::string>& required_android_permissions,
      const std::vector<std::string>& optional_android_permissions,
      base::OnceCallback<void(bool)> callback);
  ~PermissionUpdateRequester();
  PermissionUpdateRequester(const PermissionUpdateRequester&) = delete;
  PermissionUpdateRequester& operator=(const PermissionUpdateRequester&) =
      delete;

  void OnPermissionResult(JNIEnv* env, jboolean all_permissions_granted);

  void RequestPermissions();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  std::vector<ContentSettingsType> content_settings_types_;
  base::OnceCallback<void(bool)> callback_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_REQUESTER_ANDROID_H_
