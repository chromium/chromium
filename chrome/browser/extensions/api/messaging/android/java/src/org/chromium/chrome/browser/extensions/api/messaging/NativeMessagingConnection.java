// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

/** Manages a single ServiceConnection to an Android app. */
@NullMarked
public class NativeMessagingConnection implements ServiceConnection {
    private static final String TAG = "NMConnection";
    public static final String ACTION_NATIVE_MESSAGING =
            "org.chromium.chrome.browser.extensions.messaging.action.NATIVE_MESSAGING";

    public interface Observer {
        void onUnbound(String packageName);
    }

    private final String mPackageName;
    private boolean mIsBound;
    private final Map<String, ExtensionSession> mSessions = new HashMap<>();

    private @Nullable IBrowserNativeMessageService mService;
    private @Nullable Observer mObserver;

    public static String getUnableToConnectError(String packageName) {
        return "Unable to connect to " + packageName + ".";
    }

    public NativeMessagingConnection(String packageName, Observer observer) {
        mPackageName = packageName;
        mObserver = observer;

        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(ACTION_NATIVE_MESSAGING);
        intent.setPackage(mPackageName);

        mIsBound = context.bindService(intent, this, Context.BIND_AUTO_CREATE);

        if (!mIsBound) {
            Log.w(TAG, "Failed to bind to service for package: " + mPackageName);
        }
    }

    public boolean isBound() {
        return mIsBound;
    }

    public @Nullable String addPort(
            String extensionId, boolean isVerifiedExtension, NativeMessageAndroidPort port) {
        if (!mIsBound) {
            return getUnableToConnectError(mPackageName);
        }

        ExtensionSession session = mSessions.get(extensionId);
        if (session == null) {
            session = new ExtensionSession(extensionId, isVerifiedExtension, this);
            mSessions.put(extensionId, session);

            if (mService != null) {
                session.authenticateExtensionAndConnectPorts(mService);
            }
        }

        session.addPort(port);
        return null;
    }

    public void unbind() {
        if (!mIsBound) {
            return;
        }
        mIsBound = false;

        try {
            ContextUtils.getApplicationContext().unbindService(this);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Service was not registered during unbind", e);
        }
        mService = null;

        // Iterate over `mSessions.values()` after snapshotting it in a list since
        // `session.disconnect` may notify this class to also remove the session from `mSessions`.
        for (ExtensionSession session : new ArrayList<>(mSessions.values())) {
            // Use the "service disconnected" error message only for extensions that were
            // authenticated by the external app.
            String errorMessage =
                    session.isSessionConnected()
                            ? "Service disconnected for app: " + mPackageName + "."
                            : getUnableToConnectError(mPackageName);
            session.disconnect(errorMessage);
        }
        mSessions.clear();

        if (mObserver != null) {
            mObserver.onUnbound(mPackageName);
            mObserver = null;
        }
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        mService = IBrowserNativeMessageService.Stub.asInterface(service);
        for (ExtensionSession session : mSessions.values()) {
            session.authenticateExtensionAndConnectPorts(mService);
        }
    }

    @Override
    public void onServiceDisconnected(@Nullable ComponentName name) {
        // We unbind here because the app's service has likely crashed and not
        // unbinding and expecting the service to auto-startup again causes a
        // mismatch in state parity where:
        // - the extension might have all the state/session records
        // - unless the app proactively saves messaging "sessions" as they
        //   happen it will not retain any state.
        // Therefore it's cleaner to just unbind.
        Log.i(TAG, "Service disconnected for package: " + mPackageName);
        unbind();
    }

    @Override
    public void onBindingDied(@Nullable ComponentName name) {
        Log.w(TAG, "Binding died for package: " + mPackageName);
        unbind();
    }

    @Override
    public void onNullBinding(@Nullable ComponentName name) {
        Log.w(TAG, "Null binding for package: " + mPackageName);
        unbind();
    }

    public void onSessionDisconnected(String extensionId) {
        mSessions.remove(extensionId);

        // TODO(crbug.com/515159909): Only unbind if a bit of time passes and
        // there has been no more new connections.
        if (mSessions.isEmpty()) {
            unbind();
        }
    }

    @Nullable ExtensionSession getSessionForTesting(String extensionId) {
        return mSessions.get(extensionId);
    }

    @Nullable IBrowserNativeMessageService getServiceForTesting() {
        return mService;
    }

