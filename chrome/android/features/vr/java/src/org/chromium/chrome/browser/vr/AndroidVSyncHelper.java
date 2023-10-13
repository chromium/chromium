// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.view.Choreographer;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.ui.display.DisplayAndroidManager;

/**
 * Helper class for interfacing with the Android Choreographer from native code.
 */
@JNINamespace("vr")
public class AndroidVSyncHelper {
    private final long mNativeAndroidVSyncHelper;

    private final Choreographer.FrameCallback mCallback = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long frameTimeNanos) {
            if (mNativeAndroidVSyncHelper == 0) return;
            AndroidVSyncHelperJni.get().onVSync(
                    mNativeAndroidVSyncHelper, AndroidVSyncHelper.this, frameTimeNanos);
        }
    };

    @CalledByNative
    private static AndroidVSyncHelper create(long nativeAndroidVSyncHelper) {
        return new AndroidVSyncHelper(nativeAndroidVSyncHelper);
    }

    private AndroidVSyncHelper(long nativeAndroidVSyncHelper) {
        mNativeAndroidVSyncHelper = nativeAndroidVSyncHelper;
    }

    @CalledByNative
    private void requestVSync() {
        Choreographer.getInstance().postFrameCallback(mCallback);
    }

    @CalledByNative
    private void cancelVSyncRequest() {
        Choreographer.getInstance().removeFrameCallback(mCallback);
    }

    @CalledByNative
    private float getRefreshRate() {
        Context context = ContextUtils.getApplicationContext();
        return DisplayAndroidManager.getDefaultDisplayForContext(context).getRefreshRate();
    }

    @NativeMethods
    interface Natives {
        void onVSync(long nativeAndroidVSyncHelper, AndroidVSyncHelper caller, long frameTimeNanos);
    }
}
