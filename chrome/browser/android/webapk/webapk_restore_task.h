// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace webapps {
enum class InstallableStatusCode;
enum class WebAppUrlLoaderResult;
enum class WebApkInstallResult;
}  // namespace webapps

namespace webapk {

class WebApkRestoreManager;
class WebApkRestoreWebContentsManager;

// Task for installing previously synced WebAPK on new devices. Each instance
// represents a WebAPK to be install.
class WebApkRestoreTask : public webapps::AddToHomescreenDataFetcher::Observer {
 public:
  explicit WebApkRestoreTask(
      base::PassKey<WebApkRestoreManager>,
      Profile* profile,
      WebApkRestoreWebContentsManager* web_contents_manager,
      std::unique_ptr<webapps::ShortcutInfo> shortcut_info);

  WebApkRestoreTask(const WebApkRestoreTask&) = delete;
  WebApkRestoreTask& operator=(const WebApkRestoreTask&) = delete;
  ~WebApkRestoreTask() override;

  using CompleteCallback =
      base::OnceCallback<void(const GURL&, webapps::WebApkInstallResult)>;

  virtual void Start(CompleteCallback complete_callback);

  void DownloadIcon(base::OnceClosure fetch_icon_callback);

  std::u16string AppName();
  const SkBitmap& app_icon() const { return app_icon_; }

 protected:
  std::unique_ptr<webapps::ShortcutInfo> fallback_info_;

 private:
  void OnIconDownloaded(base::OnceClosure fetch_icon_callback,
                        const SkBitmap& bitmap);
  void OnIconCreated(base::OnceClosure fetch_icon_callback,
                     const SkBitmap& bitmap,
                     bool is_generated);

  void OnWebAppUrlLoaded(webapps::WebAppUrlLoaderResult result);

  // AddToHomescreenDataFetcher::Observer:
  void OnUserTitleAvailable(
      const std::u16string& user_title,
      const GURL& url,
      webapps::AddToHomescreenParams::AppType app_type) override;
  void OnDataAvailable(const webapps::ShortcutInfo& info,
                       const SkBitmap& display_icon,
                       webapps::AddToHomescreenParams::AppType app_type,
                       webapps::InstallableStatusCode status_code) override;

  void OnFinishedInstall(webapps::WebApkInstallResult result,
                         bool relax_updates,
                         const std::string& webapk_package_name);

  raw_ptr<Profile> profile_;
  base::WeakPtr<WebApkRestoreWebContentsManager> web_contents_manager_;

  CompleteCallback complete_callback_;

  SkBitmap app_icon_;

  std::unique_ptr<webapps::AddToHomescreenDataFetcher> data_fetcher_;

  base::WeakPtrFactory<WebApkRestoreTask> weak_factory_{this};
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_
