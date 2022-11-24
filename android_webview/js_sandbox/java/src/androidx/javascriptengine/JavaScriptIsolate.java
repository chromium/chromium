// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

import android.content.res.AssetFileDescriptor;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresFeature;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;

import java.io.Closeable;
import java.io.IOException;
import java.io.OutputStream;
import java.util.HashSet;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicInteger;

import javax.annotation.concurrent.GuardedBy;

/**
 * Environment within a {@link JavaScriptSandbox} where Javascript is executed.
 *
 * A single {@link JavaScriptSandbox} process can contain any number of {@link JavaScriptIsolate}
 * instances where JS can be evaluated independently and in parallel.
 * <p>
 * Each isolate has its own state and JS global object,
 * and cannot interact with any other isolate through JS APIs. There is only a <em>moderate</em>
 * security boundary between isolates in a single {@link JavaScriptSandbox}. If the code in one
 * {@link JavaScriptIsolate} is able to compromise the security of the JS engine then it may be
 * able to observe or manipulate other isolates, since they run in the same process. For strong
 * isolation multiple {@link JavaScriptSandbox} processes should be used, but it is not supported
 * at the moment. Please find the feature request <a href="https://crbug.com/1349860">here</a>.
 * <p>
 * Each isolate object must only be used from one thread.
 */
public final class JavaScriptIsolate implements AutoCloseable {
    private static final String TAG = "JavaScriptIsolate";
    private final Object mSetLock = new Object();
    /**
     * Interface to underlying service-backed implementation.
     *
     * mJsIsolateStub should only be null when the Isolate has been explicitly closed - not when the
     * isolate has crashed or simply had its pending and future evaluations cancelled.
     */
    @Nullable
    private IJsSandboxIsolate mJsIsolateStub;
    private CloseGuardHelper mGuard = CloseGuardHelper.create();
    private final ExecutorService mThreadPoolTaskExecutor =
            Executors.newCachedThreadPool(new ThreadFactory() {
                private final AtomicInteger mCount = new AtomicInteger(1);

                @Override
                public Thread newThread(Runnable r) {
                    return new Thread(r, "JavaScriptIsolate Thread #" + mCount.getAndIncrement());
                }
            });
    private final JavaScriptSandbox mJsSandbox;

