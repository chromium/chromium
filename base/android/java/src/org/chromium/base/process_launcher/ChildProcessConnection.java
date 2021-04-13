// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ChildBindingState;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureLevel;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicInteger;

import javax.annotation.concurrent.GuardedBy;

/**
 * Manages a connection between the browser activity and a child service.
 */
public class ChildProcessConnection {
    private static final String TAG = "ChildProcessConn";
    private static final int NUM_BINDING_STATES = ChildBindingState.MAX_VALUE + 1;
    private static final int FALLBACK_TIMEOUT_IN_SECONDS = 10;

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
     * Run time check if variable number of connections is supported.
     */
    public static boolean supportVariableConnections() {
        return BindService.supportVariableConnections();
    }

    /**
     * The string passed to bindToCaller to identify this class loader.
     */
    @VisibleForTesting
    public static String getBindToCallerClazz() {
        // TODO(crbug.com/1057102): Have embedder explicitly set separate different strings since
        // this could still collide in theory.
        ClassLoader cl = ChildProcessConnection.class.getClassLoader();
        return cl.toString() + cl.hashCode();
    }

    // Global lock to protect all the fields that can be accessed outside launcher thread.
    private static final Object sBindingStateLock = new Object();

    @GuardedBy("sBindingStateLock")
    private static final int[] sAllBindingStateCounts = new int[NUM_BINDING_STATES];

    // The last zygote PID metrics were recorded for.
    private static final AtomicInteger sLastRecordedZygotePid = new AtomicInteger();

    @VisibleForTesting
    static void resetBindingStateCountsForTesting() {
        synchronized (sBindingStateLock) {
            for (int i = 0; i < NUM_BINDING_STATES; ++i) {
                sAllBindingStateCounts[i] = 0;
            }
        }
    }

    // Only accessed on launcher thread.
    private static boolean sFallbackEnabled;

    private final Handler mLauncherHandler;
    private final Executor mLauncherExecutor;
    private ComponentName mServiceName;
    private final ComponentName mFallbackServiceName;

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

    // Factory which tests can override to intercept ChildServiceConnection creation.
    private final ChildServiceConnectionFactory mConnectionFactory;

    // ChildServiceConnectionDelegate for this class which is responsible for posting callbacks to
    // the launcher thread, if needed.
    private final ChildServiceConnectionDelegate mConnectionDelegate;

    // Instance named used on Android 10 and above to create separate instances from the same
    // <service> manifest declaration.
    private final String mInstanceName;

    // Use Context.BIND_EXTERNAL_SERVICE flag for this service.
    private final boolean mBindAsExternalService;

    // Strong binding will make the service priority equal to the priority of the activity.
    private ChildServiceConnection mStrongBinding;

    // Moderate binding will make the service priority equal to the priority of a visible process
    // while the app is in the foreground.
    // This is also used as the initial binding before any priorities are set.
    private ChildServiceConnection mModerateBinding;

    // Uses the BIND_NOT_FOREGROUND flag. This is the same as |mModerateBinding| for memory
    // management and same as |mWaivedBinding| for CPU scheduling.
    private ChildServiceConnection mModerateWaiveCpuBinding;

    // Low priority binding maintained in the entire lifetime of the connection, i.e. between calls
    // to start() and stop().
    private ChildServiceConnection mWaivedBinding;

    // Refcount of bindings.
    private int mStrongBindingCount;
    private int mModerateBindingCount;
    private int mModerateWaiveCpuBindingCount;

    private int mGroup;
    private int mImportanceInGroup;

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

    // If the process threw an exception before entering the main loop, the exception
    // string is reported here.
    @GuardedBy("sBindingStateLock")
    private String mExceptionInServiceDuringInit;

    // Whether the process exited cleanly or not.
    @GuardedBy("sBindingStateLock")
    private boolean mCleanExit;

