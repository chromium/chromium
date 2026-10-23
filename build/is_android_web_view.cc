// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/is_android_web_view.h"

namespace build {

// This is overridden with a strong symbol in //android_webview.
// Avoid using WEAK_SYMBOL from base/compiler_specific.h, so this function
// can be used in //base.
__attribute__((weak)) bool IsAndroidWebView() {
  return false;
}

}  // namespace build
