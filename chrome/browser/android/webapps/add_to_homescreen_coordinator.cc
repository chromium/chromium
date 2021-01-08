// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_coordinator.h"

#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/android/chrome_jni_headers/AddToHomescreenCoordinator_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/webapps/add_to_homescreen_installer.h"
#include "chrome/browser/android/webapps/add_to_homescreen_mediator.h"
#include "chrome/browser/android/webapps/add_to_homescreen_params.h"
#include "chrome/browser/banners/app_banner_manager.h"

namespace webapps {

// static
bool AddToHomescreenCoordinator::ShowForAppBanner(
    base::WeakPtr<AppBannerManager> weak_manager,
    std::unique_ptr<AddToHomescreenParams> params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        event_callback) {
  TabAndroid* tab_android =
      TabAndroid::FromWebContents(weak_manager->web_contents());

  JNIEnv* env = base::android::AttachCurrentThread();
  AddToHomescreenMediator* mediator = (AddToHomescreenMediator*)
      Java_AddToHomescreenCoordinator_initMvcAndReturnMediator(
          env, tab_android->GetJavaObject());
  if (!mediator)
    return false;

  mediator->StartForAppBanner(weak_manager, std::move(params),
                              std::move(event_callback));
  return true;
}

}  // namespace webapps
