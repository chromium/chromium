// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_VARIATIONS_SEED_BRIDGE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_VARIATIONS_SEED_BRIDGE_H_

#include <memory>
#include <string>

#include "components/variations/seed_response.h"

namespace android_webview {

// If the Java side has a seed, return it and clear it from the Java side.
// Otherwise, return null.
std::unique_ptr<variations::SeedResponse> GetAndClearJavaSeed();

// Returns true if the variations seed that was loaded is fresh.
bool IsSeedFresh();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_VARIATIONS_SEED_BRIDGE_H_
