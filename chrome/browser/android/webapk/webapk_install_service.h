// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class WebContents;
}

struct ShortcutInfo;
class SkBitmap;

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.webapps
enum class WebApkInstallResult {
  SUCCESS = 0,
  FAILURE = 1,
  // An install was initiated but it timed out. We did not get a response from
  // the install service so it is possible that the install will complete some
  // time in the future.
  PROBABLE_FAILURE = 2
};

// Service which talks to Chrome WebAPK server and Google Play to generate a
// WebAPK on the server, download it, and install it.
class WebApkInstallService : public KeyedService {
 public:
  // Called when the creation/updating of a WebAPK is finished or failed.
  // Parameters:
  // - the result of the installation.
  // - true if Chrome received a "request updates less frequently" directive.
  //   from the WebAPK server.
  // - the package name of the WebAPK.
  using FinishCallback =
      base::OnceCallback<void(WebApkInstallResult, bool, const std::string&)>;

  static WebApkInstallService* Get(content::BrowserContext* browser_context);

  explicit WebApkInstallService(content::BrowserContext* browser_context);
  ~WebApkInstallService() override;

  // Returns whether an install for |web_manifest_url| is in progress.
  bool IsInstallInProgress(const GURL& web_manifest_url);

  // Installs WebAPK and adds shortcut to the launcher. It talks to the Chrome
  // WebAPK server to generate a WebAPK on the server and to Google Play to
  // install the downloaded WebAPK.
  void InstallAsync(content::WebContents* web_contents,
                    const ShortcutInfo& shortcut_info,
                    const SkBitmap& primary_icon,
                    bool is_primary_icon_maskable,
                    const SkBitmap& badge_icon,
                    WebappInstallSource install_source);

  // Talks to the Chrome WebAPK server to update a WebAPK on the server and to
  // the Google Play server to install the downloaded WebAPK.
  // |update_request_path| is the path of the file with the update request.
  // Calls |finish_callback| once the update completed or failed.
  void UpdateAsync(const base::FilePath& update_request_path,
                   FinishCallback finish_callback);

 private:
  // Observes the lifetime of a WebContents.
  class LifetimeObserver : public content::WebContentsObserver {
   public:
    explicit LifetimeObserver(content::WebContents* web_contents)
        : WebContentsObserver(web_contents) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(LifetimeObserver);
  };

  // Called once the install/update completed or failed.
  void OnFinishedInstall(std::unique_ptr<LifetimeObserver> observer,
                         const ShortcutInfo& shortcut_info,
                         const SkBitmap& primary_icon,
                         bool is_priamry_icon_maskable,
                         WebApkInstallResult result,
                         bool relax_updates,
                         const std::string& webapk_package_name);

  // Shows a notification that an install is in progress.
  static void ShowInstallInProgressNotification(
      const ShortcutInfo& shortcut_info,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable);

  // Shows a notification that an install is completed.
  static void ShowInstalledNotification(const ShortcutInfo& shortcut_info,
                                        const SkBitmap& primary_icon,
                                        bool is_primary_icon_maskable,
                                        const std::string& webapk_package_name);

  content::BrowserContext* browser_context_;

  // In progress installs.
  std::set<GURL> installs_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstallService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallService);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_
