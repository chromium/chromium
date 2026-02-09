// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.js_injection.mojom.DocumentInjectionTime;

/** For native to pass the information of a PersistentJavascript to Java. */
@Lifetime.Temporary
@JNINamespace("android_webview")
@NullMarked
public class PersistentJavascriptInfo {
    public final String mScript;
    public final String[] mAllowedOriginRules;
    public final int mWorldId;
    public final @DocumentInjectionTime.EnumType int mInjectionTime;

    private PersistentJavascriptInfo(
            String script,
            String[] allowedOriginRules,
            int worldId,
            @DocumentInjectionTime.EnumType int injectionTime) {
        mScript = script;
        mAllowedOriginRules = allowedOriginRules;
        mWorldId = worldId;
        mInjectionTime = injectionTime;
    }

    @CalledByNative
    public static PersistentJavascriptInfo create(
            @JniType("std::u16string") String objectName,
            @JniType("std::vector<std::string>") String[] allowedOriginRules,
            int worldId,
            @JniType("js_injection::mojom::DocumentInjectionTime") @DocumentInjectionTime.EnumType
                    int injectionTime) {
        return new PersistentJavascriptInfo(objectName, allowedOriginRules, worldId, injectionTime);
    }
}
