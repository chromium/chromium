// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;

/**
 * Java-side representation of the renderer process.
 * Managed and owned by android_webview/browser/aw_render_process.cc
 */
@Lifetime.Renderer
@JNINamespace("android_webview")
public final class AwRenderProcess extends AwSupportLibIsomorphic {
    private long mNativeRenderProcess;

    private AwRenderProcess() {}

    public boolean terminate() {
        if (mNativeRenderProcess == 0) return false;

        return AwRenderProcessJni.get()
                .terminateChildProcess(mNativeRenderProcess, AwRenderProcess.this);
    }

    public boolean isProcessLockedToSiteForTesting() {
        if (mNativeRenderProcess == 0) return false;

        return AwRenderProcessJni.get()
                .isProcessLockedToSiteForTesting(mNativeRenderProcess, AwRenderProcess.this);
    }

    public boolean isReadyForTesting() {
        return mNativeRenderProcess != 0;
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

        boolean isProcessLockedToSiteForTesting(long nativeAwRenderProcess, AwRenderProcess caller);
    }
}
