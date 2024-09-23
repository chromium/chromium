// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_

#include <jni.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace base {
class ElapsedTimer;
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace webapps {
enum class WebApkInstallResult;
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

  WebApkInstaller(const WebApkInstaller&) = delete;
  WebApkInstaller& operator=(const WebApkInstaller&) = delete;

  virtual ~WebApkInstaller();

  // Creates a self-owned WebApkInstaller instance and talks to the Chrome
  // WebAPK server to generate a WebAPK on the server and locally requests the
  // APK to be installed. Calls |callback| once the install completed or failed.
  static void InstallAsync(content::BrowserContext* context,
                           content::WebContents* web_contents,
                           const webapps::ShortcutInfo& shortcut_info,
                           const SkBitmap& primary_icon,
                           webapps::WebappInstallSource install_source,
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
  static void InstallAsyncForTesting(
      WebApkInstaller* installer,
      content::WebContents* web_contents,
      const webapps::ShortcutInfo& shortcut_info,
      const SkBitmap& primary_icon,
      webapps::WebappInstallSource install_source,
      FinishCallback callback);

  // Calls the private function |UpdateAsync| for testing.
  // Should be used only for testing.
  static void UpdateAsyncForTesting(WebApkInstaller* installer,
                                    const base::FilePath& update_request_path,
                                    FinishCallback callback);

  // Sets the timeout for the server requests.
  void SetTimeoutMs(int timeout_ms);

  // Called once the installation is complete or failed.
  void OnInstallFinished(JNIEnv* env, jint result);

  // Checks if there is enough space to install a WebAPK.
  // If yes, continue the WebAPK installation process. If there is not enough
  // space to install (even after clearing Chrome's cache), fails the
  // installation process immediately.
  void OnGotSpaceStatus(JNIEnv* env, jint status);

  // Builds the WebAPK proto for an update or an install request and stores it
  // to |update_request_path|. Runs |callback| with a boolean indicating
  // whether the proto was successfully written to disk.
  static void StoreUpdateRequestToFile(
      const base::FilePath& update_request_path,
      const webapps::ShortcutInfo& shortcut_info,
      const GURL& app_key,
      std::unique_ptr<webapps::WebappIcon> primary_icon,
      std::unique_ptr<webapps::WebappIcon> splash_icon,
      const std::string& package_name,
      const std::string& version,
      std::map<GURL, std::unique_ptr<webapps::WebappIcon>> icons,
      bool is_manifest_stale,
      bool is_app_identity_update_supported,
      std::vector<webapps::WebApkUpdateReason> update_reasons,
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
  void OnResult(webapps::WebApkInstallResult result);

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
  void InstallAsync(content::WebContents* web_contents,
                    const webapps::ShortcutInfo& shortcut_info,
                    const SkBitmap& primary_icon,
                    webapps::WebappInstallSource install_source,
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

  // Called with the computed Murmur2 hash for the icons.
  void OnGotIconMurmur2Hashes(
      std::map<GURL, std::unique_ptr<webapps::WebappIcon>> icons);

  // Called with the serialized proto for the WebAPK install.
  void OnInstallProtoBuilt(std::unique_ptr<std::string> serialized_proto);

  // Sends a request to WebAPK server to create/update WebAPK. During a
  // successful request the WebAPK server responds with a token to send to
  // Google Play.
  void SendRequest(const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const std::string& serialized_proto);

  // Returns the WebAPK server URL based on the command line.
  GURL GetServerUrl();

  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtr<content::WebContents> web_contents_;

  // Sends HTTP request to WebAPK server.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Fails WebApkInstaller if WebAPK server takes too long to respond or if the
  // download takes too long.
  base::OneShotTimer timer_;

  // Tracks how long it takes to install a WebAPK.
  std::unique_ptr<base::ElapsedTimer> install_duration_timer_;

  // Callback to call once WebApkInstaller succeeds or fails.
  FinishCallback finish_callback_;

  // Helper for downloading WebAPK icons and compute Murmur2 hash of the
  // downloaded images.
  std::unique_ptr<webapps::WebApkIconsHasher> icon_hasher_;

  // Data for installs.

  // Only available if the install was scheduled directly in chrome and not in
  // the WebApkInstallCoordinatorService.
  std::unique_ptr<webapps::ShortcutInfo> install_shortcut_info_;

  SkBitmap install_primary_icon_;

  std::u16string short_name_;

  // WebAPK server URL.
  GURL server_url_;

  GURL manifest_id_;

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

  // Sources for triggering the WebAPK installs.
  webapps::WebappInstallSource install_source_;

  // Points to the Java Object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstaller> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
