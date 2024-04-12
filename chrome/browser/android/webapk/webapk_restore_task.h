// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"
#include "components/webapps/browser/android/add_to_homescreen_data_fetcher.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace webapps {
class WebAppUrlLoader;
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
  explicit WebApkRestoreTask(base::PassKey<WebApkRestoreManager>,
                             Profile* profile,
                             const sync_pb::WebApkSpecifics& webapk_specifics);
  WebApkRestoreTask(const WebApkRestoreTask&) = delete;
  WebApkRestoreTask& operator=(const WebApkRestoreTask&) = delete;
  ~WebApkRestoreTask() override;

  using CompleteCallback =
      base::OnceCallback<void(const GURL&, webapps::WebApkInstallResult)>;

  virtual void Start(WebApkRestoreWebContentsManager* web_contents_manager,
                     CompleteCallback complete_callback);

 private:
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

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<webapps::AddToHomescreenDataFetcher> data_fetcher_;

  const GURL manifest_id_;
  std::unique_ptr<webapps::ShortcutInfo> fallback_info_;

  base::WeakPtrFactory<WebApkRestoreTask> weak_factory_{this};
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_
