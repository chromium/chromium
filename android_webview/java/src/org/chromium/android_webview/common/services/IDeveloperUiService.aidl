// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

interface IDeveloperUiService {
    // Override flag values. |overriddenFlags| is a Map<String, Boolean>, where the boolean value
    // indicates whether to enable or disable the flag named by the string value. Strings can be
    // converted to org.chromium.android_webview.common.Flag via ProductionSupportedFlagList.
    //
    // This may only be called from the developer UI app itself. Calling this from any other context
    // (ex. the embedded WebView implementation) will throw a SecurityException.
    void setFlagOverrides(in Map overriddenFlags);
}
