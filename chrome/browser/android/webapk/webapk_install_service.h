// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace webapps {
struct ShortcutInfo;
enum class WebApkInstallResult;
enum class WebApkUpdateReason;
}

class SkBitmap;

// Service which talks to Chrome WebAPK server and Google Play to generate a
// WebAPK on the server, download it, and install it.
class WebApkInstallService : public KeyedService {
 public:
  // Called when the creation/updating of a WebAPK is finished or failed.
  // Parameters:
  // - the result of the installation.
  // - serialized proto for the installation, if exist.
  // - true if Chrome received a "request updates less frequently" directive.
  //   from the WebAPK server.
  // - the package name of the WebAPK.
  using FinishCallback = base::OnceCallback<void(webapps::WebApkInstallResult,
                                                 std::unique_ptr<std::string>,
                                                 bool,
                                                 const std::string&)>;

  // Called when the installation of a WebAPK that was scheduled by the
  // WebApkInstallCoordinatorService finished or failed to pass the result back
  // to the WebapkInstallCoordinatorBridge which is passing it along to the
  // connecting client.
  using ServiceInstallFinishCallback =
      base::OnceCallback<void(webapps::WebApkInstallResult)>;

  static WebApkInstallService* Get(content::BrowserContext* browser_context);

  explicit WebApkInstallService(content::BrowserContext* browser_context);

  WebApkInstallService(const WebApkInstallService&) = delete;
  WebApkInstallService& operator=(const WebApkInstallService&) = delete;

  ~WebApkInstallService() override;

  // Returns whether an install for |web_manifest_url| is in progress.
  bool IsInstallInProgress(const GURL& web_manifest_url,
                           const GURL& manifest_id);

  // Installs WebAPK and adds shortcut to the launcher. It talks to the Chrome
  // WebAPK server to generate a WebAPK on the server and to Google Play to
  // install the downloaded WebAPK.
  void InstallAsync(content::WebContents* web_contents,
                    const webapps::ShortcutInfo& shortcut_info,
                    const SkBitmap& primary_icon,
                    bool is_primary_icon_maskable,
                    webapps::WebappInstallSource install_source);

  // This function is used if the install is scheduled in the
  // WebApkInstallCoordinatorService service. Installs WebAPKs based on a
  // serialized_web_apk it receives from the client. It
  // talks to the Chrome WebAPK server to generate a WebAPK on the server and to
  // Google Play to install the downloaded WebAPK. It calls the
  // |finish_callback| with the result of the installation to propagate the
  // result to the connecting client.
  void InstallForServiceAsync(std::unique_ptr<std::string> serialized_web_apk,
                              const SkBitmap& primary_icon,
                              bool is_primary_icon_maskable,
                              ServiceInstallFinishCallback finish_callback);

  // Talks to the Chrome WebAPK server to update a WebAPK on the server and to
  // the Google Play server to install the downloaded WebAPK.
  // |update_request_path| is the path of the file with the update request.
  // Calls |finish_callback| once the update completed or failed.
  void UpdateAsync(const base::FilePath& update_request_path,
                   FinishCallback finish_callback);

 private:
  // Called once the install/update completed or failed.
  void OnFinishedInstall(base::WeakPtr<content::WebContents> web_contents,
                         const webapps::ShortcutInfo& shortcut_info,
                         const SkBitmap& primary_icon,
                         bool is_priamry_icon_maskable,
                         webapps::WebApkInstallResult result,
                         std::unique_ptr<std::string> serialized_webapk,
                         bool relax_updates,
                         const std::string& webapk_package_name);

  // Called once the install scheduled from the service completed or failed.
  // Triggers the callback to propagate the |WebApkInstallResult| to the
  // scheduling Client.
  void OnFinishedInstallForService(
      const GURL& manifest_url,
      const GURL& manifest_id,
      const GURL& url,
      const std::u16string& short_name,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable,
      ServiceInstallFinishCallback done_callback,
      webapps::WebApkInstallResult result,
      std::unique_ptr<std::string> serialized_webapk,
      bool relax_updates,
      const std::string& webapk_package_name);

  // Removes current notifications about an ongoing install and adds a
  // installed-notification if the installation was successful.
  void HandleFinishInstallNotifications(
      const GURL& manifest_url,
      const GURL& url,
      const std::u16string& short_name,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable,
      webapps::WebApkInstallResult result,
      std::unique_ptr<std::string> serialized_webapk,
      const std::string& webapk_package_name);

  // Shows a notification that an install is in progress.
  static void ShowInstallInProgressNotification(
      const GURL& manifest_url,
      const std::u16string& short_name,
      const GURL& url,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable);

  // Shows a notification that an install is completed.
  static void ShowInstalledNotification(const GURL& manifest_url,
                                        const std::u16string& short_name,
                                        const GURL& url,
                                        const SkBitmap& primary_icon,
                                        bool is_primary_icon_maskable,
                                        const std::string& package_name);

  // Shows a notification that an install is failed.
  static void ShowInstallFailedNotification(
      const GURL& manifest_url,
      const std::u16string& short_name,
      const GURL& url,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable,
      webapps::WebApkInstallResult result,
      std::unique_ptr<std::string> serialized_webapk);

  raw_ptr<content::BrowserContext> browser_context_;

  // In progress installs.
  std::set<GURL> installs_;

  // In progress installs's id.
  std::set<GURL> install_ids_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstallService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_
