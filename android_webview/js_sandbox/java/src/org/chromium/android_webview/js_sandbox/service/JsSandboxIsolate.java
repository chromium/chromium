// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import android.content.res.AssetFileDescriptor;
import android.os.RemoteException;

import androidx.javascriptengine.common.Utils;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxConsoleCallback;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateSyncCallback;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.concurrent.atomic.AtomicReference;

import javax.annotation.concurrent.GuardedBy;

/**
 * Service that provides methods for Javascript execution.
 */
@JNINamespace("android_webview")
public class JsSandboxIsolate extends IJsSandboxIsolate.Stub {
    private static final String TAG = "JsSandboxIsolate";
    // mLock must never be held whilst (synchronously) calling back into the client/embedding
    // application, otherwise it's entirely possible for the embedder to then call back into service
    // code (on another thread) and then try to take mLock again and therefore deadlock.
    private final Object mLock = new Object();
    private final JsSandboxService mService;
    private final AtomicReference<IJsSandboxConsoleCallback> mConsoleCallback =
            new AtomicReference<IJsSandboxConsoleCallback>();
    @GuardedBy("mLock")
    private long mJsSandboxIsolate;

    JsSandboxIsolate(JsSandboxService service) {
        this(service, 0);
    }

    JsSandboxIsolate(JsSandboxService service, long maxHeapSizeBytes) {
        mService = service;
        mJsSandboxIsolate = JsSandboxIsolateJni.get().createNativeJsSandboxIsolateWrapper(
                this, maxHeapSizeBytes);
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

    // Called by isolate thread
    @CalledByNative
    public void consoleMessage(int contextGroupId, int level, String message, String source,
            int line, int column, String trace) {
        final IJsSandboxConsoleCallback callback = mConsoleCallback.get();
        if (callback == null) {
            return;
        }
        final int messageLimit = 32768;
        final int sourceLimit = 4096;
        final int traceLimit = 16384;
        if (message != null && message.length() > messageLimit) {
            message = message.substring(0, messageLimit);
        }
        if (source != null && source.length() > sourceLimit) {
            source = source.substring(0, sourceLimit);
        }
        if (trace != null && trace.length() > traceLimit) {
            trace = trace.substring(0, traceLimit);
        }
        try {
            callback.consoleMessage(contextGroupId, level, message, source, line, column, trace);
        } catch (RemoteException e) {
            Log.e(TAG, "consoleMessage notification failed", e);
        }
    }

    // Called by isolate thread
    @CalledByNative
    public void consoleClear(int contextGroupId) {
        final IJsSandboxConsoleCallback callback = mConsoleCallback.get();
        if (callback == null) {
            return;
        }
        try {
            callback.consoleClear(contextGroupId);
        } catch (RemoteException e) {
            Log.e(TAG, "consoleClear notification failed", e);
        }
    }

    @Override
    public void setConsoleCallback(IJsSandboxConsoleCallback callback) {
        synchronized (mLock) {
            if (mJsSandboxIsolate == 0) {
                throw new IllegalStateException("setConsoleCallback() called after close()");
            }
            mConsoleCallback.set(callback);
            JsSandboxIsolateJni.get().setConsoleEnabled(mJsSandboxIsolate, this, callback != null);
        }
    }

    public static void initializeEnvironment() {
        JsSandboxIsolateJni.get().initializeEnvironment();
    }

    @NativeMethods
    public interface Natives {
        long createNativeJsSandboxIsolateWrapper(
                JsSandboxIsolate jsSandboxIsolate, long maxHeapSizeBytes);

        void initializeEnvironment();

        // The calling code must not call any methods after it called destroyNative().
        void destroyNative(long nativeJsSandboxIsolate, JsSandboxIsolate caller);

        boolean evaluateJavascript(long nativeJsSandboxIsolate, JsSandboxIsolate caller,
                String script, JsSandboxIsolateCallback callback);

        boolean evaluateJavascriptWithFd(long nativeJsSandboxIsolate, JsSandboxIsolate caller,
                int fd, int length, JsSandboxIsolateFdCallback callback);

        boolean provideNamedData(long nativeJsSandboxIsolate, JsSandboxIsolate caller, String name,
                int fd, int length);

        void setConsoleEnabled(
                long nativeJsSandboxIsolate, JsSandboxIsolate caller, boolean enable);
    }
}
