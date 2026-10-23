// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_IS_ANDROID_WEB_VIEW_H_
#define BUILD_IS_ANDROID_WEB_VIEW_H_

namespace build {

// Returns true if the current executable is Android WebView.
// This is resolved at link time, allowing ThinLTO to optimize code paths
// specific to WebView or Chrome.
// Using this function requires approval from //android_webview/OWNERS, see
// https://chromium.googlesource.com/chromium/src/+/main/android_webview/docs/web-platform-compatibility.md#How-do-I-implement-behaviours_options-differently-in-WebView-than-Chrome
bool IsAndroidWebView();

}  // namespace build

#endif  // BUILD_IS_ANDROID_WEB_VIEW_H_
