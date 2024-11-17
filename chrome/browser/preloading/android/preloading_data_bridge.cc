// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/preloading/chrome_preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/preloading/android/jni_headers/PreloadingDataBridge_jni.h"

static void JNI_PreloadingDataBridge_SetIsNavigationInDomainCallbackForCct(
    JNIEnv* env,
    content::WebContents* web_contents) {
  content::PreloadingData* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(web_contents);
  // Basically a navigation to CCT, which is the predictor's domain here, is
  // happening alone. Therefore, once by setting the predictor value to
  // PreloadingDataImpl just before the target navigation starts (i.e. calling
  // this function just before CCT navigation begins),
  // PreloadingDataImpl::DidStartNavigation can assume that the very first
  // navigation after setting predictor value is the navigation as its
  // predictor’s domain. Callback is not needed in this situation, so it just
  // always returns true. (Normally, it is used to recognize whether a
  // navigation observed in PreloadingDataImpl::DidStartNavigation is the
  // predictor's domain from NavigationHandle's info.)
  preloading_data->SetIsNavigationInDomainCallback(
      chrome_preloading_predictor::kChromeCustomTabs,
      base::BindRepeating([](content::NavigationHandle* navigation_handle)
                              -> bool { return true; }));
}
