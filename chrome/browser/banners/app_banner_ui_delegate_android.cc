// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_ui_delegate_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/AppBannerUiDelegateAndroid_jni.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/webapps/add_to_homescreen_installer.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

namespace banners {

// static
std::unique_ptr<AppBannerUiDelegateAndroid> AppBannerUiDelegateAndroid::Create(
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<AddToHomescreenParams> params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        event_callback) {
  return std::unique_ptr<AppBannerUiDelegateAndroid>(
      new AppBannerUiDelegateAndroid(weak_manager, std::move(params),
                                     event_callback));
}

AppBannerUiDelegateAndroid::~AppBannerUiDelegateAndroid() {
  Java_AppBannerUiDelegateAndroid_destroy(base::android::AttachCurrentThread(),
                                          java_delegate_);
  java_delegate_.Reset();
}

void AppBannerUiDelegateAndroid::AddToHomescreen(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!weak_manager_.get())
    return;

  AddToHomescreenInstaller::Install(weak_manager_->web_contents(), *params_,
                                    event_callback_);
}

void AppBannerUiDelegateAndroid::OnUiCancelled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  OnUiCancelled();
}

void AppBannerUiDelegateAndroid::OnUiCancelled() {
  event_callback_.Run(AddToHomescreenInstaller::Event::UI_DISMISSED, *params_);
}

bool AppBannerUiDelegateAndroid::ShowDialog() {
  if (!weak_manager_.get())
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(!params_->primary_icon.drawsNothing());
  base::android::ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(&(params_->primary_icon));

  if (params_->app_type == AddToHomescreenParams::AppType::NATIVE) {
    bool was_shown = Java_AppBannerUiDelegateAndroid_showNativeAppDialog(
        env, java_delegate_, java_bitmap, params_->native_app_data);
    if (was_shown)
      event_callback_.Run(AddToHomescreenInstaller::Event::UI_SHOWN, *params_);
    return was_shown;
  }

  base::android::ScopedJavaLocalRef<jstring> java_app_title =
      base::android::ConvertUTF16ToJavaString(env,
                                              params_->shortcut_info->name);
  // Trim down the app URL to the origin. Banners only show on secure origins,
  // so elide the scheme.
  base::android::ScopedJavaLocalRef<jstring> java_app_url =
      base::android::ConvertUTF16ToJavaString(
          env, url_formatter::FormatUrlForSecurityDisplay(
                   params_->shortcut_info->url,
                   url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  bool was_shown = Java_AppBannerUiDelegateAndroid_showWebAppDialog(
      env, java_delegate_, java_app_title, java_bitmap, java_app_url,
      params_->has_maskable_primary_icon);
  if (was_shown)
    event_callback_.Run(AddToHomescreenInstaller::Event::UI_SHOWN, *params_);
  return was_shown;
}

bool AppBannerUiDelegateAndroid::ShowNativeAppDetails() {
  if (params_->native_app_data.is_null())
    return false;

  Java_AppBannerUiDelegateAndroid_showAppDetails(
      base::android::AttachCurrentThread(), java_delegate_,
      params_->native_app_data);

  event_callback_.Run(AddToHomescreenInstaller::Event::NATIVE_DETAILS_SHOWN,
                      *params_);
  return true;
}

bool AppBannerUiDelegateAndroid::ShowNativeAppDetails(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return ShowNativeAppDetails();
}

AppBannerUiDelegateAndroid::AppBannerUiDelegateAndroid(
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<AddToHomescreenParams> params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)> event_callback)
    : weak_manager_(weak_manager),
      params_(std::move(params)),
      event_callback_(event_callback) {
  CreateJavaDelegate();
}

void AppBannerUiDelegateAndroid::CreateJavaDelegate() {
  TabAndroid* tab = TabAndroid::FromWebContents(weak_manager_->web_contents());

  java_delegate_.Reset(Java_AppBannerUiDelegateAndroid_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      tab->GetJavaObject()));
}

}  // namespace banners
