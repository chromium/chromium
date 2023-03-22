// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import android.content.res.AssetFileDescriptor;

import androidx.javascriptengine.common.Utils;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateSyncCallback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import javax.annotation.concurrent.GuardedBy;

/**
 * Service that provides methods for Javascript execution.
 */
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
                    mJsSandboxIsolate, this, code, new JsSandboxIsolateCallback(callback));
        }
    }

    @Override
    public void evaluateJavascriptWithFd(
            AssetFileDescriptor afd, IJsSandboxIsolateSyncCallback callback) {
        synchronized (mLock) {
            String code;
            Utils.checkAssetFileDescriptor(afd, Integer.MAX_VALUE);
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException("evaluateJavascript() called after close()");
            }
            JsSandboxIsolateJni.get().evaluateJavascriptWithFd(mJsSandboxIsolate, this,
                    afd.getParcelFileDescriptor().detachFd(), (int) afd.getLength(),
                    new JsSandboxIsolateFdCallback(callback));
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
            Utils.checkAssetFileDescriptor(afd, Integer.MAX_VALUE);
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

        boolean evaluateJavascriptWithFd(long nativeJsSandboxIsolate, JsSandboxIsolate caller,
                int fd, int length, JsSandboxIsolateFdCallback callback);

        boolean provideNamedData(long nativeJsSandboxIsolate, JsSandboxIsolate caller, String name,
                int fd, int length);
    }
}
