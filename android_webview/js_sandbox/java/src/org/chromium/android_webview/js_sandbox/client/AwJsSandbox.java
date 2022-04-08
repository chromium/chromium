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

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxService;

/**
 * Sandbox that can execute JS in a safe environment. TODO(crbug.com/1297672): Evaluate the thread
 * safety of this class and enforce ThreadChecker if needed. Refer crrev.com/c/3466074.
 */
public class AwJsSandbox implements AutoCloseable {
    // TODO(crbug.com/1297672): Add capability to this class to support spawning
    // different processes as needed. This might require that we have a static
    // variable in here that tracks the existing services we are connected to and
    // connect to a different one when creating a new object.
    private static final String TAG = "AwJsSandbox";
    private static final String JS_SANDBOX_SERVICE_NAME =
            "org.chromium.android_webview.js_sandbox.service.JsSandboxService0";

    private IJsSandboxService mJsSandboxService;
    private ConnectionSetup mConnection;

    static class ConnectionSetup implements ServiceConnection {
        private ReadyCallback mReadyCallback;
        private AwJsSandbox mAwJsSandbox;
        private Context mContext;

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            IJsSandboxService jsSandboxService = IJsSandboxService.Stub.asInterface(service);
            // We are calling this from the main looper for now.
            mAwJsSandbox = new AwJsSandbox(this, jsSandboxService);
            mReadyCallback.createdConnectedInstance(mAwJsSandbox);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mAwJsSandbox.close();
        }

        ConnectionSetup(Context context, ReadyCallback callback) {
            mContext = context;
            mReadyCallback = callback;
        }
    }

    /** Callback used to inform caller when a connected instance is ready. */
    public interface ReadyCallback {
        void createdConnectedInstance(AwJsSandbox awJsSandbox);
    }

    /**
     * Use this method to create new instances that are connected to the service. The callback is
     * called from the main looper (Looper.getMainLooper()). We only support creation of a single
     * connected instance, we would need to add restrictions to enforce this.
     *
     * @param context When the context is destroyed, the connection will be closed. Use an
     *         application
     *     context if the connection is expected to outlive a single activity/service.
     * @param callback used to pass a callback function on creation of object.
     */
    public static void newConnectedInstance(Context context, ReadyCallback callback) {
        PackageInfo systemWebViewPackage = WebView.getCurrentWebViewPackage();
        ComponentName compName =
                new ComponentName(systemWebViewPackage.packageName, JS_SANDBOX_SERVICE_NAME);
        int flag = Context.BIND_AUTO_CREATE | Context.BIND_EXTERNAL_SERVICE;
        bindToServiceWithCallback(context, compName, flag, callback);
    }

    @VisibleForTesting
    public static void newConnectedInstanceForTesting(Context context, ReadyCallback callback) {
        ComponentName compName = new ComponentName(context, JS_SANDBOX_SERVICE_NAME);
        int flag = Context.BIND_AUTO_CREATE;
        bindToServiceWithCallback(context, compName, flag, callback);
    }

    private static void bindToServiceWithCallback(
            Context context, ComponentName compName, int flag, ReadyCallback callback) {
        Intent intent = new Intent();
        intent.setComponent(compName);
        ConnectionSetup connectionSetup = new ConnectionSetup(context, callback);
        boolean isBinding = context.bindService(intent, connectionSetup, flag);
        if (!isBinding) {
            throw new RuntimeException(
                    "System couldn't find the sandbox service or client doesn't have "
                    + "permission to bind to it " + intent);
        }
    }

    // We prevent direct initialitations of this class. Use AwJsSandbox.newConnectedInstance().
    private AwJsSandbox(ConnectionSetup connectionSetup, IJsSandboxService jsSandboxService) {
        mConnection = connectionSetup;
        mJsSandboxService = jsSandboxService;
    }

    /** Creates an execution isolate within which JS can be executed multiple times. */
    public AwJsIsolate createIsolate() {
        if (mJsSandboxService == null) {
            throw new IllegalStateException(
                    "Attempting to createIsolate on a service that isn't connected");
        }
        try {
            IJsSandboxIsolate isolateStub = mJsSandboxService.createIsolate();
            return new AwJsIsolate(isolateStub);
        } catch (RemoteException e) {
            throw e.rethrowAsRuntimeException();
        }
    }

    @Override
    public void close() {
        if (mJsSandboxService == null) {
            return;
        }
        mConnection.mContext.unbindService(mConnection);
        mJsSandboxService = null;
    }
}
