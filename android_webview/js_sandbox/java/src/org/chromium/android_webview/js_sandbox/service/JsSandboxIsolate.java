// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import android.content.res.AssetFileDescriptor;
import android.os.RemoteException;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;
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
        mJsSandboxIsolate = JsSandboxIsolateJni.get().createNativeJsSandboxIsolateWrapper(0);
    }

    JsSandboxIsolate(long maxHeapSizeBytes) {
        mJsSandboxIsolate =
                JsSandboxIsolateJni.get().createNativeJsSandboxIsolateWrapper(maxHeapSizeBytes);
    }

    @Override
    public void evaluateJavascript(String code, IJsSandboxIsolateCallback callback) {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException("evaluateJavascript() called after close()");
            }
            JsSandboxIsolateJni.get().evaluateJavascript(
                    mJsSandboxIsolate, this, code, new JsSandboxIsolateCallback() {
                        @Override
                        public void onResult(String result) {
                            try {
                                callback.reportResult(result);
                            } catch (RemoteException e) {
                                Log.e(TAG, "reporting result failed", e);
                            }
                        }

                        @Override
                        public void onError(int errorType, String error) {
                            try {
                                callback.reportError(errorType, error);
                            } catch (RemoteException e) {
                                Log.e(TAG, "reporting error failed", e);
                            }
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

    @Override
    public boolean provideNamedData(String name, AssetFileDescriptor afd) {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException(
                        "provideNamedData(String, AssetFileDescriptor) called after close()");
            }
            if (afd.getStartOffset() != 0) {
                throw new UnsupportedOperationException(
                        "AssetFileDescriptor.getStartOffset() != 0");
            }
            if (afd.getLength() < 0) {
                throw new UnsupportedOperationException(
                        "AssetFileDescriptor.getLength() should be >=0");
            }
            if (afd.getLength() > Integer.MAX_VALUE) {
                throw new IllegalArgumentException(
                        "AssetFileDescriptor.getLength() should be < 2^31");
            }
            boolean nativeReturn = JsSandboxIsolateJni.get().provideNamedData(mJsSandboxIsolate,
                    this, name, afd.getParcelFileDescriptor().detachFd(), (int) afd.getLength());
            return nativeReturn;
        }
    }

    public static void initializeEnvironment() {
        JsSandboxIsolateJni.get().initializeEnvironment();
    }

    @NativeMethods
    public interface Natives {
        long createNativeJsSandboxIsolateWrapper(long maxHeapSizeBytes);

        void initializeEnvironment();

        // The calling code must not call any methods after it called destroyNative().
        void destroyNative(long nativeJsSandboxIsolate, JsSandboxIsolate caller);

        boolean evaluateJavascript(long nativeJsSandboxIsolate, JsSandboxIsolate caller,
                String script, JsSandboxIsolateCallback callback);

        boolean provideNamedData(long nativeJsSandboxIsolate, JsSandboxIsolate caller, String name,
                int fd, int length);
    }
}
