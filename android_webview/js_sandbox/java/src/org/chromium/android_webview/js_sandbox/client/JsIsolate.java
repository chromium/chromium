// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

import android.content.res.AssetFileDescriptor;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.android_webview.js_sandbox.common.ExecutionErrorTypes;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;

import java.io.Closeable;
import java.io.IOException;
import java.io.OutputStream;
import java.util.HashSet;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicInteger;

import javax.annotation.concurrent.GuardedBy;

/** Provides a sandboxed execution Isolate. This class should be accessed in a single-thread. */
public class JsIsolate implements AutoCloseable {
    private static final String TAG = "JsIsolate";
    private final Object mSetLock = new Object();
    private IJsSandboxIsolate mJsIsolateStub;
    private CloseGuardHelper mGuard = CloseGuardHelper.create();
    private Executor mMainExecutor;
    private final Executor mThreadPoolTaskExecutor =
            Executors.newCachedThreadPool(new ThreadFactory() {
                private final AtomicInteger mCount = new AtomicInteger(1);

                @Override
                public Thread newThread(Runnable r) {
                    return new Thread(r, "JsIsolate Thread #" + mCount.getAndIncrement());
                }
            });
    private final JsSandbox mJsSandbox;

    @GuardedBy("mSetLock")
    private HashSet<CallbackToFutureAdapter.Completer<String>> mPendingCompleterSet =
            new HashSet<CallbackToFutureAdapter.Completer<String>>();

    private class IJsSandboxIsolateCallbackStubWrapper extends IJsSandboxIsolateCallback.Stub {
        private CallbackToFutureAdapter.Completer<String> mCompleter;

        IJsSandboxIsolateCallbackStubWrapper(CallbackToFutureAdapter.Completer<String> completer) {
            mCompleter = completer;
        }

        @Override
        public void reportResult(String result) {
            mCompleter.set(result);
            removePending(mCompleter);
        }

        @Override
        public void reportError(@ExecutionErrorTypes int type, String error) {
            assert type == IJsSandboxIsolateCallback.JS_EVALUATION_ERROR;
            mCompleter.setException(new EvaluationFailedException(error));
            removePending(mCompleter);
        }
    }

    JsIsolate(IJsSandboxIsolate jsIsolateStub, JsSandbox sandbox, Executor executor) {
        mMainExecutor = executor;
        mJsSandbox = sandbox;
        mJsIsolateStub = jsIsolateStub;
        mGuard.open("close");
        // This should be at the end of the constructor.
    }

    /**
     * Evaluates the Javascript code and returns a ListenableFuture for the result of execution.
     * TODO(crbug.com/1297672): Add timeouts for requests.
     */
    @NonNull
    public ListenableFuture<String> evaluateJavascriptAsync(@NonNull String code) {
        if (mJsIsolateStub == null) {
            throw new IllegalStateException(
                    "Calling evaluateJavascript() after closing the Isolate");
        }

        return CallbackToFutureAdapter.getFuture(completer -> {
            final String futureDebugMessage = "evaluateJavascript Future";
            IJsSandboxIsolateCallbackStubWrapper callbackStub;
            synchronized (mSetLock) {
                if (mPendingCompleterSet == null) {
                    completer.setException(new IsolateTerminatedException());
                    return futureDebugMessage;
                }
                mPendingCompleterSet.add(completer);
            }
            callbackStub = new IJsSandboxIsolateCallbackStubWrapper(completer);
            try {
                mJsIsolateStub.evaluateJavascript(code, callbackStub);
            } catch (RemoteException e) {
                completer.setException(new RuntimeException(e));
                synchronized (mSetLock) {
                    mPendingCompleterSet.remove(completer);
                }
            }

            // Debug string.
            return futureDebugMessage;
        });
    }

    @Override
    public void close() {
        if (mJsIsolateStub == null) {
            return;
        }
        try {
            cancelAllPendingEvaluations(new IsolateTerminatedException());
            mJsIsolateStub.close();
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException was thrown during close()", e);
        }
        mJsIsolateStub = null;
        mJsSandbox.removeFromIsolateSet(this);
        mGuard.close();
    }

    public boolean provideNamedData(@NonNull String name, @NonNull byte[] inputBytes) {
        if (mJsIsolateStub == null) {
            throw new IllegalStateException("Calling provideNamedData() after closing the Isolate");
        }
        if (name == null) {
            throw new NullPointerException("name parameter cannot be null");
        }
        try {
            final long offset = 0;
            final long length = inputBytes.length;
            ParcelFileDescriptor[] pipe = ParcelFileDescriptor.createPipe();
            ParcelFileDescriptor readSide = pipe[0];
            ParcelFileDescriptor writeSide = pipe[1];
            OutputStream outputStream = new ParcelFileDescriptor.AutoCloseOutputStream(writeSide);
            mThreadPoolTaskExecutor.execute(
                    () -> { convertByteArrayToStream(inputBytes, outputStream); });

            AssetFileDescriptor asd = new AssetFileDescriptor(readSide, offset, length);
            return mJsIsolateStub.provideNamedData(name, asd);
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException was thrown during provideNamedData()", e);
        } catch (IOException e) {
            Log.e(TAG, "IOException was thrown during provideNamedData", e);
        }
        return false;
    }

    private void convertByteArrayToStream(byte[] inputBytes, OutputStream outputStream) {
        try {
            outputStream.write(inputBytes);
            outputStream.flush();
        } catch (IOException e) {
            Log.e(TAG, "Writing to outputStream failed", e);
        } finally {
            closeQuietly(outputStream);
        }
    }

    void cancelAllPendingEvaluations(Exception e) {
        final HashSet<CallbackToFutureAdapter.Completer<String>> pendingSet;
        synchronized (mSetLock) {
            if (mPendingCompleterSet == null) return;
            pendingSet = mPendingCompleterSet;
            mPendingCompleterSet = null;
        }
        for (CallbackToFutureAdapter.Completer<String> ele : pendingSet) {
            ele.setException(e);
        }
    }

    void removePending(CallbackToFutureAdapter.Completer<String> completer) {
        synchronized (mSetLock) {
            if (mPendingCompleterSet != null) {
                mPendingCompleterSet.remove(completer);
            }
        }
    }

    private static void closeQuietly(Closeable closeable) {
        if (closeable == null) return;
        try {
            closeable.close();
        } catch (IOException ex) {
            // Ignore the exception on close.
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            if (mGuard != null) {
                mGuard.warnIfOpen();
            }
            if (mJsIsolateStub != null) {
                close();
            }
        } finally {
            super.finalize();
        }
    }
}
