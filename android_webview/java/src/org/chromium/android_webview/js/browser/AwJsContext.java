// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js.browser;

import android.os.Build;
import android.os.RemoteException;

import org.chromium.android_webview.js.common.ExecutionErrorTypes;
import org.chromium.android_webview.js.common.IJsSandboxContext;
import org.chromium.android_webview.js.common.IJsSandboxContextCallback;
import org.chromium.base.Log;

/** Provides a sandboxed execution context. */
public class AwJsContext implements AutoCloseable {
    private static final String TAG = "AwJsContext";
    private IJsSandboxContext mJsContextStub;
    private android.util.CloseGuard mGuard;

    /** Used to report the results of the JS evaluation. */
    public interface ExecutionCallback {
        void reportResult(String result);

        void reportError(String error);
    }

    AwJsContext(IJsSandboxContext jsContextStub) {
        mJsContextStub = jsContextStub;
        if (Build.VERSION.SDK_INT >= 30) {
            mGuard = new android.util.CloseGuard();
            mGuard.open("close");
        }
        // This should be at the end of the constructor.
    }

    /** Evaluates the Javascript code and calls the callback with the result of the execution. */
    public void evaluateJavascript(String code, ExecutionCallback callback) {
        if (mJsContextStub == null) {
            throw new IllegalStateException(
                    "Calling evaluateJavascript() after closing the context");
        }
        IJsSandboxContextCallback.Stub callbackStub = new IJsSandboxContextCallback.Stub() {
            @Override
            public void reportResult(String result) {
                callback.reportResult(result);
            }

            @Override
            public void reportError(@ExecutionErrorTypes int type, String error) {
                assert type == IJsSandboxContextCallback.JS_EVALUATION_ERROR;
                callback.reportError(error);
            }
        };
        try {
            mJsContextStub.evaluateJavascript(code, callbackStub);
        } catch (RemoteException e) {
            throw e.rethrowAsRuntimeException();
        }
    }

    @Override
    public void close() {
        if (mJsContextStub == null) {
            return;
        }
        try {
            mJsContextStub.close();
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException was thrown during close()", e);
        }
        mJsContextStub = null;
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
            if (mJsContextStub != null) {
                close();
            }
        } finally {
            super.finalize();
        }
    }
}
