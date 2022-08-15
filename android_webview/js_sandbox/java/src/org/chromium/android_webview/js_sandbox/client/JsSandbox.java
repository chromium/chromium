// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageInfo;
import android.os.IBinder;
import android.os.RemoteException;
import android.webkit.WebView;

import androidx.annotation.NonNull;
import androidx.annotation.RestrictTo;
import androidx.annotation.StringDef;
import androidx.annotation.VisibleForTesting;
import androidx.concurrent.futures.CallbackToFutureAdapter;
import androidx.core.content.ContextCompat;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxService;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;

import javax.annotation.concurrent.GuardedBy;

/**
 * Sandbox that provides APIs for JavaScript evaluation in a restricted environment.
 *
 * JsSandbox represents a connection to an isolated process. The isolated process will be exclusive
 * to the calling app (i.e. it doesn't share anything with, and can't be compromised by another
 * app's isolated process).
 * <p>
 * Code that is run in a sandbox does not have any access to data
 * belonging to the original app unless explicitly passed into it by using the methods of this
 * class. This provides a security boundary between the calling app and the Javascript execution
 * environment.
 * <p>
 * The calling app can only have only one isolated process at a time, so only one
 * instance of this object can exist at a time.
 * <p>
 * It's safe to share a single {@link JsSandbox}
 * object with multiple threads and use it from multiple threads at once.
 * For example, {@link JsSandbox} can be stored at a global location and multiple threads can create
 * their own {@link JsIsolate} objects from it but the {@link JsIsolate} object cannot be shared.
 */
public class JsSandbox implements AutoCloseable {
    // TODO(crbug.com/1297672): Add capability to this class to support spawning
    // different processes as needed. This might require that we have a static
    // variable in here that tracks the existing services we are connected to and
    // connect to a different one when creating a new object.
    private static final String TAG = "JsSandbox";
    private static final String JS_SANDBOX_SERVICE_NAME =
            "org.chromium.android_webview.js_sandbox.service.JsSandboxService0";
    private static AtomicBoolean sIsReadyToConnect = new AtomicBoolean(true);
    private final Object mLock = new Object();
    private CloseGuardHelper mGuard = CloseGuardHelper.create();

    @GuardedBy("mLock")
    private IJsSandboxService mJsSandboxService;

    private final ConnectionSetup mConnection;

    @GuardedBy("mLock")
    private HashSet<JsIsolate> mActiveIsolateSet = new HashSet<JsIsolate>();

