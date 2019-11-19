// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
#define ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_

#include "base/feature_list.h"

namespace android_webview {
namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// Alphabetical:
extern const base::Feature kWebViewBrotliSupport;
extern const base::Feature kWebViewConnectionlessSafeBrowsing;
extern const base::Feature kWebViewSniffMimeType;
extern const base::Feature kWebViewWakeMetricsService;
extern const base::Feature kWebViewWideColorGamutSupport;

}  // namespace features
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_FEATURES_H_
