// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApkInfo;
import org.chromium.base.BaseFeatureList;
import org.chromium.base.ChildBindingState;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureLevel;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.IRelroLibInfo;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.memory.SelfFreezeCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executor;

import javax.annotation.concurrent.GuardedBy;

/** Manages a connection between the browser activity and a child service. */
@NullMarked
public class ChildProcessConnection {
    private static final String TAG = "ChildProcessConn";
    private static final int FALLBACK_TIMEOUT_IN_SECONDS = 10;
    private static final String HISTOGRAM_NAME = "Android.ChildProcessConectionEventCounts";

    /**
     * Used to notify the consumer about the process start. These callbacks will be invoked before
     * the ConnectionCallbacks.
     */
    public interface ServiceCallback {
        /**
         * Called when the child process has successfully started and is ready for connection setup.
         */
        void onChildStarted();

        /**
         * Called when the child process failed to start. This can happen if the process is already
         * in use by another client. The client will not receive any other callbacks after this one.
         */
        void onChildStartFailed(ChildProcessConnection connection);

        /**
         * Called when the service has been disconnected. whether it was stopped by the client or if
         * it stopped unexpectedly (process crash). This is the last callback from this interface
         * that a client will receive for a specific connection.
         */
        void onChildProcessDied(ChildProcessConnection connection);
    }

    /** Used to notify the consumer about the connection being established. */
    public interface ConnectionCallback {
        /**
         * Called when the connection to the service is established.
         *
         * @param connection the connection object to the child process
         */
        void onConnected(@Nullable ChildProcessConnection connection);
    }

    /**
     * Used to notify the client about the new shared relocations (RELRO) arriving from the app
     * zygote.
     */
    public interface ZygoteInfoCallback {
        /**
         * Called after the connection has been established, only once per process being connected
         * to.
         *
         * @param connection the connection object to the child process. Must hold the zygote PID
         *     that the child process was spawned from
         * @param relroInfo the IRelroLibInfo potentially containing the information making it
         *     possible to replace the current RELRO address range to memory shared with the zpp
         *     zygote. Needs to be passed to the library loader in order to take effect. Can be done
         *     before or after the LibraryLoader loads the library.
         */
        void onReceivedZygoteInfo(
                ChildProcessConnection connection, @Nullable IRelroLibInfo relroInfo);
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        EventsEnum.SCHEDULE_TIMEOUT_SANDBOXED,
        EventsEnum.SCHEDULE_TIMEOUT_UNSANDBOXED,
        EventsEnum.FALLBACK_ON_TIMEOUT_SANDBOXED,
        EventsEnum.FALLBACK_ON_TIMEOUT_UNSANDBOXED,
        EventsEnum.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface EventsEnum {
        int SCHEDULE_TIMEOUT_SANDBOXED = 0;
        int SCHEDULE_TIMEOUT_UNSANDBOXED = 1;
        int FALLBACK_ON_TIMEOUT_SANDBOXED = 2;
        int FALLBACK_ON_TIMEOUT_UNSANDBOXED = 3;
        int COUNT = 4;
    }

    /**
     * Count the ChildServiceConnectionDelegate.onServiceConnected callback.
     *
     * <p>This is to detect the service binding restart. If the counter is more than 1, it means the
     * service is restarted (e.g. due to LMK, crash).
     *
     * <p>onServiceDisconnectedOnLauncherThread() unbinds all service bindings when the child
     * process dies to prevent the service from restarting. However there is race and the child
     * process can restart. The metrics "Android.ChildProcessConnection.OnServiceConnectedCounts" is
     * to understand how much restarts happen in practice.
     *
     * <p>This is expected to be used for a service binding which is bound once for the service
     * lifetime. In ChildProcessConnection, the CountOnServiceConnectedDecorator won't double count
     * onServiceConnected because the ChildServiceConnectionController never replaces service intent
     * by replaceService(), which causes rebinding for all underlying bindings, if
     * onServiceConnected is triggered as implemented in bind() and checkBindTimeOut().
     */
    private static class CountOnServiceConnectedDecorator
            implements ChildServiceConnectionDelegate {
        private final ChildServiceConnectionDelegate mDelegate;
        private final Handler mLauncherHandler;

        private int mCountOnServiceConnected;

        CountOnServiceConnectedDecorator(
                ChildServiceConnectionDelegate delegate, Handler launcherHandler) {
            mDelegate = delegate;
            mLauncherHandler = launcherHandler;
        }

        @Override
        public void onServiceConnected(final IBinder service) {
            mDelegate.onServiceConnected(service);
            if (mLauncherHandler.getLooper() == Looper.myLooper()) {
                incrementCount();
                return;
            }
            mLauncherHandler.post(() -> incrementCount());
        }

        @Override
        public void onServiceDisconnected() {
            mDelegate.onServiceDisconnected();
        }

        private void incrementCount() {
            mCountOnServiceConnected += 1;
            RecordHistogram.recordCount100Histogram(
                    "Android.ChildProcessConnection.OnServiceConnectedCounts",
                    mCountOnServiceConnected);
        }
    }

