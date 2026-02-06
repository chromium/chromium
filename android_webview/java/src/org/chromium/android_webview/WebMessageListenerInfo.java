// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.Lifetime;

/** For native to pass the information of a WebMessageListener related info to Java. */
@Lifetime.Temporary
@JNINamespace("android_webview")
public class WebMessageListenerInfo {
    public String mObjectName;
    public String[] mAllowedOriginRules;
    public WebMessageListenerHolder mHolder;

    private WebMessageListenerInfo(
            String objectName, String[] allowedOriginRules, WebMessageListenerHolder holder) {
        mObjectName = objectName;
        mAllowedOriginRules = allowedOriginRules;
        mHolder = holder;
    }

    @CalledByNative
    public static WebMessageListenerInfo create(
            String objectName, String[] allowedOriginRules, WebMessageListenerHolder holder) {
        return new WebMessageListenerInfo(objectName, allowedOriginRules, holder);
    }
}
