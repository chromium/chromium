// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/AddToHomescreenManager_jni.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapps/add_to_homescreen_installer.h"
#include "chrome/browser/banners/app_banner_manager_android.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/installation/installation.mojom.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// The length of time to allow the add to homescreen data fetcher to run before
// timing out and generating an icon.
const int kDataTimeoutInMilliseconds = 8000;

}  // namespace

jlong JNI_AddToHomescreenManager_InitializeAndStart(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  AddToHomescreenManager* manager = new AddToHomescreenManager(env, obj);
  manager->Start(web_contents);
  return reinterpret_cast<intptr_t>(manager);
}

AddToHomescreenManager::AddToHomescreenManager(JNIEnv* env, jobject obj)
    : is_webapk_compatible_(false) {
  java_ref_.Reset(env, obj);
}

void AddToHomescreenManager::Destroy(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  delete this;
}

void AddToHomescreenManager::AddToHomescreen(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_user_title) {
  content::WebContents* web_contents = data_fetcher_->web_contents();
  if (!web_contents)
    return;

  AddToHomescreenParams params;
  params.app_type = data_fetcher_->shortcut_info().source ==
                            ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_PWA
                        ? AddToHomescreenParams::AppType::WEBAPK
                        : AddToHomescreenParams::AppType::SHORTCUT;
  params.shortcut_info =
      std::make_unique<ShortcutInfo>(data_fetcher_->shortcut_info());
  params.primary_icon = data_fetcher_->primary_icon();
  params.badge_icon = data_fetcher_->badge_icon();
  params.has_maskable_primary_icon = data_fetcher_->has_maskable_primary_icon();
  params.install_source = InstallableMetrics::GetInstallSource(
      data_fetcher_->web_contents(), InstallTrigger::MENU);
  if (!is_webapk_compatible_) {
    params.shortcut_info->user_title =
        base::android::ConvertJavaStringToUTF16(env, j_user_title);
  }

  // base::Unretained() is safe because the lifetime of this object is
  // controlled by its Java counterpart. It will be destroyed when the add to
  // home screen prompt is dismissed, which occurs after the last time
  // RecordEventForAppMenu can be called.
  AddToHomescreenInstaller::Install(
      data_fetcher_->web_contents(), params,
      base::Bind(&AddToHomescreenManager::RecordEventForAppMenu,
                 base::Unretained(this)));
}

void AddToHomescreenManager::RecordEventForAppMenu(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  DCHECK_NE(a2hs_params.app_type, AddToHomescreenParams::AppType::NATIVE);
  if (!data_fetcher_->web_contents())
    return;

  switch (event) {
    case AddToHomescreenInstaller::Event::INSTALL_STARTED:
      AppBannerSettingsHelper::RecordBannerEvent(
          data_fetcher_->web_contents(),
          data_fetcher_->web_contents()->GetURL(),
          a2hs_params.shortcut_info->url.spec(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
          base::Time::Now());
      break;
    case AddToHomescreenInstaller::Event::INSTALL_REQUEST_FINISHED: {
      banners::AppBannerManagerAndroid* app_banner_manager =
          banners::AppBannerManagerAndroid::FromWebContents(
              data_fetcher_->web_contents());
      // Fire the appinstalled event and do install time logging.
      if (app_banner_manager)
        app_banner_manager->OnInstall(a2hs_params.shortcut_info->display);
      break;
    }
    default:
      break;
  }
}

void AddToHomescreenManager::Start(content::WebContents* web_contents) {
  // Icon generation depends on having a valid visible URL.
  DCHECK(web_contents->GetVisibleURL().is_valid());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AddToHomescreenManager_showDialog(env, java_ref_);

  data_fetcher_ = std::make_unique<AddToHomescreenDataFetcher>(
      web_contents, kDataTimeoutInMilliseconds, this);
}

AddToHomescreenManager::~AddToHomescreenManager() {}

void AddToHomescreenManager::OnUserTitleAvailable(
    const base::string16& user_title,
    const GURL& url,
    bool is_webapk_compatible) {
  is_webapk_compatible_ = is_webapk_compatible;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_user_title =
      base::android::ConvertUTF16ToJavaString(env, user_title);
  // Trim down the app URL to the origin. Elide cryptographic schemes so HTTP
  // is still shown.
  ScopedJavaLocalRef<jstring> j_url = base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  Java_AddToHomescreenManager_onUserTitleAvailable(
      env, java_ref_, j_user_title, j_url,
      is_webapk_compatible_ /* isWebapp */);
}

void AddToHomescreenManager::OnDataAvailable(const ShortcutInfo& info,
                                             const SkBitmap& primary_icon,
                                             const SkBitmap& badge_icon) {
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!primary_icon.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(&primary_icon);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AddToHomescreenManager_onIconAvailable(
      env, java_ref_, java_bitmap, data_fetcher_->has_maskable_primary_icon());
}