    private static class ChildProcessMismatchException extends RuntimeException {
        ChildProcessMismatchException(String msg) {
            super(msg);
        }
    }

    /** Run time check if variable number of connections is supported. */
    public static boolean supportVariableConnections() {
        return BindService.supportVariableConnections();
    }

    /** The string passed to bindToCaller to identify this class loader. */
    @VisibleForTesting
    public static String getBindToCallerClazz() {
        // TODO(crbug.com/40677220): Have embedder explicitly set separate different strings since
        // this could still collide in theory.
        ClassLoader cl = ChildProcessConnection.class.getClassLoader();
        return cl.toString() + cl.hashCode();
    }

    // The last zygote PID for which the zygote startup metrics were recorded. Lives on the
    // launcher thread.
    private static int sLastRecordedZygotePid;

    // Only accessed on launcher thread.
    // Set when a launching a service with zygote times out, in which case
    // it's assumed the zygote code path is not functional. Make all future
    // launches directly fallback without timeout to minimize user impact.
    private static boolean sAlwaysFallback;

    // Lock to protect all the fields that can be accessed outside launcher thread.
    private final Object mProcessStateLock = new Object();

    private final Handler mLauncherHandler;
    private final Executor mLauncherExecutor;
    private final ComponentName mServiceName;
    private final @Nullable ComponentName mFallbackServiceName;

    // Parameters passed to the child process through the service binding intent.
    // If the service gets recreated by the framework the intent will be reused, so these parameters
    // should be common to all processes of that type.
    private final Bundle mServiceBundle;

    // Whether bindToCaller should be called on the service after setup to check that only one
    // process is bound to the service.
    private final boolean mBindToCaller;

    private static class ConnectionParams {
        final IChildProcessArgs mChildProcessArgs;
        final @Nullable List<IBinder> mClientInterfaces;

        ConnectionParams(
                IChildProcessArgs childProcessArgs, @Nullable List<IBinder> clientInterfaces) {
            mChildProcessArgs = childProcessArgs;
            mClientInterfaces = clientInterfaces;
        }
    }

    // This is set in start() and is used in onServiceConnected().
    private @Nullable ServiceCallback mServiceCallback;

    // This is set in setupConnection() and is later used in doConnectionSetup(), after which the
    // variable is cleared. Therefore this is only valid while the connection is being set up.
    private @Nullable ConnectionParams mConnectionParams;

    // Callback provided in setupConnection() that will communicate the result to the caller. This
    // has to be called exactly once after setupConnection(), even if setup fails, so that the
    // caller can free up resources associated with the setup attempt. This is set to null after the
    // call.
    private @Nullable ConnectionCallback mConnectionCallback;

    // Callback provided in setupConnection().
    private @Nullable ZygoteInfoCallback mZygoteInfoCallback;

    private @Nullable IChildProcessService mService;

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

    // The PID of the app zygote that the child process was spawned from. Zero if no useful
    // information about the app zygote can be obtained.
    private int mZygotePid;

    /**
     * @return true iff the child process notified that it has usable info from the App Zygote.
     */
    public boolean hasUsableZygoteInfo() {
        assert isRunningOnLauncherThread();
        return mZygotePid != 0;
    }

    // If true, then this connection fallbacking back does not cause other connections to fallback,
    // and vice version; essentially ignore `sAlwaysFallback`.
    private final boolean mIndependentFallback;

    // Should not be used for any functional changes as this class should be oblivious to whether
    // this child process is sandboxed or not. Only added here for histogram purposes since it's
    // inconvenient to log some histogram where this information is available.
    private final boolean mIsSandboxedForHistograms;

    private final ChildServiceConnectionController mConnectionController;

