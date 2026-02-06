// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.android_webview.common.Lifetime;

/** For native to pass the information of a WebMessageListener related info to Java. */
@Lifetime.Temporary
@JNINamespace("android_webview")
public class WebMessageListenerInfo {
    public String mObjectName;
    public String[] mAllowedOriginRules;
    public WebMessageListenerHolder mHolder;
    public int mWorldId;

    private WebMessageListenerInfo(
            String objectName,
            String[] allowedOriginRules,
            int worldId,
            WebMessageListenerHolder holder) {
        mObjectName = objectName;
        mAllowedOriginRules = allowedOriginRules;
        mHolder = holder;
        mWorldId = worldId;
    }

    @CalledByNative
    public static WebMessageListenerInfo create(
            @JniType("std::u16string") String objectName,
            @JniType("std::vector<std::string>") String[] allowedOriginRules,
            int worldId,
            WebMessageListenerHolder holder) {
        return new WebMessageListenerInfo(objectName, allowedOriginRules, worldId, holder);
    }
}
