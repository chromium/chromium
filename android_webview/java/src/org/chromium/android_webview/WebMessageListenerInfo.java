// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * For native to pass the information of a WebMessageListener related info to Java.
 */
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
