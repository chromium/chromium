// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** See {@link android.webkit.HttpAuthHandler}. */
@JNINamespace("android_webview")
@NullMarked
public class AwHttpAuthHandler {
    @JniType("::android_webview::AwHttpAuthHandler")
    interface NativeAwHttpAuthHandler extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeAwHttpAuthHandler> mNativeAwHttpAuthHandler;
    private final boolean mFirstAttempt;

    public void proceed(String username, String password) {
        checkOnUiThread();
        if (mNativeAwHttpAuthHandler != null) {
            AwHttpAuthHandlerJni.get().proceed(mNativeAwHttpAuthHandler, username, password);
            // Proceed normally deletes the native handler synchronously via handlerDestroyed().
            handlerDestroyed();
        }
    }

    public void cancel() {
        checkOnUiThread();
        if (mNativeAwHttpAuthHandler != null) {
            AwHttpAuthHandlerJni.get().cancel(mNativeAwHttpAuthHandler);
            // Cancel normally deletes the native handler synchronously via handlerDestroyed().
            handlerDestroyed();
        }
    }

    public boolean isFirstAttempt() {
        checkOnUiThread();
        return mFirstAttempt;
    }

    @CalledByNative
    public static AwHttpAuthHandler create(
            @Nullable JniRawPtr<NativeAwHttpAuthHandler> nativeAwAuthHandler,
            boolean firstAttempt) {
        return new AwHttpAuthHandler(nativeAwAuthHandler, firstAttempt);
    }

    private AwHttpAuthHandler(
            @Nullable JniRawPtr<NativeAwHttpAuthHandler> nativeAwHttpAuthHandler,
            boolean firstAttempt) {
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
        if (mNativeAwHttpAuthHandler != null) {
            mNativeAwHttpAuthHandler.release();
            mNativeAwHttpAuthHandler = null;
        }
    }

    @NativeMethods
    interface Natives {
        void proceed(
                JniPtr<NativeAwHttpAuthHandler> self,
                @JniType("std::u16string") String username,
                @JniType("std::u16string") String password);

        void cancel(JniPtr<NativeAwHttpAuthHandler> self);
    }
}
