// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.RemoteException;
import android.webkit.WebView;

import androidx.annotation.VisibleForTesting;
import androidx.concurrent.futures.CallbackToFutureAdapter;
import androidx.core.content.ContextCompat;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxService;

import java.util.List;
import java.util.concurrent.Executor;

import javax.annotation.concurrent.GuardedBy;

/** Sandbox that can execute JS in a safe environment. This class is thread safe. */
public class AwJsSandbox implements AutoCloseable {
    // TODO(crbug.com/1297672): Add capability to this class to support spawning
    // different processes as needed. This might require that we have a static
    // variable in here that tracks the existing services we are connected to and
    // connect to a different one when creating a new object.
    private static final String TAG = "AwJsSandbox";
    private static final String JS_SANDBOX_SERVICE_NAME =
            "org.chromium.android_webview.js_sandbox.service.JsSandboxService0";
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private IJsSandboxService mJsSandboxService;

    private final ConnectionSetup mConnection;

    static class ConnectionSetup implements ServiceConnection {
        private CallbackToFutureAdapter.Completer mCompleter;
        private AwJsSandbox mAwJsSandbox;
        private Context mContext;

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            IJsSandboxService jsSandboxService = IJsSandboxService.Stub.asInterface(service);
            mAwJsSandbox = new AwJsSandbox(this, jsSandboxService);
            mCompleter.set(mAwJsSandbox);
            mCompleter = null;
        }

        // TODO(crbug.com/1297672): We need to track evaluateJavascript requests to fail them when
        // onServiceDisconnected is called.
        // TODO(crbug.com/1297672): We may want an explicit way to signal to the client that the
        // process crashed (like onRenderProcessGone in WebView), without them having to first call
        // one of the methods and have it fail.
        @Override
        public void onServiceDisconnected(ComponentName name) {
            unbindAndSetException(
                    new RuntimeException("AwJsSandbox internal error: onServiceDisconnected()"));
        }

        @Override
        public void onBindingDied(ComponentName name) {
            unbindAndSetException(
                    new RuntimeException("AwJsSandbox internal error: onBindingDead()"));
        }

        @Override
        public void onNullBinding(ComponentName name) {
            unbindAndSetException(
                    new RuntimeException("AwJsSandbox internal error: onNullBinding()"));
        }

        private void unbindAndSetException(Exception e) {
            mContext.unbindService(this);
            if (mCompleter != null) {
                mCompleter.setException(e);
            }
            mCompleter = null;
        }

        ConnectionSetup(Context context, CallbackToFutureAdapter.Completer completer) {
            mContext = context;
            mCompleter = completer;
        }
    }

    /**
     * Use this method to create new instances that are connected to the service. We only support
     * creation of a single connected instance, we would need to add restrictions to enforce this.
     *
     * @param context When the context is destroyed, the connection will be closed. Use an
     *         application
     *     context if the connection is expected to outlive a single activity/service.
     */
    public static ListenableFuture<AwJsSandbox> newConnectedInstance(Context context) {
        PackageInfo systemWebViewPackage = WebView.getCurrentWebViewPackage();
        ComponentName compName =
                new ComponentName(systemWebViewPackage.packageName, JS_SANDBOX_SERVICE_NAME);
        int flag = Context.BIND_AUTO_CREATE | Context.BIND_EXTERNAL_SERVICE;
        return bindToServiceWithCallback(context, compName, flag);
    }

    @VisibleForTesting
    public static ListenableFuture<AwJsSandbox> newConnectedInstanceForTesting(Context context) {
        ComponentName compName = new ComponentName(context, JS_SANDBOX_SERVICE_NAME);
        int flag = Context.BIND_AUTO_CREATE;
        return bindToServiceWithCallback(context, compName, flag);
    }

    private static ListenableFuture<AwJsSandbox> bindToServiceWithCallback(
            Context context, ComponentName compName, int flag) {
        Intent intent = new Intent();
        intent.setComponent(compName);
        return CallbackToFutureAdapter.getFuture(completer -> {
            ConnectionSetup connectionSetup = new ConnectionSetup(context, completer);
            try {
                boolean isBinding = context.bindService(intent, connectionSetup, flag);
                if (isBinding) {
                    Executor mainExecutor;
                    if (Build.VERSION.SDK_INT >= 28) {
                        mainExecutor = context.getMainExecutor();
                    } else {
                        mainExecutor = ContextCompat.getMainExecutor(context);
                    }
                    completer.addCancellationListener(
                            () -> context.unbindService(connectionSetup), mainExecutor);
                } else {
                    context.unbindService(connectionSetup);
                    completer.setException(
                            new RuntimeException("bindService() returned false " + intent));
                }
            } catch (SecurityException e) {
                context.unbindService(connectionSetup);
                completer.setException(e);
            }

            // Debug string.
            return "AwJsSandbox Future";
        });
    }

    // We prevent direct initializations of this class. Use AwJsSandbox.newConnectedInstance().
    private AwJsSandbox(ConnectionSetup connectionSetup, IJsSandboxService jsSandboxService) {
        mConnection = connectionSetup;
        mJsSandboxService = jsSandboxService;
    }

    /** Creates an execution isolate within which JS can be executed multiple times. */
    public AwJsIsolate createIsolate() {
        synchronized (mLock) {
            if (mJsSandboxService == null) {
                throw new IllegalStateException(
                        "Attempting to createIsolate on a service that isn't connected");
            }
            try {
                IJsSandboxIsolate isolateStub = mJsSandboxService.createIsolate();
                Executor mainExecutor;
                if (Build.VERSION.SDK_INT >= 28) {
                    mainExecutor = mConnection.mContext.getMainExecutor();
                } else {
                    mainExecutor = ContextCompat.getMainExecutor(mConnection.mContext);
                }
                return new AwJsIsolate(isolateStub, mainExecutor);
            } catch (RemoteException e) {
                throw e.rethrowAsRuntimeException();
            }
        }
    }

    /**
     * Quick temporary feature detection interface for testing the IPC.
     * TODO(crbug.com/1297672): make parameterized and cached.
     */
    public boolean isIsolateTerminationSupported() {
        synchronized (mLock) {
            if (mJsSandboxService == null) {
                throw new IllegalStateException(
                        "Attempting to check features on a service that isn't connected");
            }
            try {
                List<String> features = mJsSandboxService.getSupportedFeatures();
                return features.contains(IJsSandboxService.ISOLATE_TERMINATION);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        }
    }

    @Override
    public void close() {
        synchronized (mLock) {
            if (mJsSandboxService == null) {
                return;
            }
            mConnection.mContext.unbindService(mConnection);
            mJsSandboxService = null;
        }
    }
}
