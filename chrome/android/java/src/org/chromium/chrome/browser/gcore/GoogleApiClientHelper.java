// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gcore;

import android.os.Bundle;
import android.os.Handler;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.GoogleApiClient.ConnectionCallbacks;
import com.google.android.gms.common.api.GoogleApiClient.OnConnectionFailedListener;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

/**
 * Helps managing connections when using {@link GoogleApiClient}.
 *
 * It features:
 *
 * <ul>
 * <li>Connection failure handling: some connection failures can be solved by retrying. In those
 * cases, a few reconnection attempts will be made.</li>
 *
 * <li>Connection and disconnection when Chrome is started/stopped: Once a client is registered,
 * it will be disconnected when Chrome goes in the background, and its connection state restored
 * when Chrome comes back to the foreground. That relies on an {@link ApplicationStateListener}.
 * That disconnection can be delayed, call {@link #setDisconnectionDelay(long)} to configure it.
 * </li>
 * </ul>
 *
 * <p>
 * It should be used when we want to keep a Client around for extended durations and use it quite
 * often, independently of the current Activity. For intermittent usage, see {@link ConnectedTask}
 * or directly create {@link GoogleApiClient}s. They are already cheap and resilient by design, and
 * can be tied to a specific activity's lifecycle using
 * {@link GoogleApiClient.Builder#enableAutoManage}.
 * </p>
 *
 * Usage:
 * <pre>
 * {@code
 * // Create a GoogleApiClient as usual
 * GoogleApiClient client = new GoogleApiClient.Builder(context)
 *                                             ...
 *                                             .build();
 *
 * // If further configuration is not needed, you don't need to care about the returned object.
 * GoogleApiClientHelper helper = new GoogleApiClient(client);
 * helper.setDisconnectionDelay(3000);
 *
 * // Use your client as usual.
 * client.connect();
 *
 * ...
 *
 * // If you don't need the client anymore and want to get rid of it, unregister it first. You need
 * // a reference to the GoogleApiClientHelper object for it.
 * helper.disable();
 *
 * // It still has to be disconnected if it's not done already.
 * client.disconnect();
 * }
 * </pre>
 */
public class GoogleApiClientHelper implements OnConnectionFailedListener, ConnectionCallbacks {
    private static final String TAG = "GCore";

    private int mResolutionAttempts;
    private boolean mWasConnectedBefore;
    private final Handler mHandler = new Handler(ThreadUtils.getUiThreadLooper());
    private final GoogleApiClient mClient;
    private long mDisconnectionDelayMs;
    private Runnable mPendingDisconnect;

    /**
     * Creates a helper and enrolls it in the various connection management features.
     * See the class documentation for {@link GoogleApiClientHelper} for more information.
     *
     * @param client The client to wrap.
     */
    public GoogleApiClientHelper(GoogleApiClient client) {
        mClient = client;
        enableConnectionRetrying(true);
        enableLifecycleManagement(true);
    }

    /**
     * Opts in or out of lifecycle management. The client's connection will be closed and reopened
     * when Chrome goes in and out of background.
     *
     * It is safe to set it to the current state. Disabling lifecycle management also cancels
     * pending disconnections.
     */
    public void enableLifecycleManagement(final boolean enabled) {
        Log.d(TAG, "enableLifecycleManagement(%s)", enabled);
        LifecycleHook hook = LifecycleHook.getInstance();

        if (enabled) {
            hook.registerClientHelper(GoogleApiClientHelper.this);
        } else {
            cancelPendingDisconnection();
            hook.unregisterClientHelper(GoogleApiClientHelper.this);
        }
    }

    /**
     * Opts in or out of connection retrying. The client will attempt to connect again after some
     * connection failures.
     *
     * Enabling or disabling it while it is already enabled or disabled has no effect.
     */
    public void enableConnectionRetrying(boolean enabled) {
        if (enabled) {
            mClient.registerConnectionCallbacks(this);
            mClient.registerConnectionFailedListener(this);
        } else {
            mClient.unregisterConnectionCallbacks(this);
            mClient.unregisterConnectionFailedListener(this);
        }
    }

