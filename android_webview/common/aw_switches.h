// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
#define ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_

namespace switches {

extern const char kWebViewLogJsConsoleMessages[];
extern const char kWebViewSandboxedRenderer[];
extern const char kWebViewDisableSafeBrowsingSupport[];
extern const char kWebViewEnableSharedImage[];
extern const char kWebViewEnableVulkan[];

// Please note that if you are adding a flag that is intended for a renderer,
// you also need to add it into
// AwContentBrowserClient::AppendExtraCommandLineSwitches.

}  // namespace switches

#endif  // ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