    // Refcount of bindings.
    private int mStrongBindingCount;
    private int mVisibleBindingCount;
    private int mNotPerceptibleBindingCount;

    private int mGroup;
    private int mImportanceInGroup;

    // Indicate |kill()| was called to intentionally kill this process.
    @GuardedBy("mProcessStateLock")
    private boolean mKilledByUs;

    private @Nullable MemoryPressureCallback mMemoryPressureCallback;
    private @Nullable SelfFreezeCallback mSelfFreezeCallback;

    // If the process threw an exception before entering the main loop, the exception
    // string is reported here.
    @GuardedBy("mProcessStateLock")
    private @Nullable String mExceptionInServiceDuringInit;

    // Whether the process exited cleanly or not.
    @GuardedBy("mProcessStateLock")
    private boolean mCleanExit;

    // Whether the EffectiveBindingState feature is enabled.
    //
    // The feature status has to stay consistent throughout the lifetime of this object, and can't
    // have it flip half way in the middle.
    private final boolean mIsEffectiveBindingStateEnabled;

    // Whether the UseIsUnboundCheck feature is enabled.
    //
    // The feature status has to stay consistent throughout the lifetime of this object, and can't
    // have it flip half way in the middle.
    private final boolean mUseIsUnboundCheck;

    public ChildProcessConnection(
            Context context,
            ComponentName serviceName,
            @Nullable ComponentName fallbackServiceName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            Bundle serviceBundle,
            @Nullable String instanceName,
            boolean independentFallback,
            boolean isSandboxedForHistograms) {
        this(
                context,
                serviceName,
                fallbackServiceName,
                bindToCaller,
                bindAsExternalService,
                serviceBundle,
                /* connectionFactory= */ null,
                instanceName,
                independentFallback,
                isSandboxedForHistograms);
    }

