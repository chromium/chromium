// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.base.ChildBindingState;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureLevel;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.memory.MemoryPressureCallback;

import java.util.Arrays;
import java.util.List;

import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

/**
 * Manages a connection between the browser activity and a child service.
 */
public class ChildProcessConnection {
    private static final String TAG = "ChildProcessConn";
    private static final int NUM_BINDING_STATES = ChildBindingState.MAX_VALUE + 1;

    /**
     * Used to notify the consumer about the process start. These callbacks will be invoked before
     * the ConnectionCallbacks.
     */
    public interface ServiceCallback {
        /**
         * Called when the child process has successfully started and is ready for connection
         * setup.
         */
        void onChildStarted();

        /**
         * Called when the child process failed to start. This can happen if the process is already
         * in use by another client. The client will not receive any other callbacks after this one.
         */
        void onChildStartFailed(ChildProcessConnection connection);

        /**
         * Called when the service has been disconnected. whether it was stopped by the client or
         * if it stopped unexpectedly (process crash).
         * This is the last callback from this interface that a client will receive for a specific
         * connection.
         */
        void onChildProcessDied(ChildProcessConnection connection);
    }

    /**
     * Used to notify the consumer about the connection being established.
     */
    public interface ConnectionCallback {
        /**
         * Called when the connection to the service is established.
         * @param connection the connection object to the child process
         */
        void onConnected(ChildProcessConnection connection);
    }

    /**
     * Delegate that ChildServiceConnection should call when the service connects/disconnects.
     * These callbacks are expected to happen on a background thread.
     */
    @VisibleForTesting
    protected interface ChildServiceConnectionDelegate {
        void onServiceConnected(IBinder service);
        void onServiceDisconnected();
    }

    @VisibleForTesting
    protected interface ChildServiceConnectionFactory {
        ChildServiceConnection createConnection(
                Intent bindIntent, int bindFlags, ChildServiceConnectionDelegate delegate);
    }

    /** Interface representing a connection to the Android service. Can be mocked in unit-tests. */
    @VisibleForTesting
    protected interface ChildServiceConnection {
        boolean bind();
        void unbind();
        boolean isBound();
    }

    /** Implementation of ChildServiceConnection that does connect to a service. */
    private static class ChildServiceConnectionImpl
            implements ChildServiceConnection, ServiceConnection {
        private final Context mContext;
        private final Intent mBindIntent;
        private final int mBindFlags;
        private final ChildServiceConnectionDelegate mDelegate;
        private boolean mBound;

        private ChildServiceConnectionImpl(Context context, Intent bindIntent, int bindFlags,
                ChildServiceConnectionDelegate delegate) {
            mContext = context;
            mBindIntent = bindIntent;
            mBindFlags = bindFlags;
            mDelegate = delegate;
        }

        @Override
        public boolean bind() {
            if (!mBound) {
                try {
                    TraceEvent.begin("ChildProcessConnection.ChildServiceConnectionImpl.bind");
                    mBound = mContext.bindService(mBindIntent, this, mBindFlags);
                } finally {
                    TraceEvent.end("ChildProcessConnection.ChildServiceConnectionImpl.bind");
                }
            }
            return mBound;
        }

        @Override
        public void unbind() {
            if (mBound) {
                mContext.unbindService(this);
                mBound = false;
            }
        }

        @Override
        public boolean isBound() {
            return mBound;
        }

        @Override
        public void onServiceConnected(ComponentName className, final IBinder service) {
            mDelegate.onServiceConnected(service);
        }

        // Called on the main thread to notify that the child service did not disconnect gracefully.
        @Override
        public void onServiceDisconnected(ComponentName className) {
            mDelegate.onServiceDisconnected();
        }
    }

    // Global lock to protect all the fields that can be accessed outside launcher thread.
    private static final Object sBindingStateLock = new Object();

    @GuardedBy("sBindingStateLock")
    private static final int[] sAllBindingStateCounts = new int[NUM_BINDING_STATES];