    /**
     * Sets the disconnection delay. It is used to delay disconnection when the lifecycle is
     * managed. That can allow in flight queries to complete before the client is disconnected.
     *
     * The default delay is 0.
     */
    public void setDisconnectionDelay(long delayMs) {
        mDisconnectionDelayMs = delayMs;
    }

    /**
     * Opt out of the various connection management features. This method should be called if the
     * helper features are not desired anymore. It can then be discarded. The client itself can
     * still be used as normal after that.
     */
    public void disable() {
        enableLifecycleManagement(false);
        enableConnectionRetrying(false);
        setDisconnectionDelay(0);
    }

    /**
     * Tells the helper that we are going to use the connection. It should postpone disconnections
     * and make sure the client is connected.
     * This is useful if the client might be used when we are in the background.
     */
    public void willUseConnection() {
        // Cancel and reschedule the disconnection if we are in the background. We do it early to
        // avoid race conditions between a disconnect on the UI thread and the connect below.
        if (!ApplicationStatus.hasVisibleActivities()) scheduleDisconnection();

        // The client might be disconnected if we were idle in the background for too long.
        if (!mClient.isConnected() && !mClient.isConnecting()) {
            Log.d(TAG, "Reconnecting the client.");
            mClient.connect();
        }
    }

    void restoreConnectedState() {
        // If we go back to the foreground before a delayed disconnect happens, cancel it.
        cancelPendingDisconnection();

        if (mWasConnectedBefore) {
            mClient.connect();
        }
    }

    /**
     * Schedule a disconnection of the client after the predefined delay. If there was a
     * disconnection already planned, it will be rescheduled from now.
     */
    void scheduleDisconnection() {
        cancelPendingDisconnection();

        mPendingDisconnect =
                new Runnable() {
                    @Override
                    public void run() {
                        Log.d(TAG, "Disconnect delay expired.");
                        mPendingDisconnect = null;
                        disconnect();
                    }
                };

        mHandler.postDelayed(mPendingDisconnect, mDisconnectionDelayMs);
    }

    private void disconnect() {
        if (mClient.isConnected() || mClient.isConnecting()) {
            mWasConnectedBefore = true;
        }

        // We always call disconnect to abort possibly pending connection requests.
        mClient.disconnect();
    }

    private void cancelPendingDisconnection() {
        if (mPendingDisconnect == null) return;

        mHandler.removeCallbacks(mPendingDisconnect);
        mPendingDisconnect = null;
    }

    @Override
    public void onConnectionFailed(ConnectionResult result) {
        if (!isErrorRecoverableByRetrying(result.getErrorCode())) {
            Log.d(
                    TAG,
                    "Not retrying managed client connection. Unrecoverable error: %d",
                    result.getErrorCode());
            return;
        }

        if (mResolutionAttempts < ConnectedTask.RETRY_NUMBER_LIMIT) {
            Log.d(
                    TAG,
                    "Retrying managed client connection. attempt %d/%d - errorCode: %d",
                    mResolutionAttempts,
                    ConnectedTask.RETRY_NUMBER_LIMIT,
                    result.getErrorCode());
            mResolutionAttempts += 1;

            mHandler.postDelayed(
                    new Runnable() {
                        @Override
                        public void run() {
                            mClient.connect();
                        }
                    },
                    ConnectedTask.CONNECTION_RETRY_TIME_MS);
        }
    }

    @Override
    public void onConnected(Bundle connectionHint) {
        mResolutionAttempts = 0;
    }

    @Override
    public void onConnectionSuspended(int cause) {
        // GoogleApiClient handles retrying on suspension itself. Logging in case it didn't succeed
        // for some reason.
        Log.w(TAG, "Managed client connection suspended. Cause: %d", cause);
    }

    private static boolean isErrorRecoverableByRetrying(int errorCode) {
        return errorCode == ConnectionResult.INTERNAL_ERROR
                || errorCode == ConnectionResult.NETWORK_ERROR
                || errorCode == ConnectionResult.SERVICE_UPDATING;
    }
}
