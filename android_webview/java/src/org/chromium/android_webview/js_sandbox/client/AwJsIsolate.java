// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

import android.os.Build;
import android.os.RemoteException;
import android.util.Log;

import org.chromium.android_webview.js_sandbox.common.ExecutionErrorTypes;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;

/** Provides a sandboxed execution Isolate. */
public class AwJsIsolate implements AutoCloseable {
    private static final String TAG = "AwJsIsolate";
    private IJsSandboxIsolate mJsIsolateStub;
    private android.util.CloseGuard mGuard;

    /** Used to report the results of the JS evaluation. */
    public interface ExecutionCallback {
        void reportResult(String result);

        void reportError(String error);
    }

    AwJsIsolate(IJsSandboxIsolate jsIsolateStub) {
        mJsIsolateStub = jsIsolateStub;
        if (Build.VERSION.SDK_INT >= 30) {
            mGuard = new android.util.CloseGuard();
            mGuard.open("close");
        }
        // This should be at the end of the constructor.
    }

    /** Evaluates the Javascript code and calls the callback with the result of the execution. */
    public void evaluateJavascript(String code, ExecutionCallback callback) {
        if (mJsIsolateStub == null) {
            throw new IllegalStateException(
                    "Calling evaluateJavascript() after closing the Isolate");
        }
        IJsSandboxIsolateCallback.Stub callbackStub = new IJsSandboxIsolateCallback.Stub() {
            @Override
            public void reportResult(String result) {
                callback.reportResult(result);
            }

            @Override
            public void reportError(@ExecutionErrorTypes int type, String error) {
                assert type == IJsSandboxIsolateCallback.JS_EVALUATION_ERROR;
                callback.reportError(error);
            }
        };
        try {
            mJsIsolateStub.evaluateJavascript(code, callbackStub);
        } catch (RemoteException e) {
            throw e.rethrowAsRuntimeException();
        }
    }

    @Override
    public void close() {
        if (mJsIsolateStub == null) {
            return;
        }
        try {
            mJsIsolateStub.close();
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException was thrown during close()", e);
        }
        mJsIsolateStub = null;
        if (Build.VERSION.SDK_INT >= 30) {
            mGuard.close();
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            if (Build.VERSION.SDK_INT >= 30) {
                if (mGuard != null) {
                    mGuard.warnIfOpen();
                }
            }
            if (mJsIsolateStub != null) {
                close();
            }
        } finally {
            super.finalize();
        }
    }
}
