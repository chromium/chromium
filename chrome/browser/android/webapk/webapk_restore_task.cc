// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_task.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "url/gurl.h"

namespace webapk {
namespace {

// The length of time to allow the add to homescreen data fetcher to run before
// timing out and generating an icon.
const int kDataFetcherTimeoutInMilliseconds = 12000;

}  // namespace

WebApkRestoreData::WebApkRestoreData(
    webapps::AppId id,
    std::unique_ptr<webapps::ShortcutInfo> info,
    base::Time time)
    : app_id(id), shortcut_info(std::move(info)), last_used_time(time) {}
WebApkRestoreData::~WebApkRestoreData() = default;

WebApkRestoreData::WebApkRestoreData(WebApkRestoreData&& other)
    : app_id(other.app_id),
      shortcut_info(std::move(other.shortcut_info)),
      last_used_time(other.last_used_time) {}

WebApkRestoreTask::WebApkRestoreTask(
    base::PassKey<WebApkRestoreManager> pass_key,
    Profile* profile,
    WebApkRestoreWebContentsManager* web_contents_manager,
    std::unique_ptr<webapps::ShortcutInfo> shortcut_info,
    base::Time last_used_time)
    : profile_(profile),
      web_contents_manager_(web_contents_manager->GetWeakPtr()),
      fallback_info_(std::move(shortcut_info)),
      last_used_time_(last_used_time) {}

WebApkRestoreTask::~WebApkRestoreTask() = default;

void WebApkRestoreTask::DownloadIcon(base::OnceClosure fetch_icon_callback) {
  if (!fallback_info_->best_primary_icon_url.is_valid()) {
    OnIconDownloaded(std::move(fetch_icon_callback), app_icon_);
    return;
  }

  content::ManifestIconDownloader::Download(
      web_contents_manager_->web_contents(),
      fallback_info_->best_primary_icon_url,
      webapps::WebappsIconUtils::GetIdealHomescreenIconSizeInPx(),
      webapps::WebappsIconUtils::GetMinimumHomescreenIconSizeInPx(),
      std::numeric_limits<int>::max(),
      base::BindOnce(&WebApkRestoreTask::OnIconDownloaded,
                     weak_factory_.GetWeakPtr(),
                     std::move(fetch_icon_callback)));
}

void WebApkRestoreTask::OnIconDownloaded(base::OnceClosure fetch_icon_callback,
                                         const SkBitmap& bitmap) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &webapps::WebappsIconUtils::FinalizeLauncherIconInBackground, bitmap,
          fallback_info_->url,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&WebApkRestoreTask::OnIconCreated,
                         weak_factory_.GetWeakPtr(),
                         std::move(fetch_icon_callback))));
}

void WebApkRestoreTask::OnIconCreated(base::OnceClosure fetch_icon_callback,
                                      const SkBitmap& bitmap,
                                      bool is_generated) {
  if ((is_generated || fallback_info_->is_primary_icon_maskable) &&
      webapps::WebappsIconUtils::DoesAndroidSupportMaskableIcons()) {
    app_icon_ = webapps::WebappsIconUtils::GenerateAdaptiveIconBitmap(bitmap);
  } else {
    app_icon_ = bitmap;
  }
  std::move(fetch_icon_callback).Run();
}

void WebApkRestoreTask::Start(CompleteCallback complete_callback) {
  complete_callback_ = std::move(complete_callback);

  web_contents_manager_->LoadUrl(
      fallback_info_->url, base::BindOnce(&WebApkRestoreTask::OnWebAppUrlLoaded,
                                          weak_factory_.GetWeakPtr()));
}

void WebApkRestoreTask::OnWebAppUrlLoaded(
    webapps::WebAppUrlLoaderResult result) {
  if (result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    // TODO(crbug.com/41496289): Log error and install fallback;
    return;
  }

  data_fetcher_ = std::make_unique<webapps::AddToHomescreenDataFetcher>(
      web_contents_manager_->web_contents(), kDataFetcherTimeoutInMilliseconds,
      this);
}

void WebApkRestoreTask::OnUserTitleAvailable(
    const std::u16string& user_title,
    const GURL& url,
    webapps::AddToHomescreenParams::AppType app_type) {}

void WebApkRestoreTask::OnDataAvailable(
    const webapps::ShortcutInfo& info,
    const SkBitmap& display_icon,
    webapps::AddToHomescreenParams::AppType app_type,
    const webapps::InstallableStatusCode status_code) {
  if (!webapps::AddToHomescreenParams::IsWebApk(app_type)) {
    // TODO(crbug.com/41496289): Log error, covert SHORTCUT to WebAPK.
    return;
  }

  // TODO(crbug.com/41496289): This should go through WebApkInstallService to
  // track current ongoing installs.
  // TODO(crbug.com/41496289): We need web_contents to construct the proto,
  // but generating WebAPK on server side and installing the apk can be done
  // in parallel with the next task.
  WebApkInstaller::InstallAsync(
      profile_, web_contents_manager_->web_contents(), info,
      webapps::WebappInstallSource::WEBAPK_RESTORE,
      base::BindOnce(&WebApkRestoreTask::OnFinishedInstall,
                     weak_factory_.GetWeakPtr()));
}

void WebApkRestoreTask::OnFinishedInstall(
    webapps::WebApkInstallResult result,
    bool relax_updates,
    const std::string& webapk_package_name) {
  if (result != webapps::WebApkInstallResult::SUCCESS) {
    // TODO(crbug.com/41496289): Log error and maybe install fallback.
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(complete_callback_),
                                fallback_info_->manifest_id, result));
}

GURL WebApkRestoreTask::manifest_id() const {
  return fallback_info_->manifest_id;
}

std::u16string WebApkRestoreTask::app_name() const {
  return fallback_info_->name;
}

}  // namespace webapk
