// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_

#include <jni.h>
#include <map>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace base {
class ElapsedTimer;
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace network {
class SimpleURLLoader;
}

// The enum values are persisted to logs |WebApkInstallSpaceStatus| in
// enums.xml, therefore they should never be reused nor renumbered.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.webapps
enum class SpaceStatus {
  ENOUGH_SPACE = 0,
  ENOUGH_SPACE_AFTER_FREE_UP_CACHE = 1,
  NOT_ENOUGH_SPACE = 2,
  COUNT = 3,
};

// Talks to Chrome WebAPK server to download metadata about a WebAPK and issue
// a request for it to be installed. The native WebApkInstaller owns the Java
// WebApkInstaller counterpart.
class WebApkInstaller {
 public:
  using FinishCallback = WebApkInstallService::FinishCallback;

  virtual ~WebApkInstaller();

  // Creates a self-owned WebApkInstaller instance and talks to the Chrome
  // WebAPK server to generate a WebAPK on the server and locally requests the
  // APK to be installed. Calls |callback| once the install completed or failed.
  static void InstallAsync(content::BrowserContext* context,
                           const ShortcutInfo& shortcut_info,
                           const SkBitmap& primary_icon,
                           bool is_primary_icon_maskable,
                           const SkBitmap& badge_icon,
                           FinishCallback finish_callback);

  // Creates a self-owned WebApkInstaller instance and talks to the Chrome
  // WebAPK server to update a WebAPK on the server and locally requests the
  // APK to be installed. Calls |callback| once the install completed or failed.
  // |update_request_path| is the path of the file with the update request.
  static void UpdateAsync(content::BrowserContext* context,
                          const base::FilePath& update_request_path,
                          FinishCallback callback);

  // Calls the private function |InstallAsync| for testing.
  // Should be used only for testing.
  static void InstallAsyncForTesting(WebApkInstaller* installer,
                                     const ShortcutInfo& shortcut_info,
                                     const SkBitmap& primary_icon,
                                     bool is_primary_icon_maskable,
                                     const SkBitmap& badge_icon,
                                     FinishCallback callback);

  // Calls the private function |UpdateAsync| for testing.
  // Should be used only for testing.
  static void UpdateAsyncForTesting(WebApkInstaller* installer,
                                    const base::FilePath& update_request_path,
                                    FinishCallback callback);

  // Sets the timeout for the server requests.
  void SetTimeoutMs(int timeout_ms);

  // Called once the installation is complete or failed.
  void OnInstallFinished(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint result);

  // Checks if there is enough space to install a WebAPK.
  // If yes, continue the WebAPK installation process. If there is not enough
  // space to install (even after clearing Chrome's cache), fails the
  // installation process immediately.
  void OnGotSpaceStatus(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint status);

  // Asynchronously builds the WebAPK proto on a background thread for an update
  // or install request. Runs |callback| on the calling thread when complete.
  static void BuildProto(
      const ShortcutInfo& shortcut_info,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable,

      const SkBitmap& badge_icon,
      const std::string& package_name,
      const std::string& version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      base::OnceCallback<void(std::unique_ptr<std::string>)> callback);

  // Builds the WebAPK proto for an update or an install request and stores it
  // to |update_request_path|. Runs |callback| with a boolean indicating
  // whether the proto was successfully written to disk.
  static void StoreUpdateRequestToFile(
      const base::FilePath& update_request_path,
      const ShortcutInfo& shortcut_info,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable,
      const SkBitmap& badge_icon,
      const std::string& package_name,
      const std::string& version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      WebApkUpdateReason update_reason,
      base::OnceCallback<void(bool)> callback);

 protected:
  explicit WebApkInstaller(content::BrowserContext* browser_context);

  // Called when the package name of the WebAPK is available and the install
  // or update request should be issued.
  virtual void InstallOrUpdateWebApk(const std::string& package_name,
                                     const std::string& token);

  // Checks if there is enough space to install a WebAPK.
  virtual void CheckFreeSpace();

  // Called when the install or update process has completed or failed.
  void OnResult(WebApkInstallResult result);

 private:
  enum TaskType {
    UNDEFINED,
    INSTALL,
    UPDATE,
  };

  // Create the Java object.
  void CreateJavaRef();

  // Talks to the Chrome WebAPK server to generate a WebAPK on the server and to
  // Google Play to install the downloaded WebAPK. Calls |callback| once the
  // install completed or failed.
  void InstallAsync(const ShortcutInfo& shortcut_info,
                    const SkBitmap& primary_icon,
                    bool is_primary_icon_maskable,
                    const SkBitmap& badge_icon,
                    FinishCallback finish_callback);

  // Talks to the Chrome WebAPK server to update a WebAPK on the server and to
  // the Google Play server to install the downloaded WebAPK.
  // |update_request_path| is the path of the file with the update request.
  // Calls |finish_callback| once the update completed or failed.
  void UpdateAsync(const base::FilePath& update_request_path,
                   FinishCallback finish_callback);

  // Called once there is sufficient space on the user's device to install a
  // WebAPK. The user may already have had sufficient space on their device
  // prior to initiating the install process. This method might be called as a
  // result of freeing up memory by clearing Chrome's cache.
  void OnHaveSufficientSpaceForInstall();

  // Called with the contents of the update request file.
  void OnReadUpdateRequest(std::unique_ptr<std::string> update_request);

  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // Called with the computed Murmur2 hash for the primary icon.
  void OnGotPrimaryIconMurmur2Hash(const std::string& primary_icon_hash);

  // Called with the computed Murmur2 hash for the badge icon, and
  // |did_fetch_badge_icon| to indicate whether there was an attempt to fetch
  // badge icon.
  void OnGotBadgeIconMurmur2Hash(bool did_fetch_badge_icon,
                                 const std::string& primary_icon_hash,
                                 const std::string& badge_icon_hash);

  // Sends a request to WebAPK server to create/update WebAPK. During a
  // successful request the WebAPK server responds with a token to send to
  // Google Play.
  void SendRequest(std::unique_ptr<std::string> serialized_proto);

  content::BrowserContext* browser_context_;

  // Sends HTTP request to WebAPK server.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Fails WebApkInstaller if WebAPK server takes too long to respond or if the
  // download takes too long.
  base::OneShotTimer timer_;

  // Tracks how long it takes to install a WebAPK.
  std::unique_ptr<base::ElapsedTimer> install_duration_timer_;

  // Callback to call once WebApkInstaller succeeds or fails.
  FinishCallback finish_callback_;

  // Data for installs.
  std::unique_ptr<ShortcutInfo> install_shortcut_info_;
  SkBitmap install_primary_icon_;
  SkBitmap install_badge_icon_;

  bool is_primary_icon_maskable_;

  base::string16 short_name_;

  // WebAPK server URL.
  GURL server_url_;

  // The number of milliseconds to wait for the WebAPK server to respond.
  int webapk_server_timeout_ms_;

  // WebAPK package name.
  std::string webapk_package_;

  // WebAPK version code.
  int webapk_version_;

  // Whether the server wants the WebAPK to request updates less frequently.
  bool relax_updates_;

  // Indicates whether the installer is for installing or updating a WebAPK.
  TaskType task_type_;

  // Points to the Java Object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstaller> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebApkInstaller);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
