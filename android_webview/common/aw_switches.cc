// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_switches.h"

namespace switches {

const char kWebViewLogJsConsoleMessages[] = "webview-log-js-console-messages";

const char kWebViewSandboxedRenderer[] = "webview-sandboxed-renderer";

// used to disable safebrowsing functionality in webview
const char kWebViewDisableSafeBrowsingSupport[] =
    "webview-disable-safebrowsing-support";

// Used to enable shared image API for webview.
const char kWebViewEnableSharedImage[] = "webview-enable-shared-image";

// Used to enable vulkan draw mode instead of interop draw mode for webview.
const char kWebViewEnableVulkan[] = "webview-enable-vulkan";

}  // namespace switches
