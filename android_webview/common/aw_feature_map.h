// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_FEATURE_MAP_H_
#define ANDROID_WEBVIEW_COMMON_AW_FEATURE_MAP_H_

namespace base::android {
class FeatureMap;
}  // namespace base::android

namespace android_webview {

// Returns the FeatureMap for android_webview.
base::android::FeatureMap* GetFeatureMap();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_FEATURE_MAP_H_