    @Nullable
    @GuardedBy("mSetLock")
    private HashSet<CallbackToFutureAdapter.Completer<String>> mPendingCompleterSet =
            new HashSet<CallbackToFutureAdapter.Completer<String>>();
    /**
     * If mPendingCompleterSet is null, new evaluations will throw this exception asynchronously.
     *
     * Note that if the isolate is closed, IllegalStateException is thrown synchronously instead.
     */
    @Nullable
    private Exception mExceptionForNewEvaluations;

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
            boolean crashing = false;
            switch (type) {
                case IJsSandboxIsolateCallback.JS_EVALUATION_ERROR:
                    mCompleter.setException(new EvaluationFailedException(error));
                    break;
                case IJsSandboxIsolateCallback.MEMORY_LIMIT_EXCEEDED:
                    mCompleter.setException(new MemoryLimitExceededException(error));
                    crashing = true;
                    break;
                default:
                    mCompleter.setException(new JavaScriptException(
                            "Crashing due to unknown JavaScriptException: " + error));
                    // Assume the worst
                    crashing = true;
            }
            removePending(mCompleter);
            if (crashing) {
                handleCrash();
            }
        }
    }

    JavaScriptIsolate(IJsSandboxIsolate jsIsolateStub, JavaScriptSandbox sandbox) {
        mJsSandbox = sandbox;
        mJsIsolateStub = jsIsolateStub;
        mGuard.open("close");
        // This should be at the end of the constructor.
    }

    /**
     * Evaluates the given JavaScript code and returns the result.
     *
     * There are 3 possible behaviors based on the output of the expression:
     * <ul>
     *   <li><strong>If the JS expression returns a JS String</strong>, then the Java Future
     * resolves to Java String.</li>
     *   <li><strong>If the JS expression returns a JS Promise</strong>,
     * and if {@link JavaScriptSandbox#isFeatureSupported(String)} for
     * {@link JavaScriptSandbox#JS_FEATURE_PROMISE_RETURN} returns {@code true}, Java Future
     * resolves to Java String once the promise resolves. If it returns {@code false}, then the
     * Future resolves to an empty string.</li>
     *   <li><strong>If the JS expression returns another data type</strong>, then Java Future
     * resolves to empty Java String.</li>
     * </ul>
     * The environment uses a single JS global object for all the calls to {@link
     * #evaluateJavaScriptAsync(String)} and {@link #provideNamedData(String, byte[])} methods.
     * These calls are queued up and are run one at a time in sequence, using the single JS
     * environment for the isolate. The global variables set by one evaluation are visible for
     * later evaluations. This is similar to adding multiple {@code <script>} tags in HTML. The
     * behavior is also similar to
     * {@link android.webkit.WebView#evaluateJavascript(String, android.webkit.ValueCallback)}.
     * <p>
     * Size of the expression to be evaluated and the result are both limited by the binder
     * transaction limit. Refer {@link android.os.TransactionTooLargeException} for more details.
     *
     * @param code JavaScript code that is evaluated, it should return a JavaScript String or a
     *         Promise of a String in case {@link JavaScriptSandbox#JS_FEATURE_PROMISE_RETURN} is
     *             supported
     *
     * @return Future that evaluates to the result String of the evaluation or exceptions (see
     *         {@link JavaScriptException} and subclasses) if there is an error
     */
    @SuppressWarnings("NullAway")
    @NonNull
    public ListenableFuture<String> evaluateJavaScriptAsync(@NonNull String code) {
        if (mJsIsolateStub == null) {
            throw new IllegalStateException(
                    "Calling evaluateJavascript() after closing the Isolate");
        }
        return CallbackToFutureAdapter.getFuture(completer -> {
            final String futureDebugMessage = "evaluateJavascript Future";
            IJsSandboxIsolateCallbackStubWrapper callbackStub;
            synchronized (mSetLock) {
                if (mPendingCompleterSet == null) {
                    completer.setException(mExceptionForNewEvaluations);
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

    /**
     * Closes the {@link JavaScriptIsolate} object and renders it unusable.
     *
     * Once closed, no more method calls should be made. Pending evaluations resolve with
     * {@link IsolateTerminatedException} immediately.
     * <p>
     * If {@link JavaScriptSandbox#isFeatureSupported(String)} is {@code true} for {@link
     * JavaScriptSandbox#JS_FEATURE_ISOLATE_TERMINATION}, then any pending evaluation is immediately
     * terminated and memory is freed. If it is {@code false}, the isolate will not get cleaned
     * up until the pending evaluations have run to completion and will consume resources until
     * then.
     */
    @Override
    public void close() {
        // IllegalStateException will be thrown synchronously instead for new evaluations.
        mExceptionForNewEvaluations = null;
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

    /**
     * Provides a byte array for consumption from the JavaScript environment.
     *
     * This method provides an efficient way to pass in data from Java into the JavaScript
     * environment which can be referred to from JavaScript. This is more efficient than including
     * data in the JS expression, and allows large data to be sent.
     * <p>
     * This data can be consumed in the JS environment using {@code
     * android.consumeNamedDataAsArrayBuffer(String)} by referring to the data with the name that
     * was used when calling this method. This is a one-time transfer and the calls should be
     * paired.
     * <p>
     * A single name can only be used once in a particular {@link JavaScriptIsolate}.
     * Clients can generate unique names for each call if they
     * need to use this method multiple times. The same name should be included into the JS code.
     * <p>
     * This API can be used to pass a WASM module into the JS
     * environment for compilation if {@link JavaScriptSandbox#isFeatureSupported(String)} returns
     * {@code true} for {@link JavaScriptSandbox#JS_FEATURE_WASM_COMPILATION}.
     * <br>
     * In Java,
     * <pre>
     *     jsIsolate.provideNamedData("id-1", byteArray);
     * </pre>
     * In JS,
     * <pre>
     *     android.consumeNamedDataAsArrayBuffer("id-1").then((value) => {
     *       return WebAssembly.compile(value).then((module) => {
     *          ...
     *       });
     *     });
     * </pre>
     * <p>
     * The environment uses a single JS global object for all the calls to {@link
     * #evaluateJavaScriptAsync(String)} and {@link #provideNamedData(String, byte[])} methods.
     * <p>
     * This method should only be called if
     * {@link JavaScriptSandbox#isFeatureSupported(String)}
     * returns true for {@link JavaScriptSandbox#JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER}.
     *
     * @param name Identifier for the data that is passed, the same identifier should be used in the
     *         JavaScript environment to refer to the data
     * @param inputBytes Bytes to be passed into the JavaScript environment. This array must not be
     *         modified until the JavaScript promise returned by consumeNamedDataAsArrayBuffer has
     *         resolved (or rejected).
     *
     * @return {@code true} on success, {@code false} if the name has already been used before,
     *         in which case the client should use an unused name
     */
    @RequiresFeature(name = JavaScriptSandbox.JS_FEATURE_PROVIDE_CONSUME_ARRAY_BUFFER,
            enforcement = "androidx.javascriptengine.JavaScriptSandbox#isFeatureSupported")
    public boolean
    provideNamedData(@NonNull String name, @NonNull byte[] inputBytes) {
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
            try {
                OutputStream outputStream =
                        new ParcelFileDescriptor.AutoCloseOutputStream(writeSide);
                mThreadPoolTaskExecutor.execute(
                        () -> { convertByteArrayToStream(inputBytes, outputStream); });

                AssetFileDescriptor asd = new AssetFileDescriptor(readSide, offset, length);
                return mJsIsolateStub.provideNamedData(name, asd);
            } finally {
                // We pass the readSide to the separate sandbox process but we still need to close
                // it on our end to avoid file descriptor leaks.
                readSide.close();
            }
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

    /**
     * Cancel all pending and future evaluations with the given exception.
     *
     * Only the first call to this method has any effect.
     */
    void cancelAllPendingEvaluations(Exception e) {
        final HashSet<CallbackToFutureAdapter.Completer<String>> pendingSet;
        synchronized (mSetLock) {
            if (mPendingCompleterSet == null) return;
            pendingSet = mPendingCompleterSet;
            mPendingCompleterSet = null;
            mExceptionForNewEvaluations = e;
        }
        for (CallbackToFutureAdapter.Completer<String> ele : pendingSet) {
            ele.setException(e);
        }
        // This is the closest thing to a .close() method for ExecutorServices. This doesn't force
        // the threads or their Runnables to immediately terminate, but will ensure that once the
        // worker threads finish their current runnable (if any) that the thread pool terminates
        // them, preventing a leak of threads.
        mThreadPoolTaskExecutor.shutdownNow();
    }

    void removePending(CallbackToFutureAdapter.Completer<String> completer) {
        synchronized (mSetLock) {
            if (mPendingCompleterSet != null) {
                mPendingCompleterSet.remove(completer);
            }
        }
    }

    private void handleCrash() {
        cancelAllPendingEvaluations(new IsolateTerminatedException());
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
    @SuppressWarnings("GenericException") // super.finalize() throws Throwable
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
