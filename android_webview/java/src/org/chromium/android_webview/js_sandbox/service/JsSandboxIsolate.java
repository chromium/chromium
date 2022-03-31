// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import android.os.RemoteException;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import javax.annotation.concurrent.GuardedBy;

/** Service that provides methods for Javascript execution. */
@JNINamespace("android_webview")
public class JsSandboxIsolate extends IJsSandboxIsolate.Stub {
    private static final String TAG = "JsSandboxIsolate";
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private long mJsSandboxIsolate;

    JsSandboxIsolate() {
        mJsSandboxIsolate = JsSandboxIsolateJni.get().createNativeJsSandboxIsolateWrapper();
    }

    @Override
    public void evaluateJavascript(String code, IJsSandboxIsolateCallback callback) {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException("evaluateJavascript() called after close()");
            }
            JsSandboxIsolateJni.get().evaluateJavascript(mJsSandboxIsolate, this, code,
                    (result)
                            -> {
                        try {
                            callback.reportResult(result);
                        } catch (RemoteException e) {
                            Log.e(TAG, "reporting result failed", e);
                        }
                    },
                    (error) -> {
                        try {
                            // Currently we only support
                            // IJsSandboxIsolateCallback.JS_EVALUATION_ERROR
                            callback.reportError(
                                    IJsSandboxIsolateCallback.JS_EVALUATION_ERROR, error);
                        } catch (RemoteException e) {
                            Log.e(TAG, "reporting error failed", e);
                        }
                    });
        }
    }

    @Override
    public void close() {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                return;
            }
            JsSandboxIsolateJni.get().destroyNative(mJsSandboxIsolate, this);
            mJsSandboxIsolate = 0;
        }
    }

    public static void initializeEnvironment() {
        JsSandboxIsolateJni.get().initializeEnvironment();
    }

    @NativeMethods
    public interface Natives {
        long createNativeJsSandboxIsolateWrapper();

        void initializeEnvironment();

        // The calling code must not call any methods after it called destroyNative().
        void destroyNative(long nativeJsSandboxIsolate, JsSandboxIsolate caller);

        boolean evaluateJavascript(long nativeJsSandboxIsolate, JsSandboxIsolate caller,
                String script, Callback<String> successCallback, Callback<String> failureCallback);
    }
}
