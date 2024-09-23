// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_install_service.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/WebApkInstallService_jni.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/android/webapps_utils.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/android/java_bitmap.h"

WebApkInstallService::WebApkInstallService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

WebApkInstallService::~WebApkInstallService() {}

bool WebApkInstallService::IsInstallInProgress(const GURL& web_manifest_id) {
  return install_ids_.count(web_manifest_id);
}

void WebApkInstallService::InstallAsync(
    content::WebContents* web_contents,
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    webapps::WebappInstallSource install_source) {
  if (IsInstallInProgress(shortcut_info.manifest_id)) {
    webapps::WebappsUtils::ShowWebApkInstallResultToast(
        webapps::WebApkInstallResult::INSTALL_ALREADY_IN_PROGRESS);
    return;
  }

  install_ids_.insert(shortcut_info.manifest_id);
  webapps::InstallableMetrics::TrackInstallEvent(install_source);

  ShowInstallInProgressNotification(
      shortcut_info.manifest_id, shortcut_info.short_name, shortcut_info.url,
      primary_icon, shortcut_info.is_primary_icon_maskable);

  // We pass an weak ptr to a WebContents to the callback, since the
  // installation may take more than 10 seconds so there is a chance that the
  // WebContents has been destroyed before the install is finished.
  WebApkInstaller::InstallAsync(
      browser_context_, web_contents, shortcut_info, primary_icon,
      install_source,
      base::BindOnce(&WebApkInstallService::OnFinishedInstall,
                     weak_ptr_factory_.GetWeakPtr(), web_contents->GetWeakPtr(),
                     shortcut_info, primary_icon));
}

void WebApkInstallService::InstallRestoreAsync(
    content::WebContents* web_contents,
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    webapps::WebappInstallSource install_source,
    InstallFinishCallback finish_callback) {
  if (IsInstallInProgress(shortcut_info.manifest_id)) {
    std::move(finish_callback)
        .Run(webapps::WebApkInstallResult::INSTALL_ALREADY_IN_PROGRESS);
    return;
  }

  install_ids_.insert(shortcut_info.manifest_id);
  webapps::InstallableMetrics::TrackInstallEvent(install_source);

  ShowInstallInProgressNotification(
      shortcut_info.manifest_id, shortcut_info.short_name, shortcut_info.url,
      primary_icon, shortcut_info.is_primary_icon_maskable);

  // We pass an weak ptr to a WebContents to the callback, since the
  // installation may take more than 10 seconds so there is a chance that the
  // WebContents has been destroyed before the install is finished.
  WebApkInstaller::InstallAsync(
      browser_context_, web_contents, shortcut_info, primary_icon,
      install_source,
      base::BindOnce(&WebApkInstallService::OnFinishedInstallRestore,
                     weak_ptr_factory_.GetWeakPtr(), shortcut_info,
                     primary_icon, std::move(finish_callback)));
}

void WebApkInstallService::UpdateAsync(
    const base::FilePath& update_request_path,
    FinishCallback finish_callback) {
  WebApkInstaller::UpdateAsync(browser_context_, update_request_path,
                               std::move(finish_callback));
}

void WebApkInstallService::OnFinishedInstall(
    base::WeakPtr<content::WebContents> web_contents,
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    webapps::WebApkInstallResult result,
    bool relax_updates,
    const std::string& webapk_package_name) {
  install_ids_.erase(shortcut_info.manifest_id);

  bool show_failure_notification = base::FeatureList::IsEnabled(
      webapps::features::kWebApkInstallFailureNotification);
  HandleFinishInstallNotifications(
      shortcut_info.manifest_id, shortcut_info.url, shortcut_info.short_name,
      primary_icon, shortcut_info.is_primary_icon_maskable, result,
      webapk_package_name, show_failure_notification);

  if (show_failure_notification) {
    return;
  }

  // If WebAPK install failed, try adding a shortcut instead.
  // If the install didn't definitely fail (i.e. PROBABLE_FAILURE), we don't add
  // a shortcut. This could happen if Play was busy with another install and
  // this one is still queued (and hence might succeed in the future).
  if (result != webapps::WebApkInstallResult::SUCCESS &&
      result != webapps::WebApkInstallResult::PROBABLE_FAILURE) {
    if (!web_contents)
      return;

    // TODO(crbug.com/40584062): Support maskable icons here.
    ShortcutHelper::AddToLauncherWithSkBitmap(
        web_contents.get(), shortcut_info, primary_icon,
        webapps::InstallableStatusCode::WEBAPK_INSTALL_FAILED);
  }
}

