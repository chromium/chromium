// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_HELPER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/common/manifest/manifest.h"

class WebAppIconDownloader;
struct InstallableData;
class InstallableManager;
class Profile;
class SkBitmap;

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class CrxInstaller;
class Extension;
class ExtensionService;

// A helper class for creating bookmark apps from a WebContents.
class BookmarkAppHelper : public content::NotificationObserver {
 public:
  enum class ForInstallableSite {
    kYes,
    kNo,
    kUnknown,
  };

  typedef base::Callback<void(const Extension*, const WebApplicationInfo&)>
      CreateBookmarkAppCallback;

  // This helper class will create a bookmark app out of |web_app_info| and
  // install it to |service|. Icons will be downloaded from the URLs in
  // |web_app_info.icons| using |contents| if |contents| is not NULL.
  // All existing icons from WebApplicationInfo will also be used. The user
  // will then be prompted to edit the creation information via a bubble and
  // will have a chance to cancel the operation.
  // |install_source| indicates how the installation was triggered.
  BookmarkAppHelper(Profile* profile,
                    WebApplicationInfo web_app_info,
                    content::WebContents* contents,
                    WebappInstallSource install_source);
  ~BookmarkAppHelper() override;

  // Update the given WebApplicationInfo with information from the manifest.
  static void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                           WebApplicationInfo* web_app_info,
                                           ForInstallableSite installable_site);

  // It is important that the linked app information in any extension that
  // gets created from sync matches the linked app information that came from
  // sync. If there are any changes, they will be synced back to other devices
  // and could potentially create a never ending sync cycle.
  // This function updates |web_app_info| with the image data of any icon from
  // |bitmap_map| that has a URL and size matching that in |web_app_info|, as
  // well as adding any new images from |bitmap_map| that have no URL.
  static void UpdateWebAppIconsWithoutChangingLinks(
      std::map<int, web_app::BitmapAndSource> bitmap_map,
      WebApplicationInfo* web_app_info);

  // Begins the asynchronous bookmark app creation.
  void Create(const CreateBookmarkAppCallback& callback);

  // If called, the installed extension will be considered policy installed.
  void set_is_policy_installed_app() { is_policy_installed_app_ = true; }

  bool is_policy_installed_app() { return is_policy_installed_app_; }

  // Forces the creation of a shortcut app instead of a PWA even if installation
  // is available.
  void set_shortcut_app_requested() { shortcut_app_requested_ = true; }

  // If called, the installed extension will be considered default installed.
  void set_is_default_app() { is_default_app_ = true; }

  bool is_default_app() { return is_default_app_; }

  // If called, the installed extension will be considered system installed.
  void set_is_system_app() { is_system_app_ = true; }

  bool is_system_app() { return is_system_app_; }

  // If called, desktop shortcuts will not be created.
  void set_skip_shortcut_creation() { create_shortcuts_ = false; }

  bool create_shortcuts() const { return create_shortcuts_; }

  // If called, the installability check won't test for a service worker.
  void set_bypass_service_worker_check() {
    DCHECK(is_default_app() || is_system_app());
    bypass_service_worker_check_ = true;
  }

  // If called, the installation will only succeed if a manifest is found.
  void set_require_manifest() {
    DCHECK(is_default_app());
    require_manifest_ = true;
  }

  // If called, the installed app will launch in |launch_type|. User might still
  // be able to change the launch type depending on the type of app.
  void set_forced_launch_type(LaunchType launch_type) {
    forced_launch_type_ = launch_type;
  }

  const base::Optional<LaunchType>& forced_launch_type() const {
    return forced_launch_type_;
  }

 protected:
  // Protected methods for testing.

  // Called by the InstallableManager when the installability check is
  // completed.
  void OnDidPerformInstallableCheck(const InstallableData& data);

  // Performs post icon download tasks including installing the bookmark app.
  virtual void OnIconsDownloaded(
      bool success,
      const std::map<GURL, std::vector<SkBitmap>>& bitmaps);

  // Downloads icons from the given WebApplicationInfo using the given
  // WebContents.
  std::unique_ptr<WebAppIconDownloader> web_app_icon_downloader_;

 private:
  FRIEND_TEST_ALL_PREFIXES(BookmarkAppHelperTest,
                           CreateWindowedPWAIntoAppWindow);

  // Called after the bubble has been shown, and the user has either accepted or
  // the dialog was dismissed.
  void OnBubbleCompleted(bool user_accepted,
                         const WebApplicationInfo& web_app_info);

  // Called when the installation of the app is complete to perform the final
  // installation steps.
  void FinishInstallation(const Extension* extension);

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // The profile that the bookmark app is being added to.
  Profile* profile_;

  // The web contents that the bookmark app is being created for.
  content::WebContents* contents_;

  // The WebApplicationInfo that the bookmark app is being created for.
  WebApplicationInfo web_app_info_;

  // Called on app creation or failure.
  CreateBookmarkAppCallback callback_;

  // Used to install the created bookmark app.
  scoped_refptr<extensions::CrxInstaller> crx_installer_;

  content::NotificationRegistrar registrar_;

  InstallableManager* installable_manager_;

  ForInstallableSite for_installable_site_ = ForInstallableSite::kUnknown;

  base::Optional<LaunchType> forced_launch_type_;

  bool is_policy_installed_app_ = false;

  bool shortcut_app_requested_ = false;

  bool is_default_app_ = false;

  bool is_system_app_ = false;

  bool create_shortcuts_ = true;

  bool bypass_service_worker_check_ = false;

  bool require_manifest_ = false;

  // The mechanism via which the app creation was triggered.
  WebappInstallSource install_source_;

  // With fast tab unloading enabled, shutting down can cause BookmarkAppHelper
  // to be destroyed before the bookmark creation bubble. Use weak pointers to
  // prevent a heap-use-after free in this instance (https://crbug.com/534994).
  base::WeakPtrFactory<BookmarkAppHelper> weak_factory_;
};

// Creates or updates a bookmark app from the given |web_app_info|. Icons will
// be downloaded from the icon URLs provided in |web_app_info|.
void CreateOrUpdateBookmarkApp(ExtensionService* service,
                               WebApplicationInfo* web_app_info,
                               bool is_locally_installed);

// Returns whether the given |url| is a valid user bookmark app url.
bool IsValidBookmarkAppUrl(const GURL& url);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_HELPER_H_
