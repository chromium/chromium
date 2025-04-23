// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;

/** For native to pass the information of a DocumentStartJavascript to Java. */
@Lifetime.Temporary
@JNINamespace("android_webview")
@NullMarked
public class StartupJavascriptInfo {
    public final String mScript;
    public final String[] mAllowedOriginRules;

    private StartupJavascriptInfo(String script, String[] allowedOriginRules) {
        mScript = script;
        mAllowedOriginRules = allowedOriginRules;
    }

    @CalledByNative
    public static StartupJavascriptInfo create(String objectName, String[] allowedOriginRules) {
        return new StartupJavascriptInfo(objectName, allowedOriginRules);
    }
}
