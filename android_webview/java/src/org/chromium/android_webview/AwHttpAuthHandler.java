// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * See {@link android.webkit.HttpAuthHandler}.
 */
@JNINamespace("android_webview")
public class AwHttpAuthHandler {

    private long mNativeAwHttpAuthHandler;
    private final boolean mFirstAttempt;

    public void proceed(String username, String password) {
        if (mNativeAwHttpAuthHandler != 0) {
            AwHttpAuthHandlerJni.get().proceed(
                    mNativeAwHttpAuthHandler, AwHttpAuthHandler.this, username, password);
            mNativeAwHttpAuthHandler = 0;
        }
    }

    public void cancel() {
        if (mNativeAwHttpAuthHandler != 0) {
            AwHttpAuthHandlerJni.get().cancel(mNativeAwHttpAuthHandler, AwHttpAuthHandler.this);
            mNativeAwHttpAuthHandler = 0;
        }
    }

    public boolean isFirstAttempt() {
        return mFirstAttempt;
    }

    @CalledByNative
    public static AwHttpAuthHandler create(long nativeAwAuthHandler, boolean firstAttempt) {
        return new AwHttpAuthHandler(nativeAwAuthHandler, firstAttempt);
    }

    private AwHttpAuthHandler(long nativeAwHttpAuthHandler, boolean firstAttempt) {
        mNativeAwHttpAuthHandler = nativeAwHttpAuthHandler;
        mFirstAttempt = firstAttempt;
    }

    @CalledByNative
    void handlerDestroyed() {
        mNativeAwHttpAuthHandler = 0;
    }

    @NativeMethods
    interface Natives {
        void proceed(long nativeAwHttpAuthHandler, AwHttpAuthHandler caller, String username,
                String password);
        void cancel(long nativeAwHttpAuthHandler, AwHttpAuthHandler caller);
    }
}
