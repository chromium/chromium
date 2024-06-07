// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The enum in this file is originated from the NotRestoredReason enumeration in
// tools/metrics/histograms/metadata/navigation/enums.xml. Only the values that
// may be used by the WebView Java code is exported.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BACKFORWARD_CACHE_NOT_RESTORED_REASON_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BACKFORWARD_CACHE_NOT_RESTORED_REASON_H_

#include "content/public/browser/back_forward_cache.h"

namespace android_webview {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview.metrics
enum BackForwardCacheNotRestoredReason {
  CACHE_FLUSHED = 21,
  WEBVIEW_SETTINGS_CHANGED = 64,
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BACKFORWARD_CACHE_NOT_RESTORED_REASON_H_
