// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_COORDINATOR_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_COORDINATOR_BRIDGE_H_

#include <memory>
#include <string>
#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "components/webapps/browser/android/shortcut_info.h"

enum class WebApkInstallResult;
class SkBitmap;

namespace webapps {

enum class WebappInstallSource;

// This class is owned by the Java WebApkInstallCoordinatorBridge object.
// The Java side is responsible for cleaning up this object.
class WebApkInstallCoordinatorBridge {
 public:
  WebApkInstallCoordinatorBridge(JNIEnv* env,
                                 const base::android::JavaRef<jobject>& obj);
  ~WebApkInstallCoordinatorBridge();

  WebApkInstallCoordinatorBridge(const WebApkInstallCoordinatorBridge&) =
      delete;
  WebApkInstallCoordinatorBridge& operator=(
      const WebApkInstallCoordinatorBridge&) = delete;

  void Install(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               const base::android::JavaParamRef<jbyteArray>& java_web_apk,
               const base::android::JavaParamRef<jobject>& java_primary_icon,
               const jboolean is_primary_icon_maskable);

  void InstallOnUiThread(
      std::unique_ptr<std::string> serialized_proto,
      const SkBitmap& primary_icon,
      const bool is_primary_icon_maskable,
      WebApkInstallService::ServiceInstallFinishCallback finish_callback);

  void OnFinishedApkInstall(const WebApkInstallResult result);

  void Destroy(JNIEnv* env);

 private:
  JavaObjectWeakGlobalRef java_ref_;

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstallCoordinatorBridge> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_COORDINATOR_BRIDGE_H_
