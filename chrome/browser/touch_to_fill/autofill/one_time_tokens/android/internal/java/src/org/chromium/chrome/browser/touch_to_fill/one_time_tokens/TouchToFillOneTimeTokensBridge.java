// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.one_time_tokens;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/**
 * The JNI bridge for the one-time tokens Touch-To-Fill component. This class is instantiated by the
 * C++ side and acts as a client to the Java component.
 */
@NullMarked
class TouchToFillOneTimeTokensBridge {
    private long mNativeBridge;
    private final WindowAndroid mWindowAndroid;

    TouchToFillOneTimeTokensBridge(WindowAndroid windowAndroid, long nativeBridge) {
        mNativeBridge = nativeBridge;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    private static TouchToFillOneTimeTokensBridge create(
            WindowAndroid windowAndroid, long nativeBridge) {
        return new TouchToFillOneTimeTokensBridge(windowAndroid, nativeBridge);
    }

    @CalledByNative
    public boolean show(String token) {
        Context context = mWindowAndroid.getContext().get();
        if (context == null) return false;
        // TODO(crbug.com/444409226): Create and show the bottom sheet UI.
        return true;
    }

    public void onDismissed(boolean tokenAccepted) {
        if (mNativeBridge == 0) return;

        TouchToFillOneTimeTokensBridgeJni.get().onDismissed(mNativeBridge, tokenAccepted);
        mNativeBridge = 0;
    }

    public void onTokenAccepted(String token) {
        if (mNativeBridge == 0) return;

        TouchToFillOneTimeTokensBridgeJni.get().onTokenAccepted(mNativeBridge, token);
    }

    public void onTokenRejected() {
        if (mNativeBridge == 0) return;

        TouchToFillOneTimeTokensBridgeJni.get().onTokenRejected(mNativeBridge);
    }

    @CalledByNative
    public void hide() {
        mNativeBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeTouchToFillOneTimeTokensBridgeImpl, boolean tokenAccepted);

        void onTokenAccepted(long nativeTouchToFillOneTimeTokensBridgeImpl, String token);

        void onTokenRejected(long nativeTouchToFillOneTimeTokensBridgeImpl);
    }
}
