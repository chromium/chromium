// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_CACHED_FLAGS_H_
#define ANDROID_WEBVIEW_COMMON_AW_CACHED_FLAGS_H_

#include "base/feature_list.h"

namespace android_webview::CachedFlags {

bool IsEnabled(const base::Feature& feature);
bool IsCachedFeatureOverridden(const base::Feature& feature);

}  // namespace android_webview::CachedFlags

#endif  // ANDROID_WEBVIEW_COMMON_AW_CACHED_FLAGS_H_
