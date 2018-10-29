// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_PERMISSION_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_PERMISSION_HELPER_H_

#include <memory>
#include "base/android/java_handler_thread.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content {
class WebContents;
}  // namespace content

namespace device {

class ArCorePermissionHelper {
 public:
  ArCorePermissionHelper();
  virtual ~ArCorePermissionHelper();

  virtual void RequestCameraPermission(int render_process_id,
                                       int render_frame_id,
                                       bool has_user_activation,
                                       base::OnceCallback<void(bool)> callback);

  virtual void OnRequestCameraPermissionResult(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> callback,
      ContentSetting content_setting);

  virtual void OnRequestAndroidCameraPermissionResult(
      base::OnceCallback<void(bool)> callback,
      bool was_android_camera_permission_granted);

 private:
  base::WeakPtr<ArCorePermissionHelper> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Must be last.
  base::WeakPtrFactory<ArCorePermissionHelper> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArCorePermissionHelper);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_PERMISSION_HELPER_H_
