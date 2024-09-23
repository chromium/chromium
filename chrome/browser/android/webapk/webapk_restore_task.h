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
#include "components/webapps/common/web_app_id.h"
#include "third_party/skia/include/core/SkBitmap.h"

class WebApkInstallService;

namespace webapps {
enum class InstallableStatusCode;
enum class WebAppUrlLoaderResult;
enum class WebApkInstallResult;
}  // namespace webapps

namespace webapk {

class WebApkRestoreManager;
class WebApkRestoreWebContentsManager;

struct WebApkRestoreData {
  WebApkRestoreData() = delete;
  explicit WebApkRestoreData(
      webapps::AppId app_id,
      std::unique_ptr<webapps::ShortcutInfo> shortcut_info,
      base::Time);
  ~WebApkRestoreData();
  WebApkRestoreData(WebApkRestoreData&& other);
  WebApkRestoreData(const WebApkRestoreData&) = delete;
  WebApkRestoreData& operator=(const WebApkRestoreData&) = delete;

  webapps::AppId app_id;
  // Fallback shortcut info
  std::unique_ptr<webapps::ShortcutInfo> shortcut_info;
  // Time when this WebApk was last used or installed
  base::Time last_used_time;
};

// Task for installing previously synced WebAPK on new devices. Each instance
// represents a WebAPK to be install.
class WebApkRestoreTask : public webapps::AddToHomescreenDataFetcher::Observer {
 public:
  explicit WebApkRestoreTask(
      base::PassKey<WebApkRestoreManager>,
      WebApkInstallService* web_apk_install_service,
      WebApkRestoreWebContentsManager* web_contents_manager,
      std::unique_ptr<webapps::ShortcutInfo> fallback_info,
      base::Time last_used_time);

  WebApkRestoreTask(const WebApkRestoreTask&) = delete;
  WebApkRestoreTask& operator=(const WebApkRestoreTask&) = delete;
  ~WebApkRestoreTask() override;

  using CompleteCallback =
      base::OnceCallback<void(const GURL&, webapps::WebApkInstallResult)>;

  // LINT.IfChange(WebApkRestoreFallbackReason)
  enum class FallbackReason {
    kNone = 0,
    kLoadUrl = 1,
    kManifestIdMismatch = 2,
    kNotWebApkCompatible = 3,
    kMaxValue = kNotWebApkCompatible,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/web_apk/enums.xml:WebApkRestoreFallbackReason)

  virtual void Start(CompleteCallback complete_callback);

  void DownloadIcon(base::OnceClosure fetch_icon_callback);

  GURL manifest_id() const;
  std::u16string app_name() const;
  const SkBitmap& app_icon() const { return app_icon_; }
  base::Time last_used_time() const { return last_used_time_; }

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

  std::unique_ptr<webapps::ShortcutInfo> UpdateFetchedInfoWithFallbackInfo(
      const webapps::ShortcutInfo& fetched_info);

  void Install(const webapps::ShortcutInfo& restore_info,
               const SkBitmap& display_icon,
               FallbackReason fallback_reason);
  void OnFinishedInstall(bool is_fallback, webapps::WebApkInstallResult result);

  raw_ptr<WebApkInstallService> web_apk_install_service_;
  base::WeakPtr<WebApkRestoreWebContentsManager> web_contents_manager_;

  CompleteCallback complete_callback_;

  std::unique_ptr<webapps::ShortcutInfo> fallback_info_;
  SkBitmap app_icon_;
  base::Time last_used_time_;

  std::unique_ptr<webapps::AddToHomescreenDataFetcher> data_fetcher_;

  base::WeakPtrFactory<WebApkRestoreTask> weak_factory_{this};
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_TASK_H_
