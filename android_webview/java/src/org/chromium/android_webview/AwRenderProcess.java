// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 */
@JNINamespace("android_webview")
public final class AwRenderProcess extends AwSupportLibIsomorphic {
    private long mNativeRenderProcess;

    private AwRenderProcess() {}

    public boolean terminate() {
        if (mNativeRenderProcess == 0) return false;

        return AwRenderProcessJni.get().terminateChildProcess(
                mNativeRenderProcess, AwRenderProcess.this);
    }

    @CalledByNative
    private static AwRenderProcess create() {
        return new AwRenderProcess();
    }

    @CalledByNative
    private void setNative(long nativeRenderProcess) {
        mNativeRenderProcess = nativeRenderProcess;
    }

    @NativeMethods
    interface Natives {
        boolean terminateChildProcess(long nativeAwRenderProcess, AwRenderProcess caller);
    }
}