    public ChildProcessConnection(Context context, ComponentName serviceName,
            ComponentName fallbackServiceName, boolean bindToCaller, boolean bindAsExternalService,
            Bundle serviceBundle, String instanceName) {
        this(context, serviceName, fallbackServiceName, bindToCaller, bindAsExternalService,
                serviceBundle, null /* connectionFactory */, instanceName);
    }

    @VisibleForTesting
    public ChildProcessConnection(final Context context, ComponentName serviceName,
            ComponentName fallbackServiceName, boolean bindToCaller, boolean bindAsExternalService,
            Bundle serviceBundle, ChildServiceConnectionFactory connectionFactory,
            String instanceName) {
        mLauncherHandler = new Handler();
        mLauncherExecutor = (Runnable runnable) -> {
            mLauncherHandler.post(runnable);
        };
        assert isRunningOnLauncherThread();
        mServiceName = serviceName;
        mFallbackServiceName = fallbackServiceName;
        mServiceBundle = serviceBundle != null ? serviceBundle : new Bundle();
        mServiceBundle.putBoolean(ChildProcessConstants.EXTRA_BIND_TO_CALLER, bindToCaller);
        mServiceBundle.putString(ChildProcessConstants.EXTRA_BROWSER_PACKAGE_NAME,
                BuildInfo.getInstance().packageName);
        mBindToCaller = bindToCaller;
        mInstanceName = instanceName;
        mBindAsExternalService = bindAsExternalService;
        if (connectionFactory == null) {
            mConnectionFactory = new ChildServiceConnectionFactory() {
                @Override
                public ChildServiceConnection createConnection(Intent bindIntent, int bindFlags,
                        ChildServiceConnectionDelegate delegate, String instanceName) {
                    return new ChildServiceConnectionImpl(context, bindIntent, bindFlags,
                            mLauncherHandler, mLauncherExecutor, delegate, instanceName);
                }
            };
        } else {
            mConnectionFactory = connectionFactory;
        }

        // Methods on the delegate are can be called on launcher thread or UI thread, so need to
        // handle both cases. See BindService for details.
        mConnectionDelegate = new ChildServiceConnectionDelegate() {
            @Override
            public void onServiceConnected(final IBinder service) {
                if (mLauncherHandler.getLooper() == Looper.myLooper()) {
                    onServiceConnectedOnLauncherThread(service);
                    return;
                }
                mLauncherHandler.post(() -> onServiceConnectedOnLauncherThread(service));
            }

            @Override
            public void onServiceDisconnected() {
                if (mLauncherHandler.getLooper() == Looper.myLooper()) {
                    onServiceDisconnectedOnLauncherThread();
                    return;
                }
                mLauncherHandler.post(() -> onServiceDisconnectedOnLauncherThread());
            }
        };

        createBindings(sFallbackEnabled && mFallbackServiceName != null ? mFallbackServiceName
                                                                        : mServiceName);
    }