    @VisibleForTesting
    public ChildProcessConnection(
            final Context context,
            ComponentName serviceName,
            @Nullable ComponentName fallbackServiceName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            Bundle serviceBundle,
            @Nullable ChildServiceConnectionFactory connectionFactory,
            @Nullable String instanceName,
            boolean independentFallback,
            boolean isSandboxedForHistograms) {
        mLauncherHandler = new Handler();
        mLauncherExecutor =
                (Runnable runnable) -> {
                    mLauncherHandler.post(runnable);
                };
        assert isRunningOnLauncherThread();
        mServiceName = serviceName;
        mFallbackServiceName = fallbackServiceName;
        mServiceBundle = serviceBundle != null ? serviceBundle : new Bundle();
        mServiceBundle.putString(
                ChildProcessConstants.EXTRA_BROWSER_PACKAGE_NAME, ApkInfo.getPackageName());
        mBindToCaller = bindToCaller;
        mIndependentFallback = independentFallback;
        mIsSandboxedForHistograms = isSandboxedForHistograms;
        mIsEffectiveBindingStateEnabled = BaseFeatureList.sEffectiveBindingState.isEnabled();
        mUseIsUnboundCheck = BaseFeatureList.sUseIsUnboundCheck.isEnabled();

        // Incremental install does not work with isolatedProcess, and externalService requires
        // isolatedProcess, so both need to be turned off for incremental install.
        int defaultBindFlags =
                Context.BIND_AUTO_CREATE
                        | ((bindAsExternalService && !BuildConfig.IS_INCREMENTAL_INSTALL)
                                ? Context.BIND_EXTERNAL_SERVICE
                                : 0);

        if (connectionFactory == null) {
            connectionFactory =
                    new ChildServiceConnectionFactory() {
                        @Override
                        public ChildServiceConnection createConnection(
                                Intent bindIntent,
                                int bindFlags,
                                ChildServiceConnectionDelegate delegate,
                                @Nullable String instanceName) {
                            return new ChildServiceConnectionImpl(
                                    context,
                                    bindIntent,
                                    bindFlags,
                                    mLauncherHandler,
                                    mLauncherExecutor,
                                    delegate,
                                    instanceName);
                        }
                    };
        }

        // Methods on the delegate are can be called on launcher thread or UI thread, so need to
        // handle both cases. See BindService for details.
        ChildServiceConnectionDelegate connectionDelegate =
                new ChildServiceConnectionDelegate() {
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

        Intent bindIntent = new Intent();
        bindIntent.setComponent(
                getAlwaysFallback() && mFallbackServiceName != null
                        ? mFallbackServiceName
                        : mServiceName);
        if (mServiceBundle != null) {
            bindIntent.putExtras(mServiceBundle);
        }
        if (mIsEffectiveBindingStateEnabled
                && RebindingChildServiceConnectionController.isEnabled()) {
            mConnectionController =
                    new RebindingChildServiceConnectionController(
                            connectionFactory,
                            bindIntent,
                            defaultBindFlags,
                            new CountOnServiceConnectedDecorator(
                                    connectionDelegate, mLauncherHandler),
                            instanceName);
        } else {
            mConnectionController =
                    new LegacyChildServiceConnectionController(
                            connectionFactory,
                            bindIntent,
                            defaultBindFlags,
                            connectionDelegate,
                            new CountOnServiceConnectedDecorator(
                                    connectionDelegate, mLauncherHandler),
                            instanceName);
        }
    }

    public final @Nullable IChildProcessService getService() {
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
     * @return whether the connection to the child process is established and the connected process
     *     is still alive.
     */
    public boolean isConnectedProcessAlive() {
        return mService != null && mService.asBinder().pingBinder();
    }

    /**
     * @return the connection pid, or 0 if not yet connected
     */
    public int getPid() {
        assert isRunningOnLauncherThread();
        return mPid;
    }

    /**
     * @return the app zygote PID known by the connection.
     */
    public int getZygotePid() {
        assert isRunningOnLauncherThread();
        return mZygotePid;
    }

    /**
     * Starts a connection to an IChildProcessService. This must be followed by a call to
     * setupConnection() to setup the connection parameters. start() and setupConnection() are
     * separate to allow to pass whatever parameters are available in start(), and complete the
     * remainder addStrongBinding while reducing the connection setup latency.
     *
     * @param useStrongBinding whether a strong binding should be bound by default. If false, an
     *     initial moderate binding is used.
     * @param serviceCallback (optional) callbacks invoked when the child process starts or fails to
     *     start and when the service stops.
     */
    public void start(@ChildBindingState int initialBindingState, ServiceCallback serviceCallback) {
        // TODO(https://crbug.com/446030170): Consider taking ChildProcessImportance as a parameter
        // instead. This is not trivial because ChildProcessImportance is in the content layer.
        assert initialBindingState != ChildBindingState.UNBOUND;
        try {
            TraceEvent.begin("ChildProcessConnection.start");
            assert isRunningOnLauncherThread();
            assert mConnectionParams == null
                    : "setupConnection() called before start() in ChildProcessConnection.";

            mServiceCallback = serviceCallback;

            if (!bind(initialBindingState)) {
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
    boolean tryStart(@ChildBindingState int initialBindingState, ServiceCallback serviceCallback) {
        try {
            TraceEvent.begin("ChildProcessConnection.tryStart");
            assert isRunningOnLauncherThread();
            assert mConnectionParams == null
                    : "setupConnection() called before start() in ChildProcessConnection.";

            if (!bind(initialBindingState)) {
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
        mConnectionController.rebind();
    }

    /**
     * Sets-up the connection after it was started with start().
     *
     * @param childProcessArgs an aidl interface with all miscellaneous parameters for the child
     *     process connection.
     * @param clientInterfaces optional client specified interfaces that the child can use to
     *     communicate with the parent process
     * @param connectionCallback will be called exactly once after the connection is set up or the
     *     setup fails
     * @param zygoteInfoCallback will be called exactly once after the connection is set up
     */
    public void setupConnection(
            IChildProcessArgs childProcessArgs,
            @Nullable List<IBinder> clientInterfaces,
            ConnectionCallback connectionCallback,
            ZygoteInfoCallback zygoteInfoCallback) {
        assert isRunningOnLauncherThread();
        assert mConnectionParams == null;
        if (mServiceDisconnected) {
            Log.w(TAG, "Tried to setup a connection that already disconnected.");
            connectionCallback.onConnected(null);
            return;
        }
        try (TraceEvent te = TraceEvent.scoped("ChildProcessConnection.setupConnection")) {
            mConnectionCallback = connectionCallback;
            mZygoteInfoCallback = zygoteInfoCallback;
            childProcessArgs.bindToCaller = mBindToCaller;
            mConnectionParams = new ConnectionParams(childProcessArgs, clientInterfaces);
            // Run the setup if the service is already connected. If not, doConnectionSetup() will
            // be called from onServiceConnected().
            if (mServiceConnectComplete) {
                doConnectionSetup();
            }
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
        synchronized (mProcessStateLock) {
            mKilledByUs = true;
        }
        notifyChildProcessDied();
    }

    /** Dumps the stack of the child process without crashing it. */
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

            // Validate that the child process is running the same code as the parent process.
            boolean childMatches = true;
            try {
                String[] childAppInfoStrings = mService.getAppInfoStrings();

                ApplicationInfo parentAppInfo = ApkInfo.getInstance().getBrowserApplicationInfo();
                String[] parentAppInfoStrings = ChildProcessService.convertToStrings(parentAppInfo);

                // Don't compare splitSourceDirs as isolatedSplits/dynamic feature modules/etc
                // make this potentially complicated.
                childMatches = Arrays.equals(parentAppInfoStrings, childAppInfoStrings);
            } catch (RemoteException ex) {
                // If the child can't handle getAppInfo then it is old and doesn't match.
                childMatches = false;
            }
            if (!childMatches) {
                // Check if it looks like the browser's package version has been changed since the
                // browser process launched (i.e. if the install somehow did not kill our process)
                PackageInfo latestPackage = PackageUtils.getApplicationPackageInfo(0);
                long latestVersionCode = latestPackage.getLongVersionCode();
                long loadedVersionCode = BuildConfig.VERSION_CODE;
                if (latestVersionCode != loadedVersionCode) {
                    // Crashing the process is likely to improve the situation - when we are next
                    // launched, we should be running the new version and match new children.
                    throw new ChildProcessMismatchException(
                            "Child process's classpath doesn't match, and main process's package"
                                    + " has been updated since process launch; process needs"
                                    + " restarting!");
                } else {
                    // Crashing the process is unlikely to improve the situation - our classpath
                    // will probably be the same on next launch and probably still won't match.
                    // Log an error but just carry on and hope.
                    Log.e(
                            TAG,
                            "Child process's classpath doesn't match, but main process's package"
                                    + " hasn't changed; the child is likely to be broken!");
                }
            }

            if (mServiceCallback != null) {
                mServiceCallback.onChildStarted();
            }

            mServiceConnectComplete = true;

            if (mMemoryPressureCallback == null) {
                final MemoryPressureCallback callback = this::onMemoryPressure;
                ThreadUtils.postOnUiThread(() -> MemoryPressureListener.addCallback(callback));
                mMemoryPressureCallback = callback;
            }

            if (mSelfFreezeCallback == null) {
                final SelfFreezeCallback callback = this::onSelfFreeze;
                MemoryPressureListener.addSelfFreezeCallback(callback);
                mSelfFreezeCallback = callback;
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
        Log.w(
                TAG,
                "onServiceDisconnected (crash or killed by oom): pid=%d %s",
                mPid,
                buildDebugStateString());
        stop(); // We don't want to auto-restart on crash. Let the browser do that.

        // If we have a pending connection callback, we need to communicate the failure to
        // the caller.
        if (mConnectionCallback != null) {
            mConnectionCallback.onConnected(null);
            mConnectionCallback = null;
        }
    }

    /**
     * @return the detailed binding state for debugging/metrics purposes.
     */
    public ChildProcessConnectionState getConnectionStateForDebugging() {
        return mConnectionController.getConnectionStateForDebugging();
    }

    private String buildDebugStateString() {
        ChildProcessConnectionState connectionState =
                mConnectionController.getConnectionStateForDebugging();
        StringBuilder s = new StringBuilder();
        s.append("bindings:");
        s.append(connectionState.mIsWaivedBound ? "W" : " ");
        s.append(connectionState.mIsVisibleBound ? "V" : " ");
        s.append(connectionState.mIsNotPerceptibleBound ? "N" : " ");
        s.append(connectionState.mIsStrongBound ? "S" : " ");
        return s.toString();
    }

    private void onSetupConnectionResultOnLauncherThread(
            int pid,
            int zygotePid,
            long zygoteStartupTimeMillis,
            @Nullable IRelroLibInfo relroInfo) {
        assert isRunningOnLauncherThread();

        // The RELRO parcel should be accepted only when establishing the connection. This is to
        // prevent untrusted code from controlling shared memory regions in other processes. Make
        // further IPCs a noop.
        if (mPid != 0) {
            Log.e(TAG, "Pid was sent more than once: pid=%d", mPid);
            return;
        }
        mPid = pid;
        assert mPid != 0 : "Child service claims to be run by a process of pid=0.";

        // Remember zygote pid to detect zygote restarts later.
        mZygotePid = zygotePid;

        // Newly arrived zygote info sometimes needs to be broadcast to a number of processes.
        if (mZygoteInfoCallback != null) {
            mZygoteInfoCallback.onReceivedZygoteInfo(this, relroInfo);
        }
        mZygoteInfoCallback = null;

        // Only record the zygote startup time for a process the first time it is sent.  The zygote
        // may get killed and recreated, so keep track of the last PID recorded to avoid double
        // counting. The app may reuse a zygote process if the app is stopped and started again
        // quickly, so the startup time of that zygote may be recorded multiple times. There's not
        // much we can do about that, and it shouldn't be a major issue.
        if (sLastRecordedZygotePid != mZygotePid && hasUsableZygoteInfo()) {
            sLastRecordedZygotePid = mZygotePid;
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Android.ChildProcessStartTimeV2.Zygote", zygoteStartupTimeMillis);
        }

        if (mConnectionCallback != null) {
            mConnectionCallback.onConnected(this);
        }
        mConnectionCallback = null;
    }

    /** Passes the zygote parcel to the service. */
    public void consumeRelroLibInfo(IRelroLibInfo relroInfo) {
        if (mService == null) return;
        try {
            mService.consumeRelroLibInfo(relroInfo);
        } catch (RemoteException e) {
            // Ignore.
        }
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

            IParentProcess parentProcess =
                    new IParentProcess.Stub() {
                        @Override
                        public void finishSetupConnection(
                                int pid,
                                int zygotePid,
                                long zygoteStartupTimeMillis,
                                @Nullable IRelroLibInfo relroInfo) {
                            mLauncherHandler.post(
                                    () -> {
                                        onSetupConnectionResultOnLauncherThread(
                                                pid, zygotePid, zygoteStartupTimeMillis, relroInfo);
                                    });
                        }

                        @Override
                        public void reportExceptionInInit(String exception) {
                            synchronized (mProcessStateLock) {
                                mExceptionInServiceDuringInit = exception;
                            }
                            mLauncherHandler.post(createUnbindRunnable());
                        }

                        @Override
                        public void reportCleanExit() {
                            synchronized (mProcessStateLock) {
                                mCleanExit = true;
                            }
                            mLauncherHandler.post(createUnbindRunnable());
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
                mService.setupConnection(
                        mConnectionParams.mChildProcessArgs,
                        parentProcess,
                        mConnectionParams.mClientInterfaces);
            } catch (RemoteException re) {
                Log.e(TAG, "Failed to setup connection.", re);
            }
            mConnectionParams = null;
        } finally {
            TraceEvent.end("ChildProcessConnection.doConnectionSetup");
        }
    }

    private boolean bind(@ChildBindingState int initialBindingState) {
        assert isRunningOnLauncherThread();

        boolean success = mConnectionController.bind(initialBindingState);

        boolean usedFallback = getAlwaysFallback() && mFallbackServiceName != null;
        boolean canFallback = !getAlwaysFallback() && mFallbackServiceName != null;
        if (!success && !usedFallback && canFallback) {
            // Note this error condition is generally transient so `sAlwaysFallback` is
            // not set in this code path.
            fallbackService();
            success = mConnectionController.bind(initialBindingState);
            usedFallback = true;
            canFallback = false;
        }

        if (success) {
            if (initialBindingState == ChildBindingState.NOT_PERCEPTIBLE) {
                mNotPerceptibleBindingCount++;
            } else if (initialBindingState == ChildBindingState.STRONG) {
                mStrongBindingCount++;
            } else if (initialBindingState == ChildBindingState.VISIBLE) {
                mVisibleBindingCount++;
            }
        }

        if (success && !usedFallback && canFallback) {
            mLauncherHandler.postDelayed(
                    this::checkBindTimeOut, FALLBACK_TIMEOUT_IN_SECONDS * 1000);

            if (mIsSandboxedForHistograms) {
                RecordHistogram.recordEnumeratedHistogram(
                        HISTOGRAM_NAME, EventsEnum.SCHEDULE_TIMEOUT_SANDBOXED, EventsEnum.COUNT);
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        HISTOGRAM_NAME, EventsEnum.SCHEDULE_TIMEOUT_UNSANDBOXED, EventsEnum.COUNT);
            }
        }

        return success;
    }

    private void checkBindTimeOut() {
        assert isRunningOnLauncherThread();
        assert mFallbackServiceName != null;
        if (mDidOnServiceConnected || mServiceDisconnected) {
            return;
        }
        if (mConnectionController.isUnbound()) {
            return;
        }
        if (!mIndependentFallback) {
            sAlwaysFallback = true;
        }
        fallbackService();
        if (mIsSandboxedForHistograms) {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_NAME, EventsEnum.FALLBACK_ON_TIMEOUT_SANDBOXED, EventsEnum.COUNT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_NAME, EventsEnum.FALLBACK_ON_TIMEOUT_UNSANDBOXED, EventsEnum.COUNT);
        }
    }

    private void fallbackService() {
        assert mFallbackServiceName != null;
        Log.w(TAG, "Fallback to %s", mFallbackServiceName);

        Intent bindIntent = new Intent();
        bindIntent.setComponent(mFallbackServiceName);
        if (mServiceBundle != null) {
            bindIntent.putExtras(mServiceBundle);
        }
        mConnectionController.replaceService(bindIntent);
    }

    @VisibleForTesting
    protected void unbind() {
        assert isRunningOnLauncherThread();
        mService = null;
        mConnectionParams = null;
        mConnectionController.unbind();

        if (mMemoryPressureCallback != null) {
            final MemoryPressureCallback callback = mMemoryPressureCallback;
            ThreadUtils.postOnUiThread(() -> MemoryPressureListener.removeCallback(callback));
            mMemoryPressureCallback = null;
        }

        if (mSelfFreezeCallback != null) {
            final SelfFreezeCallback callback = mSelfFreezeCallback;
            MemoryPressureListener.removeSelfFreezeCallback(callback);
            mSelfFreezeCallback = null;
        }
    }

    public boolean updateGroupImportance(int group, int importanceInGroup) {
        assert isRunningOnLauncherThread();
        assert group != 0 || importanceInGroup == 0;
        if (mGroup != group || mImportanceInGroup != importanceInGroup) {
            mGroup = group;
            mImportanceInGroup = importanceInGroup;
            return isConnected()
                    && mConnectionController.updateGroupImportance(group, importanceInGroup);
        }
        return false;
    }

    public int getGroup() {
        assert isRunningOnLauncherThread();
        return mGroup;
    }

    public int getImportanceInGroup() {
        assert isRunningOnLauncherThread();
        return mImportanceInGroup;
    }

    public void addStrongBinding() {
        assert isRunningOnLauncherThread();
        if (isUnboundForStateChange()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        mStrongBindingCount++;
        if (mIsEffectiveBindingStateEnabled) {
            applyEffectiveBindingState();
        } else if (mStrongBindingCount == 1) {
            mConnectionController.setStrongBinding();
        }
    }

    public void removeStrongBinding() {
        assert isRunningOnLauncherThread();
        if (isUnboundForStateChange()) {
            return;
        }
        assert mStrongBindingCount > 0;
        mStrongBindingCount--;
        if (mIsEffectiveBindingStateEnabled) {
            applyEffectiveBindingState();
        } else if (mStrongBindingCount == 0) {
            mConnectionController.unsetStrongBinding();
        }
    }

    public int getStrongBindingCount() {
        assert isRunningOnLauncherThread();
        return mStrongBindingCount;
    }

    public int getVisibleBindingCount() {
        assert isRunningOnLauncherThread();
        return mVisibleBindingCount;
    }

    public void addVisibleBinding() {
        assert isRunningOnLauncherThread();
        if (isUnboundForStateChange()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        mVisibleBindingCount++;
        if (mIsEffectiveBindingStateEnabled) {
            applyEffectiveBindingState();
        } else if (mVisibleBindingCount == 1) {
            mConnectionController.setVisibleBinding();
        }
    }

    public void removeVisibleBinding() {
        assert isRunningOnLauncherThread();
        if (isUnboundForStateChange()) {
            return;
        }
        assert mVisibleBindingCount > 0;
        mVisibleBindingCount--;
        if (mIsEffectiveBindingStateEnabled) {
            applyEffectiveBindingState();
        } else if (mVisibleBindingCount == 0) {
            mConnectionController.unsetVisibleBinding();
        }
    }

    public int getNotPerceptibleBindingCount() {
        assert isRunningOnLauncherThread();
        return mNotPerceptibleBindingCount;
    }

    public void addNotPerceptibleBinding() {
        assert isRunningOnLauncherThread();
        if (isUnboundForStateChange()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        mNotPerceptibleBindingCount++;
        if (mIsEffectiveBindingStateEnabled) {
            applyEffectiveBindingState();
        } else if (mNotPerceptibleBindingCount == 1) {
            mConnectionController.setNotPerceptibleBinding();
        }
    }

    public void removeNotPerceptibleBinding() {
        assert isRunningOnLauncherThread();
        if (isUnboundForStateChange()) {
            return;
        }
        assert mNotPerceptibleBindingCount > 0;
        mNotPerceptibleBindingCount--;
        if (mIsEffectiveBindingStateEnabled) {
            applyEffectiveBindingState();
        } else if (mNotPerceptibleBindingCount == 0) {
            mConnectionController.unsetNotPerceptibleBinding();
        }
    }

    private void applyEffectiveBindingState() {
        assert isRunningOnLauncherThread();
        assert !isUnboundForStateChange();
        @ChildBindingState int effectiveBindingState = ChildBindingState.WAIVED;
        if (mStrongBindingCount > 0) {
            effectiveBindingState = ChildBindingState.STRONG;
        } else if (mVisibleBindingCount > 0) {
            effectiveBindingState = ChildBindingState.VISIBLE;
        } else if (mNotPerceptibleBindingCount > 0) {
            effectiveBindingState = ChildBindingState.NOT_PERCEPTIBLE;
        }
        mConnectionController.setEffectiveBindingState(effectiveBindingState);
    }

    /**
     * @return the current connection binding state.
     */
    public @ChildBindingState int bindingStateCurrent() {
        // This method can be called from a thread other than the launcher thread.
        return mConnectionController.getBindingState();
    }

    /**
     * @return true if the connection is bound and only bound with the waived binding or if the
     *     connection is unbound and was only bound with the waived binding when it disconnected.
     */
    public @ChildBindingState int bindingStateCurrentOrWhenDied() {
        // This method can be called from a thread other than the launcher thread.
        return mConnectionController.getBindingStateCurrentOrWhenDied();
    }

    /**
     * @return true if the connection is intentionally killed by calling kill().
     */
    public boolean isKilledByUs() {
        // WARNING: this method can be called from a thread other than the launcher thread.
        // Note that it returns the current waived bound only state and is racy. This not really
        // preventable without changing the caller's API, short of blocking.
        synchronized (mProcessStateLock) {
            return mKilledByUs;
        }
    }

    /**
     * @return true if the process exited cleanly.
     */
    public boolean hasCleanExit() {
        synchronized (mProcessStateLock) {
            return mCleanExit;
        }
    }

    /**
     * @return the exception string if service threw an exception during init. null otherwise.
     */
    public @Nullable String getExceptionDuringInit() {
        synchronized (mProcessStateLock) {
            return mExceptionInServiceDuringInit;
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

    public void crashServiceForTesting() {
        try {
            assumeNonNull(mService).forceKill();
        } catch (RemoteException e) {
            // Expected. Ignore.
        }
    }

    public boolean didOnServiceConnectedForTesting() {
        return mDidOnServiceConnected;
    }

    @VisibleForTesting
    protected Handler getLauncherHandler() {
        return mLauncherHandler;
    }

    private boolean getAlwaysFallback() {
        return sAlwaysFallback && !mIndependentFallback;
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

    private void onSelfFreeze() {
        assert isRunningOnLauncherThread();
        // This will handle all processes with only a WAIVED binding, and
        // the last visible tab, which covers all renderers (W or WN), but
        // excludes the GPU process (WS).
        @ChildBindingState int bindingState = mConnectionController.getBindingState();
        if (bindingState != ChildBindingState.WAIVED
                && bindingState != ChildBindingState.NOT_PERCEPTIBLE) {
            return;
        }
        if (mService == null) return;
        try {
            mService.onSelfFreeze();
        } catch (RemoteException ex) {
            // Ignore
        }
    }

    /**
     * Returns whether we can update binding state.
     *
     * <p>Historically, we used `isConnected()`. But mConnectionController.isUnbound() is more
     * accurate.
     *
     * <p>TODO(crbug.com/447057423): Remove the flag and always use
     * mConnectionController.isUnbound() after verifying it's safe.
     */
    private boolean isUnboundForStateChange() {
        return mUseIsUnboundCheck ? mConnectionController.isUnbound() : !isConnected();
    }
}