    /**
     * @hide
     */
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @StringDef(value =
                       {
                               ISOLATE_TERMINATION,
                               PROMISE_RETURN,
                               PROVIDE_CONSUME_ARRAY_BUFFER,
                               WASM_COMPILATION,
                       })
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.PARAMETER, ElementType.METHOD})
    public @interface JsSandboxFeature {}

    /**
     * Feature for {@link #isFeatureSupported(String)}.
     *
     * When this
     * feature is present, {@link JsIsolate#close()} will terminate the currently running JS
     * evaluation and close the isolate. If it is absent, {@link JsIsolate#close()} cannot terminate
     * any running or queued evaluations in the background,
     * so the isolate will continue to consume resources until they complete.
     * <p>
     * Irrespective of this feature, calling {@link JsSandbox#close()} will terminate all
     * {@link JsIsolate} objects (and the isolated process) immediately and all pending
     * {@link JsIsolate#evaluateJavascriptAsync(String)} futures will resolve with {@link
     * IsolateTerminatedException}.
     */
    public static final String ISOLATE_TERMINATION = "ISOLATE_TERMINATION";

    /**
     * Feature for {@link #isFeatureSupported(String)}.
     *
     * When this feature is present, JS expressions may return promises. The Future returned by
     * {@link JsIsolate#evaluateJavascriptAsync(String)} will resolve to the promise's result,
     * once the promise resolves.
     */
    public static final String PROMISE_RETURN = "PROMISE_RETURN";

    /**
     * Feature for {@link #isFeatureSupported(String)}.
     * When this feature is present, {@link JsIsolate#provideNamedData(String, byte[])} can be used.
     * <p>
     * This also covers the JS API android.consumeNamedDataAsArrayBuffer(string).
     */
    public static final String PROVIDE_CONSUME_ARRAY_BUFFER = "PROVIDE_CONSUME_ARRAY_BUFFER";

    /**
     * Feature for {@link #isFeatureSupported(String)}.
     *
     * This features provides additional behaviour to {@link
     * JsIsolate#evaluateJavascriptAsync(String)} ()}. When this feature is present, the JS API
     * WebAssembly.compile(ArrayBuffer) can be used.
     */
    public static final String WASM_COMPILATION = "WASM_COMPILATION";

    /**
     * Feature for {@link #isFeatureSupported(String)}.
     *
     * When this feature is present,
     * {@link JsSandbox#createIsolate(IsolateSettings)} can be used.
     */
    public static final String ISOLATE_MAX_HEAP_SIZE = "ISOLATE_MAX_HEAP_SIZE";

    private HashSet<String> mClientSideFeatureSet;

    static class ConnectionSetup implements ServiceConnection {
        private CallbackToFutureAdapter.Completer<JsSandbox> mCompleter;
        private JsSandbox mJsSandbox;
        Context mContext;

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            IJsSandboxService jsSandboxService = IJsSandboxService.Stub.asInterface(service);
            mJsSandbox = new JsSandbox(this, jsSandboxService);
            mCompleter.set(mJsSandbox);
            mCompleter = null;
        }

        // TODO(crbug.com/1297672): We may want an explicit way to signal to the client that the
        // process crashed (like onRenderProcessGone in WebView), without them having to first call
        // one of the methods and have it fail.
        @Override
        public void onServiceDisconnected(ComponentName name) {
            runShutdownTasks(
                    new RuntimeException("JsSandbox internal error: onServiceDisconnected()"));
        }

        @Override
        public void onBindingDied(ComponentName name) {
            runShutdownTasks(new RuntimeException("JsSandbox internal error: onBindingDead()"));
        }

        @Override
        public void onNullBinding(ComponentName name) {
            runShutdownTasks(new RuntimeException("JsSandbox internal error: onNullBinding()"));
        }

        private void runShutdownTasks(Exception e) {
            if (mJsSandbox != null) {
                mJsSandbox.doClose(new SandboxDeadException());
            } else {
                mContext.unbindService(this);
                sIsReadyToConnect.set(true);
            }
            if (mCompleter != null) {
                mCompleter.setException(e);
            }
            mCompleter = null;
        }

        ConnectionSetup(Context context, CallbackToFutureAdapter.Completer<JsSandbox> completer) {
            mContext = context;
            mCompleter = completer;
        }
    }

    /**
     * Asynchronously create and connect to the sandbox process.
     *
     * Only one sandbox process can exist at a time. Attempting to create a new instance before
     * the previous instance has been closed will fail with an {@link IllegalStateException}.
     *
     * @param context When the context is destroyed, the connection will be closed. Use an
     *         application
     *     context if the connection is expected to outlive a single activity/service.
     *
     * @return Future that evaluates to a connected {@link JsSandbox} instance or an exception if
     *     binding to service fails.
     */
    @NonNull
    public static ListenableFuture<JsSandbox> newConnectedInstanceAsync(@NonNull Context context) {
        PackageInfo systemWebViewPackage = WebView.getCurrentWebViewPackage();
        ComponentName compName =
                new ComponentName(systemWebViewPackage.packageName, JS_SANDBOX_SERVICE_NAME);
        int flag = Context.BIND_AUTO_CREATE | Context.BIND_EXTERNAL_SERVICE;
        return bindToServiceWithCallback(context, compName, flag);
    }

    @NonNull
    @VisibleForTesting
    public static ListenableFuture<JsSandbox> newConnectedInstanceForTestingAsync(
            @NonNull Context context) {
        ComponentName compName = new ComponentName(context, JS_SANDBOX_SERVICE_NAME);
        int flag = Context.BIND_AUTO_CREATE;
        return bindToServiceWithCallback(context, compName, flag);
    }

    @NonNull
    private static ListenableFuture<JsSandbox> bindToServiceWithCallback(
            Context context, ComponentName compName, int flag) {
        Intent intent = new Intent();
        intent.setComponent(compName);
        return CallbackToFutureAdapter.getFuture(completer -> {
            ConnectionSetup connectionSetup = new ConnectionSetup(context, completer);
            if (sIsReadyToConnect.compareAndSet(true, false)) {
                try {
                    boolean isBinding = context.bindService(intent, connectionSetup, flag);
                    if (isBinding) {
                        Executor mainExecutor;
                        mainExecutor = ContextCompat.getMainExecutor(context);
                        completer.addCancellationListener(
                                () -> { context.unbindService(connectionSetup); }, mainExecutor);
                    } else {
                        context.unbindService(connectionSetup);
                        completer.setException(
                                new RuntimeException("bindService() returned false " + intent));
                    }
                } catch (SecurityException e) {
                    context.unbindService(connectionSetup);
                    completer.setException(e);
                }
            } else {
                completer.setException(
                        new IllegalStateException("Binding to already bound service"));
            }

            // Debug string.
            return "JsSandbox Future";
        });
    }

    // We prevent direct initializations of this class. Use JsSandbox.newConnectedInstance().
    JsSandbox(ConnectionSetup connectionSetup, IJsSandboxService jsSandboxService) {
        mConnection = connectionSetup;
        mJsSandboxService = jsSandboxService;
        mGuard.open("close");
        // This should be at the end of the constructor.
    }

    /**
     * Creates and returns an {@link JsIsolate} within which JS can be executed with default
     * settings.
     */
    @NonNull
    public JsIsolate createIsolate() {
        synchronized (mLock) {
            if (mJsSandboxService == null) {
                throw new IllegalStateException(
                        "Attempting to createIsolate on a service that isn't connected");
            }
            IJsSandboxIsolate isolateStub;
            try {
                isolateStub = mJsSandboxService.createIsolate();
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
            return createJsIsolateLocked(isolateStub);
        }
    }

    /**
     * Creates and returns an {@link JsIsolate} within which JS can be executed with the specified
     * settings.
     *
     * @param settings configuration used to set up the isolate
     */
    @NonNull
    public JsIsolate createIsolate(IsolateSettings settings) {
        synchronized (mLock) {
            if (mJsSandboxService == null) {
                throw new IllegalStateException(
                        "Attempting to createIsolate on a service that isn't connected");
            }
            IJsSandboxIsolate isolateStub;
            try {
                if (settings.getMaxHeapSizeBytes() == 0) {
                    isolateStub = mJsSandboxService.createIsolate();
                } else {
                    isolateStub = mJsSandboxService.createIsolateWithMaxHeapSizeBytes(
                            settings.getMaxHeapSizeBytes());
                    if (isolateStub == null) {
                        throw new RuntimeException(
                                "Service implementation doesn't support setting maximum heap size");
                    }
                }
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
            return createJsIsolateLocked(isolateStub);
        }
    }

    @GuardedBy("mLock")
    private void populateClientFeatureSet() {
        mClientSideFeatureSet = new HashSet<String>();
        List<String> features;
        try {
            features = mJsSandboxService.getSupportedFeatures();
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }

        if (features.contains(IJsSandboxService.ISOLATE_TERMINATION)) {
            mClientSideFeatureSet.add(ISOLATE_TERMINATION);
        }
        if (features.contains(IJsSandboxService.WASM_FROM_ARRAY_BUFFER)) {
            mClientSideFeatureSet.add(PROMISE_RETURN);
            mClientSideFeatureSet.add(PROVIDE_CONSUME_ARRAY_BUFFER);
            mClientSideFeatureSet.add(WASM_COMPILATION);
        }
        if (features.contains(IJsSandboxService.ISOLATE_MAX_HEAP_SIZE_LIMIT)) {
            mClientSideFeatureSet.add(ISOLATE_MAX_HEAP_SIZE);
        }
    }

    @GuardedBy("mLock")
    private JsIsolate createJsIsolateLocked(IJsSandboxIsolate isolateStub) {
        Executor mainExecutor;
        mainExecutor = ContextCompat.getMainExecutor(mConnection.mContext);
        JsIsolate isolate = new JsIsolate(isolateStub, this, mainExecutor);
        mActiveIsolateSet.add(isolate);
        return isolate;
    }

    /**
     * Checks whether a given feature is supported by the JS Sandbox implementation.
     *
     * The sandbox implementation is provided by the version of WebView installed on the device.
     * The app must use this method to check which library features are supported by the device's
     * implementation before using them.
     * <p>
     * A feature check should be made prior to depending
     * on features enlisted in {@link JsSandboxFeature}.
     *
     * @param feature feature to be checked
     *
     * @return {@code true} if supported, {@code false} otherwise
     */
    public boolean isFeatureSupported(@NonNull @JsSandboxFeature String feature) {
        synchronized (mLock) {
            if (mJsSandboxService == null) {
                throw new IllegalStateException(
                        "Attempting to check features on a service that isn't connected");
            }
            if (mClientSideFeatureSet == null) {
                populateClientFeatureSet();
            }
            return mClientSideFeatureSet.contains(feature);
        }
    }

    void removeFromIsolateSet(JsIsolate isolate) {
        synchronized (mLock) {
            if (mActiveIsolateSet != null) {
                mActiveIsolateSet.remove(isolate);
            }
        }
    }

    /**
     * Closes the {@link JsSandbox} object and renders it unusable.
     *
     * The client is expected to call this method explicitly to terminate the isolated process.
     * <p>
     * Once closed, no more {@link JsSandbox} and {@link JsIsolate} method calls can be made.
     * Closing will terminate the isolated process immediately. All pending evaluations are
     * immediately terminated. Once closed, the client may call {@link
     * JsSandbox#newConnectedInstanceAsync(Context)} to create another {@link JsSandbox}.
     */
    @Override
    public void close() {
        doClose(new IsolateTerminatedException());
    }

    private void doClose(Exception cancelPendingWith) {
        synchronized (mLock) {
            if (mJsSandboxService == null) {
                return;
            }
            cancelPendingEvaluationsLocked(cancelPendingWith);
            mConnection.mContext.unbindService(mConnection);
            // Currently we consider that we are ready for a new connection once we unbind. This
            // might not be true if the process is not immediately killed by ActivityManager once it
            // is unbound.
            sIsReadyToConnect.set(true);
            mJsSandboxService = null;
        }
    }

    @GuardedBy("mLock")
    private void cancelPendingEvaluationsLocked(Exception e) {
        for (JsIsolate ele : mActiveIsolateSet) {
            ele.cancelAllPendingEvaluations(e);
        }
        mActiveIsolateSet = null;
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            if (mGuard != null) {
                mGuard.warnIfOpen();
            }
            synchronized (mLock) {
                if (mJsSandboxService != null) {
                    close();
                }
            }
        } finally {
            super.finalize();
        }
    }
}
