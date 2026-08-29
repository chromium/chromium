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
import androidx.annotation.VisibleForTesting;

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
import java.util.concurrent.atomic.AtomicBoolean;

/** Manages a single ServiceConnection to an Android app. */
@NullMarked
public class NativeMessagingConnection implements ServiceConnection {
    private static final String TAG = "NMConnection";
    public static final String ACTION_NATIVE_MESSAGING =
            "org.chromium.chrome.browser.extensions.messaging.action.NATIVE_MESSAGING";

    @VisibleForTesting public static final long CONNECT_TIMEOUT_MS = 30 * 1000;

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
        if (mIsBound) {
            // Unbind if too much time passes between bindService returning true and
            // OnServiceConnected returning a `IBrowserNativeMessageService`.
            // This will no-op if:
            // - OnServiceConnected has returned a `IBrowserNativeMessageService`.
            // - The other ServiceConnection methods were called first which causes this class to
            // unbind.
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (mIsBound && mService == null) {
                            Log.w(TAG, "Service connection timed out for package: " + mPackageName);
                            unbind();
                        }
                    },
                    CONNECT_TIMEOUT_MS);
        } else {
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
        // This can be called after timeout. However if the connection times out
        // then we unbind from the external app and set `mIsBound` to be false.
        // No-op if this happens.
        if (!mIsBound) {
            return;
        }

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

    public void onExtensionUnloaded(String extensionId) {
        ExtensionSession session = mSessions.get(extensionId);
        if (session != null) {
            session.onExtensionUnloaded();
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
            Bundle info = new Bundle();
            info.putBoolean("isVerified", mIsVerifiedExtension);
            ConnectExtensionCallback callback = new ConnectExtensionCallback();
            try {
                browserService.connectExtension(mExtensionId, info, callback);
                // Report an error and abort the extension connecting if too much time passes
                // between the connectExtension call and the external app responding (success or
                // error) via `callback`.
                // This will no-op if:
                // - The external app has already responded via `callback`.
                // - The session was closed or unbound first (e.g. extension unloaded or service
                // disconnected).
                PostTask.postDelayedTask(
                        TaskTraits.UI_DEFAULT, callback::onTimeout, CONNECT_TIMEOUT_MS);
            } catch (RemoteException e) {
                onConnectExtensionError("RemoteException when calling connectExtension.");
            }
        }

        private void onConnectExtensionSuccess(IExtensionNativeMessageService service) {
            ThreadUtils.assertOnUiThread();
            if (mState != ConnectionState.PENDING) {
                // This can happen if the extension was unloaded after
                // IBrowserNativeMessageService.connectExtension was called but before the external
                // app returned with the result. In this case, call closeConnection() right when the
                // connectExtension call finishes.
                if (service != null) {
                    try {
                        service.closeConnection();
                    } catch (RemoteException e) {
                        Log.w(TAG, "Failed to call closeConnection() for " + mExtensionId, e);
                    }
                }
                return;
            }

            if (service == null) {
                onConnectExtensionError("App returned null service.");
                return;
            }

            mExtensionService = service;
            mState = ConnectionState.CONNECTED;
            for (NativeMessageAndroidPort port : new ArrayList<>(mPendingPorts)) {
                connectPort(port);
            }
            assert mPendingPorts.isEmpty();
        }

        private void onConnectExtensionError(String error) {
            ThreadUtils.assertOnUiThread();
            if (mState != ConnectionState.PENDING) {
                // Connection was closed/unbound while background task was in
                // flight.
                return;
            }

            Log.w(TAG, "Failed to connect extension session for " + mExtensionId + ": " + error);

            // We need to ensure the "connection rejected" message is identical to the "app not
            // found" or other error cases. The extension shouldn't be able to glean any
            // information about whether an app was installed or has native message handlers
            // from an error response.
            disconnect(getUnableToConnectError(mConnection.mPackageName));
        }

        private void connectPort(NativeMessageAndroidPort port) {
            if (mExtensionService == null) {
                port.closeChannel("Session is not connected.");
                return;
            }

            mActivePorts.add(port);
            mPendingPorts.remove(port);

            // Construct the Callback here and link it to `port` because the
            // external app might send a message BEFORE returning a
            // IExtensionNativeMessagePort.
            final NativeMessageAndroidPort.Callback portCallback =
                    new NativeMessageAndroidPort.Callback(port);

            ConnectPortCallback callback = new ConnectPortCallback(port);
            try {
                mExtensionService.connectPort(portCallback, callback);
                // Report an error and close the port if too much time passes between the
                // connectPort call and the external app responding (success or error) via
                // `callback`.
                // This will no-op if:
                // - The external app has already responded via `callback`.
                // - The port was closed or the session was disconnected first.
                PostTask.postDelayedTask(
                        TaskTraits.UI_DEFAULT, callback::onTimeout, CONNECT_TIMEOUT_MS);
            } catch (RemoteException e) {
                onConnectPortError(port, "RemoteException when calling connectPort.");
            }
        }

        private void onConnectPortSuccess(
                NativeMessageAndroidPort port, @Nullable IExtensionNativeMessagePort remotePort) {
            ThreadUtils.assertOnUiThread();
            // The port connected successfully if:
            // - The app returns a valid `IExtensionNativeMessagePort`.
            // - The extension is still connected to the app.
            // - The `NativeMessageAndroidPort` itself is still active in the
            //   browser.
            if (remotePort != null && isSessionConnected() && mActivePorts.contains(port)) {
                port.onConnected(remotePort);
                return;
            }

            // Close the port with an error if it is still active and was not already closed while
            // the connection attempt was in flight.
            if (mActivePorts.remove(port)) {
                port.closeChannel("Could not connect port to " + mConnection.mPackageName + ".");
            }

            // If the app returns a valid `IExtensionNativeMessagePort` but the
            // browser is not in a state to connect, tell it to disconnect.
            if (remotePort != null) {
                try {
                    remotePort.disconnect();
                } catch (RemoteException e) {
                    Log.w(TAG, "Failed to disconnect aborted remote port", e);
                }
            }
        }

        private void onConnectPortError(NativeMessageAndroidPort port, String error) {
            ThreadUtils.assertOnUiThread();
            if (mActivePorts.remove(port)) {
                Log.w(TAG, "Failed to connect port for " + mExtensionId + ": " + error);
                port.closeChannel("Could not connect port to " + mConnection.mPackageName + ".");
            }
        }

        private void disconnect(@Nullable String errorMessage) {
            // Only log if we were connected and unexpectedly getting torn down (e.g.
            // OnServiceDisconnected). Rejected connection errors are already logged in
            // onConnectExtensionError.
            if (errorMessage != null && mState == ConnectionState.CONNECTED) {
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

        public void onExtensionUnloaded() {
            if (mExtensionService != null) {
                try {
                    mExtensionService.closeConnection();
                } catch (RemoteException e) {
                    Log.w(TAG, "Failed to call closeConnection() for " + mExtensionId, e);
                }
            }

            disconnect(null);
        }

        @Nullable IExtensionNativeMessageService getServiceForTesting() {
            assert mState == ConnectionState.CONNECTED : "Session is not connected";
            return mExtensionService;
        }

        private class ConnectExtensionCallback extends IConnectExtensionCallback.Stub {
            private final AtomicBoolean mCalled = new AtomicBoolean();

            @Override
            public void onSuccess(IExtensionNativeMessageService service) {
                if (mCalled.compareAndSet(false, true)) {
                    ThreadUtils.postOnUiThread(() -> onConnectExtensionSuccess(service));
                } else if (service != null) {
                    // Multiple onSuccess calls should be rare. Try to close the
                    // duplicate `service` if this happens.
                    try {
                        service.closeConnection();
                    } catch (RemoteException e) {
                    }
                }
            }

            @Override
            public void onError(String error) {
                if (mCalled.compareAndSet(false, true)) {
                    ThreadUtils.postOnUiThread(() -> onConnectExtensionError(error));
                }
            }

            void onTimeout() {
                if (mCalled.compareAndSet(false, true)) {
                    // This no-ops if the session was already closed or unbound.
                    onConnectExtensionError("connectExtension call timed out.");
                }
            }
        }

        private class ConnectPortCallback extends IConnectPortCallback.Stub {
            private final AtomicBoolean mCalled = new AtomicBoolean();
            private final NativeMessageAndroidPort mPort;

            ConnectPortCallback(NativeMessageAndroidPort port) {
                mPort = port;
            }

            @Override
            public void onSuccess(IExtensionNativeMessagePort remotePort) {
                if (mCalled.compareAndSet(false, true)) {
                    ThreadUtils.postOnUiThread(() -> onConnectPortSuccess(mPort, remotePort));
                } else if (remotePort != null) {
                    // Multiple onSuccess calls should be rare. Try to close the
                    // duplicate `remotePort` if this happens.
                    try {
                        remotePort.disconnect();
                    } catch (RemoteException e) {
                    }
                }
            }

            @Override
            public void onError(String error) {
                if (mCalled.compareAndSet(false, true)) {
                    ThreadUtils.postOnUiThread(() -> onConnectPortError(mPort, error));
                }
            }

            void onTimeout() {
                if (mCalled.compareAndSet(false, true)) {
                    // This no-ops if the port was already closed or disconnected.
                    onConnectPortError(mPort, "connectPort call timed out.");
                }
            }
        }
    }
}
