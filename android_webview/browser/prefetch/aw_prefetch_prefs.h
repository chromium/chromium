// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_PREFS_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_PREFS_H_

#include "url/origin.h"

namespace android_webview {

namespace prefs {
// Keys for storing the latest prefetch info (origin and JavaScript enablement)
// in persisted preferences. Can be updated per the
// `AwPrefetchManager::StartRequest()`. Upon app startup, these persisted values
// are passed to `PrePrefetchService::Create()` during `AwPrefetchManager`
// initialization as optimization hints for the likely initial PrePrefetch
// request, avoiding thread hops during PrePrefetch. See
// `PrePrefetchService::Create()` for details. Only used when
// `kWebViewPrefetchOffTheMainThread` is enabled.
inline constexpr char kAwPrefetchLatestOrigin[] = "aw_prefetch.latest_origin";
inline constexpr char kAwPrefetchLatestJavascriptEnabled[] =
    "aw_prefetch.latest_javascript_enabled";
}  // namespace prefs

// A struct to hold the above pref values.
struct AwPrefetchLatestInfoPref {
  AwPrefetchLatestInfoPref(url::Origin origin, bool javascript_enabled);
  ~AwPrefetchLatestInfoPref();
  AwPrefetchLatestInfoPref(const AwPrefetchLatestInfoPref&);
  AwPrefetchLatestInfoPref& operator=(const AwPrefetchLatestInfoPref&);

  bool operator==(const AwPrefetchLatestInfoPref& other) const = default;

  url::Origin origin;
  bool javascript_enabled;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_PREFS_H_
