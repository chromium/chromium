// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

import android.os.Build;
import android.os.RemoteException;
import android.util.Log;

import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.android_webview.js_sandbox.common.ExecutionErrorTypes;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;

import java.util.concurrent.Executor;

/** Provides a sandboxed execution Isolate. */
public class AwJsIsolate implements AutoCloseable {
    private static final String TAG = "AwJsIsolate";
    private IJsSandboxIsolate mJsIsolateStub;
    private android.util.CloseGuard mGuard;
    private Executor mExecutor;

    private static class IJsSandboxIsolateCallbackStubWrapper
            extends IJsSandboxIsolateCallback.Stub {
        private CallbackToFutureAdapter.Completer mCompleter;

        IJsSandboxIsolateCallbackStubWrapper(CallbackToFutureAdapter.Completer completer) {
            mCompleter = completer;
        }

        @Override
        public void reportResult(String result) {
            mCompleter.set(result);
        }

        @Override
        public void reportError(@ExecutionErrorTypes int type, String error) {
            assert type == IJsSandboxIsolateCallback.JS_EVALUATION_ERROR;
            mCompleter.setException(new JsEvaluationException(error));
        }
    }

    AwJsIsolate(IJsSandboxIsolate jsIsolateStub, Executor executor) {
        mExecutor = executor;
        mJsIsolateStub = jsIsolateStub;
        if (Build.VERSION.SDK_INT >= 30) {
            mGuard = new android.util.CloseGuard();
            mGuard.open("close");
        }
        // This should be at the end of the constructor.
    }

    /**
     * Evaluates the Javascript code and returns a ListenableFuture for the result of execution.
     * TODO(crbug.com/1297672): Add timeouts for requests.
     */
    public ListenableFuture<String> evaluateJavascript(String code) {
        if (mJsIsolateStub == null) {
            throw new IllegalStateException(
                    "Calling evaluateJavascript() after closing the Isolate");
        }

        return CallbackToFutureAdapter.getFuture(completer -> {
            IJsSandboxIsolateCallbackStubWrapper callbackStub =
                    new IJsSandboxIsolateCallbackStubWrapper(completer);
            try {
                mJsIsolateStub.evaluateJavascript(code, callbackStub);
            } catch (RemoteException e) {
                completer.setException(e.rethrowAsRuntimeException());
            }

            // Debug string.
            return "evaluateJavascript Future";
        });
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