    static class ExtensionSession implements NativeMessageAndroidPort.Observer {
        // Tracks the state of this ExtensionSession.
        @IntDef({
            ConnectionState.DISCONNECTED,
            ConnectionState.PENDING,
            ConnectionState.CONNECTED,
        })
        @Retention(RetentionPolicy.SOURCE)
        private @interface ConnectionState {
            // The session is not connected to the target app. If this state is
            // reached from any of the other two, the session should be purged.
            int DISCONNECTED = 0;
            // A connection to the target app has been initiated.
            int PENDING = 1;
            // The session is connected to the target app and `service` is available.
            int CONNECTED = 2;
        }

        private static class ConnectionResult<T> {
            public final @Nullable T remote;
            public final @Nullable String errorMessage;

            public ConnectionResult(@Nullable T remote, @Nullable String errorMessage) {
                this.remote = remote;
                this.errorMessage = errorMessage;
            }

            public boolean isSuccess() {
                return remote != null && errorMessage == null;
            }
        }

        private final String mExtensionId;
        private final boolean mIsVerifiedExtension;
        private @Nullable IExtensionNativeMessageService mExtensionService;
        private @ConnectionState int mState = ConnectionState.DISCONNECTED;

        // Ports waiting for the service to bind and authenticate.
        private final Set<NativeMessageAndroidPort> mPendingPorts = new LinkedHashSet<>();

        // Ports that are connecting or have connected to the app.
        private final Set<NativeMessageAndroidPort> mActivePorts = new LinkedHashSet<>();

        private final NativeMessagingConnection mConnection;

        public ExtensionSession(
                String extensionId,
                boolean isVerifiedExtension,
                NativeMessagingConnection connection) {
            mExtensionId = extensionId;
            mIsVerifiedExtension = isVerifiedExtension;
            mConnection = connection;
        }

        @Override
        public void onPortDestroying(NativeMessageAndroidPort port) {
            mPendingPorts.remove(port);
            mActivePorts.remove(port);
        }

        public boolean isSessionConnected() {
            return mState == ConnectionState.CONNECTED && mExtensionService != null;
        }

        public void addPort(NativeMessageAndroidPort port) {
            port.setObserver(this);

            if (isSessionConnected()) {
                connectPort(port);
            } else {
                mPendingPorts.add(port);
            }
        }