    private void createBindings(ComponentName serviceName) {
        Intent intent = new Intent();
        intent.setComponent(serviceName);
        if (mServiceBundle != null) {
            intent.putExtras(mServiceBundle);
        }

        int defaultFlags = Context.BIND_AUTO_CREATE
                | (mBindAsExternalService ? Context.BIND_EXTERNAL_SERVICE : 0);

        mModerateBinding = mConnectionFactory.createConnection(
                intent, defaultFlags, mConnectionDelegate, mInstanceName);
        mModerateWaiveCpuBinding = mConnectionFactory.createConnection(intent,
                defaultFlags | Context.BIND_NOT_FOREGROUND, mConnectionDelegate, mInstanceName);
        mStrongBinding = mConnectionFactory.createConnection(
                intent, defaultFlags | Context.BIND_IMPORTANT, mConnectionDelegate, mInstanceName);
        mWaivedBinding = mConnectionFactory.createConnection(intent,
                defaultFlags | Context.BIND_WAIVE_PRIORITY, mConnectionDelegate, mInstanceName);
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

    // This is the same as start, but returns a boolean whether bind succeeded. Also on failure,
    // no method is called on |serviceCallback| so the allocation can be tried again. This is
    // package private and is meant to be used by Android10WorkaroundAllocatorImpl. See comment
    // there for details.
    boolean tryStart(boolean useStrongBinding, ServiceCallback serviceCallback) {
        try {
            TraceEvent.begin("ChildProcessConnection.tryStart");
            assert isRunningOnLauncherThread();
            assert mConnectionParams
                    == null : "setupConnection() called before start() in ChildProcessConnection.";

            if (!bind(useStrongBinding)) {
                return false;
            }
            mServiceCallback = serviceCallback;
        } finally {
            TraceEvent.end("ChildProcessConnection.tryStart");
        }
        return true;
    }

    /**
     * Call bindService again on this connection. This must be called while connection is already
     * bound. This is useful for controlling the recency of this connection, and also for updating
     */
    public void rebind() {
        assert isRunningOnLauncherThread();
        if (!isConnected()) return;
        assert mWaivedBinding.isBound();
        mWaivedBinding.bindServiceConnection();
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

    /**
     * Dumps the stack of the child process without crashing it.
     */
    public void dumpProcessStack() {
        assert isRunningOnLauncherThread();
        IChildProcessService service = mService;
        try {
            if (service != null) service.dumpProcessStack();
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to dump process stack.", e);
        }
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
                    if (!mService.bindToCaller(getBindToCallerClazz())) {
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
        Log.w(TAG, "onServiceDisconnected (crash or killed by oom): pid=%d %s", mPid,
                buildDebugStateString());
        stop(); // We don't want to auto-restart on crash. Let the browser do that.

        // If we have a pending connection callback, we need to communicate the failure to
        // the caller.
        if (mConnectionCallback != null) {
            mConnectionCallback.onConnected(null);
            mConnectionCallback = null;
        }
    }

    private String buildDebugStateString() {
        StringBuilder s = new StringBuilder();
        s.append("bindings:");
        s.append(mWaivedBinding.isBound() ? "W" : " ");
        s.append(mModerateBinding.isBound() ? "M" : " ");
        s.append(mModerateWaiveCpuBinding.isBound() ? "C" : " ");
        s.append(mStrongBinding.isBound() ? "S" : " ");

        synchronized (sBindingStateLock) {
            s.append(" state:").append(mBindingState);
            s.append(" counts:");
            for (int i = 0; i < NUM_BINDING_STATES; ++i) {
                s.append(sAllBindingStateCounts[i]).append(",");
            }
        }
        return s.toString();
    }

    private void onSetupConnectionResult(int pid) {
        if (mPid != 0) {
            Log.e(TAG, "sendPid was called more than once: pid=%d", mPid);
            return;
        }
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

            IParentProcess parentProcess = new IParentProcess.Stub() {
                @Override
                public void sendPid(final int pid) {
                    mLauncherHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            onSetupConnectionResult(pid);
                        }
                    });
                }

                @Override
                public void reportExceptionInInit(String exception) {
                    synchronized (sBindingStateLock) {
                        mExceptionInServiceDuringInit = exception;
                    }
                    mLauncherHandler.post(createUnbindRunnable());
                }

                @Override
                public void reportCleanExit() {
                    synchronized (sBindingStateLock) {
                        mCleanExit = true;
                    }
                    mLauncherHandler.post(createUnbindRunnable());
                }

                @Override
                public void sendZygoteInfo(int zygotePid, long zygoteStartupTimeMillis) {
                    // Only record the zygote startup time for a process the first time it is sent.
                    // The zygote may get killed and recreated, so keep track of the last PID
                    // recorded to avoid double counting. The app may reuse a zygote process if the
                    // app is stopped and started again quickly, so the startup time of that zygote
                    // may be recorded multiple times. There's not much we can do about that, and it
                    // shouldn't be a major issue.
                    if (sLastRecordedZygotePid.getAndSet(zygotePid) != zygotePid) {
                        RecordHistogram.recordMediumTimesHistogram(
                                "Android.ChildProcessStartTimeV2.Zygote", zygoteStartupTimeMillis);
                    }
                }

                private Runnable createUnbindRunnable() {
                    return new Runnable() {
                        @Override
                        public void run() {
                            unbind();
                        }
                    };
                }
            };
            try {
                mService.setupConnection(mConnectionParams.mConnectionBundle, parentProcess,
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
            success = mStrongBinding.bindServiceConnection();
        } else {
            mModerateBindingCount++;
            success = mModerateBinding.bindServiceConnection();
        }
        if (!success) return false;

        if (!sFallbackEnabled && mFallbackServiceName != null) {
            mLauncherHandler.postDelayed(
                    this::checkBindTimeOut, FALLBACK_TIMEOUT_IN_SECONDS * 1000);
        }

        mWaivedBinding.bindServiceConnection();
        updateBindingState();
        return true;
    }

    // NOTE: Keep values in sync with OnServiceConnectedTimedOutResult in enums.xml.
    @Retention(RetentionPolicy.SOURCE)
    public @interface TimeoutResult {
        int ALREADY_CONNECTED = 0;
        int NOT_NEEDED = 1;
        int FALLBACK = 2;
        int NUM_ENTRIES = 3;
    }

    private void checkBindTimeOut() {
        assert isRunningOnLauncherThread();
        assert mFallbackServiceName != null;
        final String histogramName =
                "Android.ChildProcessLauncher.OnServiceConnectedTimedOutResult";
        if (mDidOnServiceConnected || mServiceDisconnected) {
            RecordHistogram.recordEnumeratedHistogram(
                    histogramName, TimeoutResult.ALREADY_CONNECTED, TimeoutResult.NUM_ENTRIES);
            return;
        }
        if (mUnbound) {
            RecordHistogram.recordEnumeratedHistogram(
                    histogramName, TimeoutResult.NOT_NEEDED, TimeoutResult.NUM_ENTRIES);
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                histogramName, TimeoutResult.FALLBACK, TimeoutResult.NUM_ENTRIES);
        Log.w(TAG, "Fallback to " + mFallbackServiceName);
        sFallbackEnabled = true;
        boolean isStrongBindingBound = mStrongBinding.isBound();
        boolean isModerateBindingBound = mModerateBinding.isBound();
        boolean isModerateWaiveCpuBindingBound = mModerateWaiveCpuBinding.isBound();
        boolean isWaivedBindingBound = mWaivedBinding.isBound();
        mStrongBinding.retire();
        mModerateBinding.retire();
        mModerateWaiveCpuBinding.retire();
        mWaivedBinding.retire();
        createBindings(mFallbackServiceName);
        if (isStrongBindingBound) mStrongBinding.bindServiceConnection();
        if (isModerateBindingBound) mModerateBinding.bindServiceConnection();
        if (isModerateWaiveCpuBindingBound) {
            mModerateWaiveCpuBinding.bindServiceConnection();
        }
        if (isWaivedBindingBound) mWaivedBinding.bindServiceConnection();
    }

    @VisibleForTesting
    protected void unbind() {
        assert isRunningOnLauncherThread();
        mService = null;
        mConnectionParams = null;
        mUnbound = true;
        mStrongBinding.unbindServiceConnection();
        mWaivedBinding.unbindServiceConnection();
        mModerateBinding.unbindServiceConnection();
        mModerateWaiveCpuBinding.unbindServiceConnection();
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

    public void updateGroupImportance(int group, int importanceInGroup) {
        assert isRunningOnLauncherThread();
        if (!isConnected()) return;
        assert !mUnbound;
        assert mWaivedBinding.isBound();
        assert group != 0 || importanceInGroup == 0;
        if (mGroup != group || mImportanceInGroup != importanceInGroup) {
            mGroup = group;
            mImportanceInGroup = importanceInGroup;
            mWaivedBinding.updateGroupImportance(group, importanceInGroup);
        }
    }

    public int getGroup() {
        assert isRunningOnLauncherThread();
        return mGroup;
    }

    public int getImportanceInGroup() {
        assert isRunningOnLauncherThread();
        return mImportanceInGroup;
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
            mStrongBinding.bindServiceConnection();
            if (mModerateWaiveCpuBinding.isBound()) {
                mModerateWaiveCpuBinding.unbindServiceConnection();
            }
            updateBindingState();
        }
        mStrongBindingCount++;
    }

    public void removeStrongBinding() {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            return;
        }
        assert mStrongBindingCount > 0;
        mStrongBindingCount--;
        if (mStrongBindingCount == 0) {
            if (mModerateWaiveCpuBindingCount > 0 && !mModerateWaiveCpuBinding.isBound()
                    && !mModerateBinding.isBound()) {
                mModerateWaiveCpuBinding.bindServiceConnection();
            }
            mStrongBinding.unbindServiceConnection();
            updateBindingState();
        }
    }

