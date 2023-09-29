// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * For native to pass the information of a DocumentStartJavascript to Java.
 */
@Lifetime.Temporary
@JNINamespace("android_webview")
public class StartupJavascriptInfo {
    public String mScript;
    public String[] mAllowedOriginRules;

    private StartupJavascriptInfo(String script, String[] allowedOriginRules) {
        mScript = script;
        mAllowedOriginRules = allowedOriginRules;
    }

    @CalledByNative
    public static StartupJavascriptInfo create(String objectName, String[] allowedOriginRules) {
        return new StartupJavascriptInfo(objectName, allowedOriginRules);
    }
}