void WebApkInstallService::OnFinishedInstallRestore(
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    InstallFinishCallback finish_callback,
    webapps::WebApkInstallResult result,
    bool /* relax_updates */,
    const std::string& webapk_package_name) {
  install_ids_.erase(shortcut_info.manifest_id);
  HandleFinishInstallNotifications(
      shortcut_info.manifest_id, shortcut_info.url, shortcut_info.short_name,
      primary_icon, shortcut_info.is_primary_icon_maskable, result,
      webapk_package_name, /* show_failure_notification= */ true);

  std::move(finish_callback).Run(result);
}

void WebApkInstallService::HandleFinishInstallNotifications(
    const GURL& notification_id,
    const GURL& url,
    const std::u16string& short_name,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    webapps::WebApkInstallResult result,
    const std::string& webapk_package_name,
    bool show_failure_notification) {
  if (result == webapps::WebApkInstallResult::SUCCESS) {
    ShowInstalledNotification(notification_id, short_name, url, primary_icon,
                              is_primary_icon_maskable, webapk_package_name);
  } else if (show_failure_notification) {
    ShowInstallFailedNotification(notification_id, short_name, url,
                                  primary_icon, is_primary_icon_maskable,
                                  result);
  } else {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jstring> java_notification_id =
        base::android::ConvertUTF8ToJavaString(env, notification_id.spec());
    Java_WebApkInstallService_cancelNotification(env, java_notification_id);
  }
}

// static
void WebApkInstallService::ShowInstallInProgressNotification(
    const GURL& notification_id,
    const std::u16string& short_name,
    const GURL& url,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_notification_id =
      base::android::ConvertUTF8ToJavaString(env, notification_id.spec());
  base::android::ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, short_name);
  base::android::ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  base::android::ScopedJavaLocalRef<jobject> java_primary_icon =
      !primary_icon.isNull() ? gfx::ConvertToJavaBitmap(primary_icon) : nullptr;

  Java_WebApkInstallService_showInstallInProgressNotification(
      env, java_notification_id, java_short_name, java_url, java_primary_icon,
      is_primary_icon_maskable);
}

// static
void WebApkInstallService::ShowInstalledNotification(
    const GURL& notification_id,
    const std::u16string& short_name,
    const GURL& url,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    const std::string& webapk_package_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_webapk_package =
      base::android::ConvertUTF8ToJavaString(env, webapk_package_name);
  base::android::ScopedJavaLocalRef<jstring> java_notification_id =
      base::android::ConvertUTF8ToJavaString(env, notification_id.spec());
  base::android::ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, short_name);
  base::android::ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  base::android::ScopedJavaLocalRef<jobject> java_primary_icon =
      !primary_icon.isNull() ? gfx::ConvertToJavaBitmap(primary_icon) : nullptr;

  Java_WebApkInstallService_showInstalledNotification(
      env, java_webapk_package, java_notification_id, java_short_name, java_url,
      java_primary_icon, is_primary_icon_maskable);
}

// static
void WebApkInstallService::ShowInstallFailedNotification(
    const GURL& notification_id,
    const std::u16string& short_name,
    const GURL& url,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    webapps::WebApkInstallResult result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_notification_id =
      base::android::ConvertUTF8ToJavaString(env, notification_id.spec());
  base::android::ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, short_name);
  base::android::ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  base::android::ScopedJavaLocalRef<jobject> java_primary_icon =
      !primary_icon.isNull() ? gfx::ConvertToJavaBitmap(primary_icon) : nullptr;

  Java_WebApkInstallService_showInstallFailedNotification(
      env, java_notification_id, java_short_name, java_url, java_primary_icon,
      is_primary_icon_maskable, static_cast<int>(result));
}
