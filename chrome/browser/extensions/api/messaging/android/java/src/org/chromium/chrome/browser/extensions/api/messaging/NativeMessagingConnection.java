// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

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
import java.util.Map;

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

    public NativeMessagingConnection(String packageName, Observer observer) {
        mPackageName = packageName;
        mObserver = observer;

        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(ACTION_NATIVE_MESSAGING);
        intent.setPackage(mPackageName);

        mIsBound = context.bindService(intent, this, Context.BIND_AUTO_CREATE);

        if (!mIsBound) {
            Log.e(TAG, "Failed to bind to service for package: " + mPackageName);
        }
    }

    public boolean isBound() {
        return mIsBound;
    }

    // TODO(crbug.com/515159909): Rename this "addPort" and potentially add a
    // callback param since `session.connectExtension` is asynchronous.
    public @Nullable String connectExtension(String extensionId) {
        if (!mIsBound) {
            return "Could not add port: not connected to app " + mPackageName;
        }

        ExtensionSession session = mSessions.get(extensionId);
        if (session == null) {
            session = new ExtensionSession(extensionId, this);
            mSessions.put(extensionId, session);

            if (mService != null) {
                session.connectExtension(mService);
            }
        }

        return null;
    }

    public void unbind() {
        if (mIsBound) {
            try {
                ContextUtils.getApplicationContext().unbindService(this);
            } catch (IllegalArgumentException e) {
                Log.w(TAG, "Service was not registered during unbind", e);
            }
            mIsBound = false;
        }
        mService = null;

        // Iterate over `mSessions.values()` after snapshotting it in a list
        // since `session.disconnect` may notify this class to also remove the
        // session from `mSessions`.
        for (ExtensionSession session : new ArrayList<>(mSessions.values())) {
            session.disconnect("Service disconnected for app: " + mPackageName);
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
            session.connectExtension(mService);
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

    static class ExtensionSession {
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

        private static class ConnectionResult {
            public final @Nullable IExtensionNativeMessageService service;
            public final @Nullable String errorMessage;

            public ConnectionResult(
                    @Nullable IExtensionNativeMessageService service,
                    @Nullable String errorMessage) {
                this.service = service;
                this.errorMessage = errorMessage;
            }

            public boolean isSuccess() {
                return service != null;
            }
        }

        private final String mExtensionId;
        private @Nullable IExtensionNativeMessageService mExtensionService;
        private @ConnectionState int mState = ConnectionState.DISCONNECTED;

        private final NativeMessagingConnection mConnection;

        public ExtensionSession(String extensionId, NativeMessagingConnection connection) {
            mExtensionId = extensionId;
            mConnection = connection;
        }

        public void connectExtension(IBrowserNativeMessageService browserService) {
            if (mState != ConnectionState.DISCONNECTED) {
                assert mExtensionService != null;
                return;
            }

            mState = ConnectionState.PENDING;
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE_MAY_BLOCK,
                    () -> {
                        ConnectionResult result =
                                authenticateExtensionInBackground(browserService, mExtensionId);
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT, () -> onConnectExtensionResult(result));
                    });
        }

        private static ConnectionResult authenticateExtensionInBackground(
                IBrowserNativeMessageService browserService, String extensionId) {
            ThreadUtils.assertOnBackgroundThread();
            try {
                IExtensionNativeMessageService service =
                        browserService.connectExtension(extensionId, null);
                if (service != null) {
                    return new ConnectionResult(service, null);
                }
                return new ConnectionResult(
                        null, "connectExtension returned null for " + extensionId);
            } catch (Exception e) {
                Log.e(TAG, "Exception during connectExtension for " + extensionId, e);
                String errorMsg = e.getMessage() != null ? e.getMessage() : e.toString();
                return new ConnectionResult(null, errorMsg);
            }
        }

        private void onConnectExtensionResult(ConnectionResult result) {
            if (mState != ConnectionState.PENDING) {
                // Connection was closed/unbound while background task was in
                // flight.
                return;
            }

            if (result.isSuccess()) {
                mExtensionService = result.service;
                mState = ConnectionState.CONNECTED;
                Log.i(TAG, "Extension session connected for " + mExtensionId);
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

        private void disconnect(@Nullable String errorMessage) {
            if (errorMessage != null) {
                Log.w(TAG, "Disconnecting session for " + mExtensionId + ": " + errorMessage);
            }
            mExtensionService = null;
            mState = ConnectionState.DISCONNECTED;
            mConnection.onSessionDisconnected(mExtensionId);
        }

        @Nullable IExtensionNativeMessageService getServiceForTesting() {
            assert mState == ConnectionState.CONNECTED : "Session is not connected";
            return mExtensionService;
        }
    }
}