    @VisibleForTesting
    static void resetBindingStateCountsForTesting() {
        synchronized (sBindingStateLock) {
            for (int i = 0; i < NUM_BINDING_STATES; ++i) {
                sAllBindingStateCounts[i] = 0;
            }
        }
    }

    private final Handler mLauncherHandler;
    private final ComponentName mServiceName;

    // Parameters passed to the child process through the service binding intent.
    // If the service gets recreated by the framework the intent will be reused, so these parameters
    // should be common to all processes of that type.
    private final Bundle mServiceBundle;

    // Whether bindToCaller should be called on the service after setup to check that only one
    // process is bound to the service.
    private final boolean mBindToCaller;

    private static class ConnectionParams {
        final Bundle mConnectionBundle;
        final List<IBinder> mClientInterfaces;

        ConnectionParams(Bundle connectionBundle, List<IBinder> clientInterfaces) {
            mConnectionBundle = connectionBundle;
            mClientInterfaces = clientInterfaces;
        }
    }

    // This is set in start() and is used in onServiceConnected().
    private ServiceCallback mServiceCallback;

    // This is set in setupConnection() and is later used in doConnectionSetup(), after which the
    // variable is cleared. Therefore this is only valid while the connection is being set up.
    private ConnectionParams mConnectionParams;

    // Callback provided in setupConnection() that will communicate the result to the caller. This
    // has to be called exactly once after setupConnection(), even if setup fails, so that the
    // caller can free up resources associated with the setup attempt. This is set to null after the
    // call.
    private ConnectionCallback mConnectionCallback;

    private IChildProcessService mService;

    // Set to true when the service connection callback runs. This differs from
    // mServiceConnectComplete, which tracks that the connection completed successfully.
    private boolean mDidOnServiceConnected;

    // Set to true when the service connected successfully.
    private boolean mServiceConnectComplete;

    // Set to true when the service disconnects, as opposed to being properly closed. This happens
    // when the process crashes or gets killed by the system out-of-memory killer.
    private boolean mServiceDisconnected;

    // Process ID of the corresponding child process.
    private int mPid;

    // Strong binding will make the service priority equal to the priority of the activity.
    private final ChildServiceConnection mStrongBinding;

    // Moderate binding will make the service priority equal to the priority of a visible process
    // while the app is in the foreground.
    // This is also used as the initial binding before any priorities are set.
    private final ChildServiceConnection mModerateBinding;

    // Low priority binding maintained in the entire lifetime of the connection, i.e. between calls
    // to start() and stop().
    private final ChildServiceConnection mWaivedBinding;

    // Refcount of bindings.
    private int mStrongBindingCount;
    private int mModerateBindingCount;

    // Set to true once unbind() was called.
    private boolean mUnbound;

    // Binding state of this connection.
    @GuardedBy("sBindingStateLock")
    private @ChildBindingState int mBindingState;

    // Same as above except it no longer updates after |unbind()|.
    @GuardedBy("sBindingStateLock")
    private @ChildBindingState int mBindingStateCurrentOrWhenDied;

    // Indicate |kill()| was called to intentionally kill this process.
    @GuardedBy("sBindingStateLock")
    private boolean mKilledByUs;

    // Copy of |sAllBindingStateCounts| at the time this is unbound.
    @GuardedBy("sBindingStateLock")
    private int[] mAllBindingStateCountsWhenDied;

    private MemoryPressureCallback mMemoryPressureCallback;

    public ChildProcessConnection(Context context, ComponentName serviceName, boolean bindToCaller,
            boolean bindAsExternalService, Bundle serviceBundle) {
        this(context, serviceName, bindToCaller, bindAsExternalService, serviceBundle,
                null /* connectionFactory */);
    }

