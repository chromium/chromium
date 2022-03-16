// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js.renderer;

import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.android_webview.js.common.IJsSandboxContext;
import org.chromium.android_webview.js.common.IJsSandboxContextCallback;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import javax.annotation.concurrent.GuardedBy;

/** Service that provides methods for Javascript execution. */
@JNINamespace("android_webview")
public class JsSandboxContext extends IJsSandboxContext.Stub {
    private static final String TAG = "JsSandboxContext";
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private long mJsSandboxContext;

    JsSandboxContext() {
        mJsSandboxContext = JsSandboxContextJni.get().createNativeJsSandboxIsolateWrapper();
    }

    @Override
    public void evaluateJavascript(String code, IJsSandboxContextCallback callback) {
        synchronized (mLock) {
            if (mJsSandboxContext == 0) {
                throw new IllegalStateException("evaluateJavascript() called after close()");
            }
            if (code.equals("ERROR")) {
                Handler handler = new Handler(Looper.getMainLooper());
                handler.postDelayed(() -> {
                    try {
                        callback.reportError("There has been an error.");
                    } catch (RemoteException e) {
                        Log.e(TAG, "reporting result failed", e);
                    }
                }, 100);
            } else {
                JsSandboxContextJni.get().evaluateJavascript(
                        mJsSandboxContext, this, code, (result) -> {
                            try {
                                callback.reportResult(result);
                            } catch (RemoteException e) {
                                Log.e(TAG, "reporting result failed", e);
                            }
                        });
            }
        }
    }

    @Override
    public void close() {
        synchronized (mLock) {
            if (mJsSandboxContext == 0) {
                return;
            }
            JsSandboxContextJni.get().destroyNative(mJsSandboxContext, this);
            mJsSandboxContext = 0;
        }
    }

    public static void initializeEnvironment() {
        JsSandboxContextJni.get().initializeEnvironment();
    }

    @NativeMethods
    public interface Natives {
        long createNativeJsSandboxIsolateWrapper();

        void initializeEnvironment();

        // The calling code must not call any methods after it called destroyNative().
        void destroyNative(long nativeJsSandboxContext, JsSandboxContext caller);

        boolean evaluateJavascript(long nativeJsSandboxContext, JsSandboxContext caller,
                String script, Callback<String> finishedCallback);
    }
}
