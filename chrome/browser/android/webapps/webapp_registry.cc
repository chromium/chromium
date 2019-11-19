// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/webapp_registry.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/WebappRegistry_jni.h"
#include "chrome/browser/android/browsing_data/url_filter_bridge.h"

using base::android::JavaParamRef;

void WebappRegistry::UnregisterWebappsForUrls(
    const base::Callback<bool(const GURL&)>& url_filter) {
  // |filter_bridge| is destroyed from its Java counterpart.
  UrlFilterBridge* filter_bridge = new UrlFilterBridge(url_filter);

  Java_WebappRegistry_unregisterWebappsForUrls(
      base::android::AttachCurrentThread(), filter_bridge->j_bridge());
}

void WebappRegistry::ClearWebappHistoryForUrls(
    const base::Callback<bool(const GURL&)>& url_filter) {
  // |filter_bridge| is destroyed from its Java counterpart.
  UrlFilterBridge* filter_bridge = new UrlFilterBridge(url_filter);

  Java_WebappRegistry_clearWebappHistoryForUrls(
      base::android::AttachCurrentThread(), filter_bridge->j_bridge());
}
