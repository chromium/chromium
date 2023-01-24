// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

@JNINamespace("android_webview")
class AwOriginVerificationSchedulerBridge {
    @CalledByNative
    static void verify(String url, long nativeCallbackPtr) {
        AwOriginVerificationScheduler.getInstance().verify(url, (verified) -> {
            AwOriginVerificationSchedulerBridgeJni.get().onVerificationResult(
                    nativeCallbackPtr, verified);
        });
    }

    @NativeMethods
    interface Natives {
        void onVerificationResult(long callbackPtr, boolean verified);
    }
}
