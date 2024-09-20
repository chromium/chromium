// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;

/** See {@link android.webkit.HttpAuthHandler}. */
@JNINamespace("android_webview")
public class AwHttpAuthHandler {
    private long mNativeAwHttpAuthHandler;
    private final boolean mFirstAttempt;

    public void proceed(String username, String password) {
        checkOnUiThread();
        if (mNativeAwHttpAuthHandler != 0) {
            AwHttpAuthHandlerJni.get()
                    .proceed(mNativeAwHttpAuthHandler, AwHttpAuthHandler.this, username, password);
            mNativeAwHttpAuthHandler = 0;
        }
    }

    public void cancel() {
        checkOnUiThread();
        if (mNativeAwHttpAuthHandler != 0) {
            AwHttpAuthHandlerJni.get().cancel(mNativeAwHttpAuthHandler, AwHttpAuthHandler.this);
            mNativeAwHttpAuthHandler = 0;
        }
    }

    public boolean isFirstAttempt() {
        checkOnUiThread();
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

    private void checkOnUiThread() {
        if (!ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException(
                    "Either proceed(), cancel, or isFirstAttempt() should be called on UI thread");
        }
    }

    @CalledByNative
    void handlerDestroyed() {
        mNativeAwHttpAuthHandler = 0;
    }

    @NativeMethods
    interface Natives {
        void proceed(
                long nativeAwHttpAuthHandler,
                AwHttpAuthHandler caller,
                String username,
                String password);

        void cancel(long nativeAwHttpAuthHandler, AwHttpAuthHandler caller);
    }
}
