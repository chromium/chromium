// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_task.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "url/gurl.h"

namespace webapk {
namespace {

// The length of time to allow the add to homescreen data fetcher to run before
// timing out and generating an icon.
const int kDataFetcherTimeoutInMilliseconds = 12000;

}  // namespace

WebApkRestoreTask::WebApkRestoreTask(
    base::PassKey<WebApkRestoreManager> pass_key,
    Profile* profile,
    const sync_pb::WebApkSpecifics& webapk_specifics)
    : profile_(profile) {
  fallback_info_ = std::make_unique<webapps::ShortcutInfo>(
      GURL(webapk_specifics.start_url()));
  fallback_info_->manifest_id = GURL(webapk_specifics.manifest_id());
  fallback_info_->scope = GURL(webapk_specifics.scope());
  fallback_info_->user_title = base::UTF8ToUTF16(webapk_specifics.name());
  fallback_info_->name = fallback_info_->user_title;
  fallback_info_->short_name = fallback_info_->user_title;
}

WebApkRestoreTask::~WebApkRestoreTask() = default;

void WebApkRestoreTask::Start(
    WebApkRestoreWebContentsManager* web_contents_manager,
    CompleteCallback complete_callback) {
  web_contents_manager_ = web_contents_manager->GetWeakPtr();
  complete_callback_ = std::move(complete_callback);

  url_loader_ = web_contents_manager_->CreateUrlLoader();
  url_loader_->LoadUrl(
      fallback_info_->url, web_contents_manager_->web_contents(),
      webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&WebApkRestoreTask::OnWebAppUrlLoaded,
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
  // TODO(crbug.com/41496289): We need web_contents to construct the proto, but
  // generating WebAPK on server side and installing the apk can be done in
  // parallel with the next task.
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

}  // namespace webapk