    @VisibleForTesting
    public ChildProcessConnection(final Context context, ComponentName serviceName,
            boolean bindToCaller, boolean bindAsExternalService, Bundle serviceBundle,
            ChildServiceConnectionFactory connectionFactory) {
        mLauncherHandler = new Handler();
        assert isRunningOnLauncherThread();
        mServiceName = serviceName;
        mServiceBundle = serviceBundle != null ? serviceBundle : new Bundle();
        mServiceBundle.putBoolean(ChildProcessConstants.EXTRA_BIND_TO_CALLER, bindToCaller);
        mBindToCaller = bindToCaller;

        if (connectionFactory == null) {
            connectionFactory = new ChildServiceConnectionFactory() {
                @Override
                public ChildServiceConnection createConnection(
                        Intent bindIntent, int bindFlags, ChildServiceConnectionDelegate delegate) {
                    return new ChildServiceConnectionImpl(context, bindIntent, bindFlags, delegate);
                }
            };
        }

        ChildServiceConnectionDelegate delegate = new ChildServiceConnectionDelegate() {
            @Override
            public void onServiceConnected(final IBinder service) {
                mLauncherHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        onServiceConnectedOnLauncherThread(service);
                    }
                });
            }

            @Override
            public void onServiceDisconnected() {
                mLauncherHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        onServiceDisconnectedOnLauncherThread();
                    }
                });
            }
        };

        Intent intent = new Intent();
        intent.setComponent(serviceName);
        if (serviceBundle != null) {
            intent.putExtras(serviceBundle);
        }

        int defaultFlags = Context.BIND_AUTO_CREATE
                | (bindAsExternalService ? Context.BIND_EXTERNAL_SERVICE : 0);

        mModerateBinding = connectionFactory.createConnection(intent, defaultFlags, delegate);
        mStrongBinding = connectionFactory.createConnection(
                intent, defaultFlags | Context.BIND_IMPORTANT, delegate);
        mWaivedBinding = connectionFactory.createConnection(
                intent, defaultFlags | Context.BIND_WAIVE_PRIORITY, delegate);
    }

    public final IChildProcessService getService() {
        assert isRunningOnLauncherThread();
        return mService;
    }

    public final ComponentName getServiceName() {
        assert isRunningOnLauncherThread();
        return mServiceName;
    }

    public boolean isConnected() {
        return mService != null;
    }

    /**
     * @return the connection pid, or 0 if not yet connected
     */
    public int getPid() {
        assert isRunningOnLauncherThread();
        return mPid;
    }

    /**
     * Starts a connection to an IChildProcessService. This must be followed by a call to
     * setupConnection() to setup the connection parameters. start() and setupConnection() are
     * separate to allow to pass whatever parameters are available in start(), and complete the
     * remainder addStrongBinding while reducing the connection setup latency.
     * @param useStrongBinding whether a strong binding should be bound by default. If false, an
     * initial moderate binding is used.
     * @param serviceCallback (optional) callbacks invoked when the child process starts or fails to
     * start and when the service stops.
     */
    public void start(boolean useStrongBinding, ServiceCallback serviceCallback) {
        try {
            TraceEvent.begin("ChildProcessConnection.start");
            assert isRunningOnLauncherThread();
            assert mConnectionParams
                    == null : "setupConnection() called before start() in ChildProcessConnection.";

            mServiceCallback = serviceCallback;

            if (!bind(useStrongBinding)) {
                Log.e(TAG, "Failed to establish the service connection.");
                // We have to notify the caller so that they can free-up associated resources.
                // TODO(ppi): Can we hard-fail here?
                notifyChildProcessDied();
            }
        } finally {
            TraceEvent.end("ChildProcessConnection.start");
        }
    }

    /**
     * Sets-up the connection after it was started with start().
     * @param connectionBundle a bundle passed to the service that can be used to pass various
     *         parameters to the service
     * @param clientInterfaces optional client specified interfaces that the child can use to
     *         communicate with the parent process
     * @param connectionCallback will be called exactly once after the connection is set up or the
     *                           setup fails
     */
    public void setupConnection(Bundle connectionBundle, @Nullable List<IBinder> clientInterfaces,
            ConnectionCallback connectionCallback) {
        assert isRunningOnLauncherThread();
        assert mConnectionParams == null;
        if (mServiceDisconnected) {
            Log.w(TAG, "Tried to setup a connection that already disconnected.");
            connectionCallback.onConnected(null);
            return;
        }
        try {
            TraceEvent.begin("ChildProcessConnection.setupConnection");
            mConnectionCallback = connectionCallback;
            mConnectionParams = new ConnectionParams(connectionBundle, clientInterfaces);
            // Run the setup if the service is already connected. If not, doConnectionSetup() will
            // be called from onServiceConnected().
            if (mServiceConnectComplete) {
                doConnectionSetup();
            }
        } finally {
            TraceEvent.end("ChildProcessConnection.setupConnection");
        }
    }

    /**
     * Terminates the connection to IChildProcessService, closing all bindings. It is safe to call
     * this multiple times.
     */
    public void stop() {
        assert isRunningOnLauncherThread();
        unbind();
        notifyChildProcessDied();
    }

    public void kill() {
        assert isRunningOnLauncherThread();
        IChildProcessService service = mService;
        unbind();
        try {
            if (service != null) service.forceKill();
        } catch (RemoteException e) {
            // Intentionally ignore since we are killing it anyway.
        }
        synchronized (sBindingStateLock) {
            mKilledByUs = true;
        }
        notifyChildProcessDied();
    }

    @VisibleForTesting
    protected void onServiceConnectedOnLauncherThread(IBinder service) {
        assert isRunningOnLauncherThread();
        // A flag from the parent class ensures we run the post-connection logic only once
        // (instead of once per each ChildServiceConnection).
        if (mDidOnServiceConnected) {
            return;
        }
        try {
            TraceEvent.begin("ChildProcessConnection.ChildServiceConnection.onServiceConnected");
            mDidOnServiceConnected = true;
            mService = IChildProcessService.Stub.asInterface(service);

            if (mBindToCaller) {
                try {
                    if (!mService.bindToCaller()) {
                        if (mServiceCallback != null) {
                            mServiceCallback.onChildStartFailed(this);
                        }
                        unbind();
                        return;
                    }
                } catch (RemoteException ex) {
                    // Do not trigger the StartCallback here, since the service is already
                    // dead and the onChildStopped callback will run from onServiceDisconnected().
                    Log.e(TAG, "Failed to bind service to connection.", ex);
                    return;
                }
            }

            if (mServiceCallback != null) {
                mServiceCallback.onChildStarted();
            }

            mServiceConnectComplete = true;

            if (mMemoryPressureCallback == null) {
                final MemoryPressureCallback callback = this ::onMemoryPressure;
                ThreadUtils.postOnUiThread(() -> MemoryPressureListener.addCallback(callback));
                mMemoryPressureCallback = callback;
            }

            // Run the setup if the connection parameters have already been provided. If
            // not, doConnectionSetup() will be called from setupConnection().
            if (mConnectionParams != null) {
                doConnectionSetup();
            }
        } finally {
            TraceEvent.end("ChildProcessConnection.ChildServiceConnection.onServiceConnected");
        }
    }

    @VisibleForTesting
    protected void onServiceDisconnectedOnLauncherThread() {
        assert isRunningOnLauncherThread();
        // Ensure that the disconnection logic runs only once (instead of once per each
        // ChildServiceConnection).
        if (mServiceDisconnected) {
            return;
        }
        mServiceDisconnected = true;
        Log.w(TAG, "onServiceDisconnected (crash or killed by oom): pid=%d", mPid);
        stop(); // We don't want to auto-restart on crash. Let the browser do that.

        // If we have a pending connection callback, we need to communicate the failure to
        // the caller.
        if (mConnectionCallback != null) {
            mConnectionCallback.onConnected(null);
            mConnectionCallback = null;
        }
    }

    private void onSetupConnectionResult(int pid) {
        mPid = pid;
        assert mPid != 0 : "Child service claims to be run by a process of pid=0.";

        if (mConnectionCallback != null) {
            mConnectionCallback.onConnected(this);
        }
        mConnectionCallback = null;
    }

    /**
     * Called after the connection parameters have been set (in setupConnection()) *and* a
     * connection has been established (as signaled by onServiceConnected()). These two events can
     * happen in any order.
     */
    private void doConnectionSetup() {
        try {
            TraceEvent.begin("ChildProcessConnection.doConnectionSetup");
            assert mServiceConnectComplete && mService != null;
            assert mConnectionParams != null;

            ICallbackInt pidCallback = new ICallbackInt.Stub() {
                @Override
                public void call(final int pid) {
                    mLauncherHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            onSetupConnectionResult(pid);
                        }
                    });
                }
            };
            try {
                mService.setupConnection(mConnectionParams.mConnectionBundle, pidCallback,
                        mConnectionParams.mClientInterfaces);
            } catch (RemoteException re) {
                Log.e(TAG, "Failed to setup connection.", re);
            }
            mConnectionParams = null;
        } finally {
            TraceEvent.end("ChildProcessConnection.doConnectionSetup");
        }
    }

    private boolean bind(boolean useStrongBinding) {
        assert isRunningOnLauncherThread();
        assert !mUnbound;

        boolean success;
        if (useStrongBinding) {
            success = mStrongBinding.bind();
        } else {
            mModerateBindingCount++;
            success = mModerateBinding.bind();
        }
        if (!success) return false;

        mWaivedBinding.bind();
        updateBindingState();
        return true;
    }

    @VisibleForTesting
    protected void unbind() {
        assert isRunningOnLauncherThread();
        mService = null;
        mConnectionParams = null;
        mUnbound = true;
        mStrongBinding.unbind();
        mWaivedBinding.unbind();
        mModerateBinding.unbind();
        updateBindingState();

        synchronized (sBindingStateLock) {
            mAllBindingStateCountsWhenDied =
                    Arrays.copyOf(sAllBindingStateCounts, NUM_BINDING_STATES);
        }

        if (mMemoryPressureCallback != null) {
            final MemoryPressureCallback callback = mMemoryPressureCallback;
            ThreadUtils.postOnUiThread(() -> MemoryPressureListener.removeCallback(callback));
            mMemoryPressureCallback = null;
        }
    }

    public boolean isStrongBindingBound() {
        assert isRunningOnLauncherThread();
        return mStrongBinding.isBound();
    }

    public void addStrongBinding() {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        if (mStrongBindingCount == 0) {
            mStrongBinding.bind();
            updateBindingState();
        }
        mStrongBindingCount++;
    }

    public void removeStrongBinding() {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        assert mStrongBindingCount > 0;
        mStrongBindingCount--;
        if (mStrongBindingCount == 0) {
            mStrongBinding.unbind();
            updateBindingState();
        }
    }

    public boolean isModerateBindingBound() {
        assert isRunningOnLauncherThread();
        return mModerateBinding.isBound();
    }

    public void addModerateBinding() {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        if (mModerateBindingCount == 0) {
            mModerateBinding.bind();
            updateBindingState();
        }
        mModerateBindingCount++;
    }

    public void removeModerateBinding() {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        assert mModerateBindingCount > 0;
        mModerateBindingCount--;
        if (mModerateBindingCount == 0) {
            mModerateBinding.unbind();
            updateBindingState();
        }
    }

    /**
     * @return true if the connection is bound and only bound with the waived binding or if the
     * connection is unbound and was only bound with the waived binding when it disconnected.
     */
    public @ChildBindingState int bindingStateCurrentOrWhenDied() {
        // WARNING: this method can be called from a thread other than the launcher thread.
        // Note that it returns the current waived bound only state and is racy. This not really
        // preventable without changing the caller's API, short of blocking.
        synchronized (sBindingStateLock) {
            return mBindingStateCurrentOrWhenDied;
        }
    }

    /**
     * @return true if the connection is intentionally killed by calling kill().
     */
    public boolean isKilledByUs() {
        // WARNING: this method can be called from a thread other than the launcher thread.
        // Note that it returns the current waived bound only state and is racy. This not really
        // preventable without changing the caller's API, short of blocking.
        synchronized (sBindingStateLock) {
            return mKilledByUs;
        }
    }

    /**
     * Returns the binding state of remaining processes, excluding the current connection.
     *
     * If the current process is dead then returns the binding state of all processes when it died.
     * Otherwise returns current state.
     */
    public int[] remainingBindingStateCountsCurrentOrWhenDied() {
        // WARNING: this method can be called from a thread other than the launcher thread.
        // Note that it returns the current waived bound only state and is racy. This not really
        // preventable without changing the caller's API, short of blocking.
        synchronized (sBindingStateLock) {
            if (mAllBindingStateCountsWhenDied != null) {
                return Arrays.copyOf(mAllBindingStateCountsWhenDied, NUM_BINDING_STATES);
            }

            int[] counts = Arrays.copyOf(sAllBindingStateCounts, NUM_BINDING_STATES);
            // If current process is still bound then remove it from the counts.
            if (mBindingState != ChildBindingState.UNBOUND) {
                assert counts[mBindingState] > 0;
                counts[mBindingState]--;
            }
            return counts;
        }
    }

    // Should be called any binding is bound or unbound.
    private void updateBindingState() {
        int newBindingState;
        if (mUnbound) {
            newBindingState = ChildBindingState.UNBOUND;
        } else if (mStrongBinding.isBound()) {
            newBindingState = ChildBindingState.STRONG;
        } else if (mModerateBinding.isBound()) {
            newBindingState = ChildBindingState.MODERATE;
        } else {
            assert mWaivedBinding.isBound();
            newBindingState = ChildBindingState.WAIVED;
        }

        synchronized (sBindingStateLock) {
            if (newBindingState != mBindingState) {
                if (mBindingState != ChildBindingState.UNBOUND) {
                    assert sAllBindingStateCounts[mBindingState] > 0;
                    sAllBindingStateCounts[mBindingState]--;
                }
                if (newBindingState != ChildBindingState.UNBOUND) {
                    sAllBindingStateCounts[newBindingState]++;
                }
            }
            mBindingState = newBindingState;
            if (!mUnbound) {
                mBindingStateCurrentOrWhenDied = mBindingState;
            }
        }
    }

    private void notifyChildProcessDied() {
        if (mServiceCallback != null) {
            // Guard against nested calls to this method.
            ServiceCallback serviceCallback = mServiceCallback;
            mServiceCallback = null;
            serviceCallback.onChildProcessDied(this);
        }
    }

    private boolean isRunningOnLauncherThread() {
        return mLauncherHandler.getLooper() == Looper.myLooper();
    }

    @VisibleForTesting
    public void crashServiceForTesting() {
        try {
            mService.forceKill();
        } catch (RemoteException e) {
            // Expected. Ignore.
        }
    }

    @VisibleForTesting
    public boolean didOnServiceConnectedForTesting() {
        return mDidOnServiceConnected;
    }

    @VisibleForTesting
    protected Handler getLauncherHandler() {
        return mLauncherHandler;
    }

    private void onMemoryPressure(@MemoryPressureLevel int pressure) {
        mLauncherHandler.post(() -> onMemoryPressureOnLauncherThread(pressure));
    }

    private void onMemoryPressureOnLauncherThread(@MemoryPressureLevel int pressure) {
        if (mService == null) return;
        try {
            mService.onMemoryPressure(pressure);
        } catch (RemoteException ex) {
            // Ignore
        }
    }
}