        public void authenticateExtensionAndConnectPorts(
                IBrowserNativeMessageService browserService) {
            if (mState != ConnectionState.DISCONNECTED) {
                // No-op on repeated calls. Check that `mExtensionService` is
                // set if and only if ConnectionState CONNECTED.
                assert mState != ConnectionState.CONNECTED || mExtensionService != null;
                return;
            }

            mState = ConnectionState.PENDING;
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE_MAY_BLOCK,
                    () -> {
                        ConnectionResult<IExtensionNativeMessageService> result =
                                authenticateExtensionInBackground(
                                        browserService,
                                        mExtensionId,
                                        mIsVerifiedExtension,
                                        mConnection.mPackageName);
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT, () -> onConnectExtensionResult(result));
                    });
        }

        private static ConnectionResult<IExtensionNativeMessageService>
                authenticateExtensionInBackground(
                        IBrowserNativeMessageService browserService,
                        String extensionId,
                        boolean isVerifiedExtension,
                        String packageName) {
            ThreadUtils.assertOnBackgroundThread();
            try {
                Bundle info = new Bundle();
                info.putBoolean("isVerified", isVerifiedExtension);
                IExtensionNativeMessageService service =
                        browserService.connectExtension(extensionId, info);
                if (service != null) {
                    return new ConnectionResult<>(service, null);
                }
                return new ConnectionResult<>(null, getUnableToConnectError(packageName));
            } catch (Exception e) {
                Log.w(TAG, "Exception during connectExtension for " + extensionId, e);

                // We need to ensure the "connection rejected" message is identical to the "app not
                // found" or other error cases. The extension shouldn't be able to glean any
                // information about whether an app was installed or has native message handlers
                // from an error response.
                return new ConnectionResult<>(null, getUnableToConnectError(packageName));
            }
        }

        private void onConnectExtensionResult(
                ConnectionResult<IExtensionNativeMessageService> result) {
            if (mState != ConnectionState.PENDING) {
                // Connection was closed/unbound while background task was in
                // flight.
                return;
            }

            if (result.isSuccess()) {
                mExtensionService = result.remote;
                mState = ConnectionState.CONNECTED;
                for (NativeMessageAndroidPort port : new ArrayList<>(mPendingPorts)) {
                    connectPort(port);
                }
                assert mPendingPorts.isEmpty();
            } else {
                Log.w(
                        TAG,
                        "Failed to connect extension session for "
                                + mExtensionId
                                + ": "
                                + result.errorMessage);
                disconnect(result.errorMessage);
            }
        }

        private void connectPort(NativeMessageAndroidPort port) {
            if (mExtensionService == null) {
                port.closeChannel("Session is not connected.");
                return;
            }

            mActivePorts.add(port);
            mPendingPorts.remove(port);
            final IExtensionNativeMessageService service = mExtensionService;

            // Construct the Callback here and link it to `port` because the
            // external app might send a message BEFORE returning a
            // IExtensionNativeMessagePort.
            final NativeMessageAndroidPort.Callback portCallback =
                    new NativeMessageAndroidPort.Callback(port);

            PostTask.postTask(
                    TaskTraits.USER_VISIBLE_MAY_BLOCK,
                    () -> {
                        ConnectionResult<IExtensionNativeMessagePort> result =
                                connectPortInBackground(
                                        service,
                                        portCallback,
                                        mExtensionId,
                                        mConnection.mPackageName);
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT, () -> onConnectPortResult(port, result));
                    });
        }

        private static ConnectionResult<IExtensionNativeMessagePort> connectPortInBackground(
                IExtensionNativeMessageService service,
                NativeMessageAndroidPort.Callback callback,
                String extensionId,
                String packageName) {
            ThreadUtils.assertOnBackgroundThread();
            try {
                IExtensionNativeMessagePort remotePort = service.connectPort(callback);
                if (remotePort != null) {
                    return new ConnectionResult<>(remotePort, null);
                }
                return new ConnectionResult<>(
                        null, "Could not connect port to " + packageName + ".");
            } catch (Exception e) {
                Log.w(TAG, "Failed to connect port for extension: " + extensionId, e);
                return new ConnectionResult<>(
                        null, "Could not connect port to " + packageName + ".");
            }
        }

        private void onConnectPortResult(
                NativeMessageAndroidPort port,
                ConnectionResult<IExtensionNativeMessagePort> result) {
            // The port connected successfully if:
            // - The app returns a valid `IExtensionNativeMessagePort`.
            // - The extension is still connected to the app.
            // - The `NativeMessageAndroidPort` itself is still active in the
            //   browser.
            if (result.isSuccess() && isSessionConnected() && mActivePorts.contains(port)) {
                assert result.remote != null; // Needed for NullAway.
                port.onConnected(result.remote);
                return;
            }

            // Close the port with an error if it is still active and was not already closed while
            // the connection attempt was in flight.
            if (mActivePorts.remove(port)) {
                String error =
                        result.errorMessage != null
                                ? result.errorMessage
                                : "Could not connect port to " + mConnection.mPackageName + ".";
                port.closeChannel(error);
            }

            // If the app returns a valid `IExtensionNativeMessagePort` but the
            // browser is not in a state to connect, tell it to disconnect.
            if (result.remote != null) {
                try {
                    result.remote.disconnect();
                } catch (RemoteException e) {
                    Log.w(TAG, "Failed to disconnect aborted remote port", e);
                }
            }
        }

        private void disconnect(@Nullable String errorMessage) {
            if (errorMessage != null) {
                Log.w(TAG, "Disconnecting session for " + mExtensionId + ": " + errorMessage);
            }
            mExtensionService = null;
            mState = ConnectionState.DISCONNECTED;

            // TODO(crbug.com/515159909): In the future, port teardown and error propagation
            // should be coordinated with the C++ NativeMessageAndroidPort layer.
            String closeReason = errorMessage != null ? errorMessage : "Session disconnected.";
            for (NativeMessageAndroidPort port : new ArrayList<>(mPendingPorts)) {
                port.closeChannel(closeReason);
            }
            mPendingPorts.clear();

            for (NativeMessageAndroidPort port : new ArrayList<>(mActivePorts)) {
                port.closeChannel(closeReason);
            }
            mActivePorts.clear();

            mConnection.onSessionDisconnected(mExtensionId);
        }

        @Nullable IExtensionNativeMessageService getServiceForTesting() {
            assert mState == ConnectionState.CONNECTED : "Session is not connected";
            return mExtensionService;
        }
    }
}
