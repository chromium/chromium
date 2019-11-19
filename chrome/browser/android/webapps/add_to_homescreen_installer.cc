// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_installer.h"

#include <utility>

#include "base/callback.h"
#include "chrome/android/chrome_jni_headers/AddToHomescreenInstaller_jni.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "content/public/browser/web_contents.h"

// static
void AddToHomescreenInstaller::Install(
    content::WebContents* web_contents,
    const AddToHomescreenParams& params,
    const base::RepeatingCallback<void(Event, const AddToHomescreenParams&)>&
        event_callback) {
  if (!web_contents) {
    event_callback.Run(Event::INSTALL_FAILED, params);
    return;
  }

  event_callback.Run(Event::INSTALL_STARTED, params);
  switch (params.app_type) {
    case AddToHomescreenParams::AppType::NATIVE:
      InstallOrOpenNativeApp(web_contents, params, event_callback);
      break;
    case AddToHomescreenParams::AppType::WEBAPK:
      InstallWebApk(web_contents, params);
      break;
    case AddToHomescreenParams::AppType::SHORTCUT:
      InstallShortcut(web_contents, params);
      break;
  }
  event_callback.Run(Event::INSTALL_REQUEST_FINISHED, params);
}

// static
void AddToHomescreenInstaller::InstallOrOpenNativeApp(
    content::WebContents* web_contents,
    const AddToHomescreenParams& params,
    const base::RepeatingCallback<void(Event, const AddToHomescreenParams&)>&
        event_callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);

  if (tab == nullptr) {
    event_callback.Run(Event::INSTALL_FAILED, params);
    return;
  }

  bool was_successful = Java_AddToHomescreenInstaller_installOrOpenNativeApp(
      env, tab->GetJavaObject(), params.native_app_data);
  event_callback.Run(was_successful ? Event::NATIVE_INSTALL_OR_OPEN_SUCCEEDED
                                    : Event::NATIVE_INSTALL_OR_OPEN_FAILED,
                     params);
}

// static
void AddToHomescreenInstaller::InstallWebApk(
    content::WebContents* web_contents,
    const AddToHomescreenParams& params) {
  WebApkInstallService::Get(web_contents->GetBrowserContext())
      ->InstallAsync(web_contents, *(params.shortcut_info), params.primary_icon,
                     params.has_maskable_primary_icon, params.badge_icon,
                     params.install_source);
}

// static
void AddToHomescreenInstaller::InstallShortcut(
    content::WebContents* web_contents,
    const AddToHomescreenParams& params) {
  ShortcutHelper::AddToLauncherWithSkBitmap(
      web_contents, *(params.shortcut_info), params.primary_icon,
      params.has_maskable_primary_icon);
}