    public boolean isModerateBindingBound() {
        assert isRunningOnLauncherThread();
        return mModerateBinding.isBound() || mModerateWaiveCpuBinding.isBound();
    }

    /**
     * @param waiveCpuPriority Normally moderate binding may raise the CPU scheduling priority
     * as well as the importance for memory management. Pass true to not affect CPU scheduling
     * priority. Note the refcounts for waiveCpuPriority and not are separate,
     * so removeModerateBinding parameter must match.
     */
    public void addModerateBinding(boolean waiveCpuPriority) {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        if (waiveCpuPriority) {
            if (mModerateWaiveCpuBindingCount == 0 && !mStrongBinding.isBound()
                    && !mModerateBinding.isBound()) {
                mModerateWaiveCpuBinding.bindServiceConnection();
                updateBindingState();
            }
            mModerateWaiveCpuBindingCount++;
            return;
        }
        if (mModerateBindingCount == 0) {
            mModerateBinding.bindServiceConnection();
            if (mModerateWaiveCpuBinding.isBound()) {
                mModerateWaiveCpuBinding.unbindServiceConnection();
            }
            updateBindingState();
        }
        mModerateBindingCount++;
    }

    /**
     * @param waiveCpuPriority See addModerateBinding.
     */
    public void removeModerateBinding(boolean waiveCpuPriority) {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            return;
        }
        if (waiveCpuPriority) {
            assert mModerateWaiveCpuBindingCount > 0;
            mModerateWaiveCpuBindingCount--;
            if (mModerateWaiveCpuBindingCount == 0 && mModerateWaiveCpuBinding.isBound()) {
                mModerateWaiveCpuBinding.unbindServiceConnection();
                updateBindingState();
            }
            return;
        }
        assert mModerateBindingCount > 0;
        mModerateBindingCount--;
        if (mModerateBindingCount == 0) {
            if (mModerateWaiveCpuBindingCount > 0 && !mModerateWaiveCpuBinding.isBound()
                    && !mStrongBinding.isBound()) {
                mModerateWaiveCpuBinding.bindServiceConnection();
            }
            mModerateBinding.unbindServiceConnection();
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
     * @return true if the process exited cleanly.
     */
    public boolean hasCleanExit() {
        synchronized (sBindingStateLock) {
            return mCleanExit;
        }
    }

    /**
     * @return the exception string if service threw an exception during init.
     *         null otherwise.
     */
    public @Nullable String getExceptionDuringInit() {
        synchronized (sBindingStateLock) {
            return mExceptionInServiceDuringInit;
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
        } else if (mModerateBinding.isBound() || mModerateWaiveCpuBinding.isBound()) {
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
