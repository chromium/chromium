// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_task.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
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

constexpr char kFetchDataResultAppTypeHistogram[] =
    "WebApk.Sync.Restore.FetchInstallableData.AppType";
constexpr char kFetchDataResultCodeHistogram[] =
    "WebApk.Sync.Restore.FetchInstallableData.NotWebApkStatusCode";
constexpr char kRestoreInstallEventHistogram[] =
    "WebApk.Sync.Restore.InstallEvent";
constexpr char kRestoreInstallFallbackWebApkResultHistogram[] =
    "WebApk.Sync.Restore.InstallResult.Fallback";
constexpr char kRestoreInstallFetchedWebApkResultHistogram[] =
    "WebApk.Sync.Restore.InstallResult.Fetched";

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
    WebApkInstallService* web_apk_install_service,
    WebApkRestoreWebContentsManager* web_contents_manager,
    std::unique_ptr<webapps::ShortcutInfo> shortcut_info,
    base::Time last_used_time)
    : web_apk_install_service_(web_apk_install_service),
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
    Install(*fallback_info_, app_icon_, FallbackReason::kLoadUrl);
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
    const webapps::ShortcutInfo& fetched_info,
    const SkBitmap& primary_icon,
    webapps::AddToHomescreenParams::AppType app_type,
    const webapps::InstallableStatusCode status_code) {
  base::UmaHistogramEnumeration(kFetchDataResultAppTypeHistogram, app_type);
  base::UmaHistogramEnumeration(kFetchDataResultCodeHistogram, status_code);

  if (fetched_info.manifest_id != manifest_id()) {
    Install(*fallback_info_, app_icon_, FallbackReason::kManifestIdMismatch);
    return;
  }

  if (!webapps::AddToHomescreenParams::IsWebApk(app_type)) {
    CHECK(fetched_info.url.is_valid());
    auto restore_info = UpdateFetchedInfoWithFallbackInfo(fetched_info);
    Install(*restore_info, primary_icon, FallbackReason::kNotWebApkCompatible);
    return;
  }

  Install(fetched_info, primary_icon, FallbackReason::kNone);
}

std::unique_ptr<webapps::ShortcutInfo>
WebApkRestoreTask::UpdateFetchedInfoWithFallbackInfo(
    const webapps::ShortcutInfo& fetched_info) {
  auto new_info = std::make_unique<webapps::ShortcutInfo>(fetched_info);
  if (new_info->name.empty()) {
    new_info->name = fallback_info_->name;
    new_info->short_name = fallback_info_->short_name;
  }
  if (!new_info->best_primary_icon_url.is_valid()) {
    new_info->best_primary_icon_url = fallback_info_->best_primary_icon_url;
    new_info->is_primary_icon_maskable =
        fallback_info_->is_primary_icon_maskable;
  }
  return new_info;
}

void WebApkRestoreTask::Install(const webapps::ShortcutInfo& restore_info,
                                const SkBitmap& primary_icon,
                                FallbackReason fallback_reason) {
  base::UmaHistogramEnumeration(kRestoreInstallEventHistogram, fallback_reason);

  // TODO(crbug.com/41496289): We need web_contents to construct the proto,
  // but generating WebAPK on server side and installing the apk can be done
  // in parallel with the next task.
  web_apk_install_service_->InstallRestoreAsync(
      web_contents_manager_->web_contents(), restore_info, primary_icon,
      webapps::WebappInstallSource::WEBAPK_RESTORE,
      base::BindOnce(&WebApkRestoreTask::OnFinishedInstall,
                     weak_factory_.GetWeakPtr(),
                     fallback_reason != FallbackReason::kNone));
}

void WebApkRestoreTask::OnFinishedInstall(bool is_fallback,
                                          webapps::WebApkInstallResult result) {
  base::UmaHistogramEnumeration(
      (is_fallback ? kRestoreInstallFallbackWebApkResultHistogram
                   : kRestoreInstallFetchedWebApkResultHistogram),
      result);

  if (complete_callback_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(complete_callback_), manifest_id(), result));
  }
}

GURL WebApkRestoreTask::manifest_id() const {
  return fallback_info_->manifest_id;
}

std::u16string WebApkRestoreTask::app_name() const {
  return fallback_info_->name;
}

}  // namespace webapk
