// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.view.Choreographer;
import android.view.WindowManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

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
        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        return windowManager.getDefaultDisplay().getRefreshRate();
    }

    @NativeMethods
    interface Natives {
        void onVSync(long nativeAndroidVSyncHelper, AndroidVSyncHelper caller, long frameTimeNanos);
    }
}
