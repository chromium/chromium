// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js.browser;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.android_webview.js.common.IJsSandboxContext;
import org.chromium.android_webview.js.common.IJsSandboxService;
import org.chromium.android_webview.js.renderer.JsSandboxService0;
import org.chromium.base.ContextUtils;

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

    private IJsSandboxService mJsSandboxService;
    private ConnectionSetup mConnection;

    static class ConnectionSetup implements ServiceConnection {
        private ReadyCallback mReadyCallback;
        private AwJsSandbox mAwJsSandbox;

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

        ConnectionSetup(ReadyCallback callback) {
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
     * @param callback used to pass a callback function on creation of object.
     */
    public static void newConnectedInstance(ReadyCallback callback) {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), JsSandboxService0.class);
        ConnectionSetup connectionSetup = new ConnectionSetup(callback);
        boolean isBinding = ContextUtils.getApplicationContext().bindService(
                intent, connectionSetup, Context.BIND_AUTO_CREATE);
        if (!isBinding) {
            throw new RuntimeException(
                    "System couldn't find the sandbox service or client doesn't have"
                    + "permission to bind to it " + intent);
        }
    }

    // We prevent direct initialitations of this class. Use AwJsSandbox.newConnectedInstance().
    private AwJsSandbox(ConnectionSetup connectionSetup, IJsSandboxService jsSandboxService) {
        mConnection = connectionSetup;
        mJsSandboxService = jsSandboxService;
    }

    /** Creates an execution context within which JS can be executed multiple times. */
    public AwJsContext createContext() {
        if (mJsSandboxService == null) {
            throw new IllegalStateException(
                    "Attempting to createContext on a service that isn't connected");
        }
        try {
            IJsSandboxContext contextStub = mJsSandboxService.createContext();
            return new AwJsContext(contextStub);
        } catch (RemoteException e) {
            throw e.rethrowAsRuntimeException();
        }
    }

    @Override
    public void close() {
        if (mJsSandboxService == null) {
            return;
        }
        ContextUtils.getApplicationContext().unbindService(mConnection);
        mJsSandboxService = null;
    }
}
