// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Build;
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
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;

import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.Executor;

import javax.annotation.concurrent.GuardedBy;

/** Manages a connection between the browser activity and a child service. */
public class ChildProcessConnection {
    private static final String TAG = "ChildProcessConn";
    private static final int FALLBACK_TIMEOUT_IN_SECONDS = 10;
    private static final boolean SUPPORT_NOT_PERCEPTIBLE_BINDING =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;

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

    /** Used to notify the consumer about the connection being established. */
    public interface ConnectionCallback {
        /**
         * Called when the connection to the service is established.
         * @param connection the connection object to the child process
         */
        void onConnected(ChildProcessConnection connection);
    }

    /**
     * Used to notify the client about the new shared relocations (RELRO) arriving from the app
     * zygote.
     */
    public interface ZygoteInfoCallback {
        /**
         * Called after the connection has been established, only once per process being connected
         * to.
         * @param connection the connection object to the child process. Must hold the zygote PID
         *                   that the child process was spawned from
         * @param relroBundle the bundle potentially containing the information making it possible
         *                    to replace the current RELRO address range to memory shared with the
         *                    zpp zygote. Needs to be passed to the library loader in order to take
         *                    effect. Can be done before or after the LibraryLoader loads the
         *                    library.
         */
        void onReceivedZygoteInfo(ChildProcessConnection connection, Bundle relroBundle);
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

    /** Run time check if not perceptible binding is supported. */
    public static boolean supportNotPerceptibleBinding() {
        return SUPPORT_NOT_PERCEPTIBLE_BINDING;
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
    private final Object mBindingStateLock = new Object();

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
        final IBinder mBinderBox;

        ConnectionParams(
                Bundle connectionBundle, List<IBinder> clientInterfaces, IBinder binderBox) {
            mConnectionBundle = connectionBundle;
            mClientInterfaces = clientInterfaces;
            mBinderBox = binderBox;
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

    // Callback provided in setupConnection().
    private ZygoteInfoCallback mZygoteInfoCallback;

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

    // Visible binding will make the service priority equal to the priority of a visible process
    // while the app is in the foreground.
    // This is also used as the initial binding before any priorities are set.
    private ChildServiceConnection mVisibleBinding;

    // On Android Q+ a not perceptible binding will make the service priority below that of a
    // perceptible process of a backgrounded app. Only created on Android Q+.
    private ChildServiceConnection mNotPerceptibleBinding;

    // Low priority binding maintained in the entire lifetime of the connection, i.e. between calls
    // to start() and stop().
    private ChildServiceConnection mWaivedBinding;

    // Refcount of bindings.
    private int mStrongBindingCount;
    private int mVisibleBindingCount;
    private int mNotPerceptibleBindingCount;

    private int mGroup;
    private int mImportanceInGroup;

    // Set to true once unbind() was called.
    private boolean mUnbound;

    // Binding state of this connection.
    @GuardedBy("mBindingStateLock")
    private @ChildBindingState int mBindingState;

    // Same as above except it no longer updates after |unbind()|.
    @GuardedBy("mBindingStateLock")
    private @ChildBindingState int mBindingStateCurrentOrWhenDied;

    // Indicate |kill()| was called to intentionally kill this process.
    @GuardedBy("mBindingStateLock")
    private boolean mKilledByUs;

    private MemoryPressureCallback mMemoryPressureCallback;

    // If the process threw an exception before entering the main loop, the exception
    // string is reported here.
    @GuardedBy("mBindingStateLock")
    private String mExceptionInServiceDuringInit;

    // Whether the process exited cleanly or not.
    @GuardedBy("mBindingStateLock")
    private boolean mCleanExit;

    public ChildProcessConnection(
            Context context,
            ComponentName serviceName,
            ComponentName fallbackServiceName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            Bundle serviceBundle,
            String instanceName) {
        this(
                context,
                serviceName,
                fallbackServiceName,
                bindToCaller,
                bindAsExternalService,
                serviceBundle,
                /* connectionFactory= */ null,
                instanceName);
    }

    @VisibleForTesting
    public ChildProcessConnection(
            final Context context,
            ComponentName serviceName,
            ComponentName fallbackServiceName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            Bundle serviceBundle,
            ChildServiceConnectionFactory connectionFactory,
            String instanceName) {
        mLauncherHandler = new Handler();
        mLauncherExecutor =
                (Runnable runnable) -> {
                    mLauncherHandler.post(runnable);
                };
        assert isRunningOnLauncherThread();
        mServiceName = serviceName;
        mFallbackServiceName = fallbackServiceName;
        mServiceBundle = serviceBundle != null ? serviceBundle : new Bundle();
        mServiceBundle.putBoolean(ChildProcessConstants.EXTRA_BIND_TO_CALLER, bindToCaller);
        mServiceBundle.putString(
                ChildProcessConstants.EXTRA_BROWSER_PACKAGE_NAME,
                BuildInfo.getInstance().packageName);
        mBindToCaller = bindToCaller;
        mInstanceName = instanceName;
        // Incremental install does not work with isolatedProcess, and externalService requires
        // isolatedProcess, so both need to be turned off for incremental install.
        mBindAsExternalService = bindAsExternalService && !BuildConfig.IS_INCREMENTAL_INSTALL;
        if (connectionFactory == null) {
            mConnectionFactory =
                    new ChildServiceConnectionFactory() {
                        @Override
                        public ChildServiceConnection createConnection(
                                Intent bindIntent,
                                int bindFlags,
                                ChildServiceConnectionDelegate delegate,
                                String instanceName) {
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
        } else {
            mConnectionFactory = connectionFactory;
        }

        // Methods on the delegate are can be called on launcher thread or UI thread, so need to
        // handle both cases. See BindService for details.
        mConnectionDelegate =
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

        createBindings(
                sAlwaysFallback && mFallbackServiceName != null
                        ? mFallbackServiceName
                        : mServiceName);
    }

    private void createBindings(ComponentName serviceName) {
        Intent intent = new Intent();
        intent.setComponent(serviceName);
        if (mServiceBundle != null) {
            intent.putExtras(mServiceBundle);
        }

        int defaultFlags =
                Context.BIND_AUTO_CREATE
                        | (mBindAsExternalService ? Context.BIND_EXTERNAL_SERVICE : 0);

        mVisibleBinding =
                mConnectionFactory.createConnection(
                        intent, defaultFlags, mConnectionDelegate, mInstanceName);
        if (supportNotPerceptibleBinding()) {
            mNotPerceptibleBinding =
                    mConnectionFactory.createConnection(
                            intent,
                            defaultFlags | Context.BIND_NOT_PERCEPTIBLE,
                            mConnectionDelegate,
                            mInstanceName);
        }

        mStrongBinding =
                mConnectionFactory.createConnection(
                        intent,
                        defaultFlags | Context.BIND_IMPORTANT,
                        mConnectionDelegate,
                        mInstanceName);
        mWaivedBinding =
                mConnectionFactory.createConnection(
                        intent,
                        defaultFlags | Context.BIND_WAIVE_PRIORITY,
                        mConnectionDelegate,
                        mInstanceName);
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
     * @param useStrongBinding whether a strong binding should be bound by default. If false, an
     * initial moderate binding is used.
     * @param serviceCallback (optional) callbacks invoked when the child process starts or fails to
     * start and when the service stops.
     */
    public void start(boolean useStrongBinding, ServiceCallback serviceCallback) {
        try {
            TraceEvent.begin("ChildProcessConnection.start");
            assert isRunningOnLauncherThread();
            assert mConnectionParams == null
                    : "setupConnection() called before start() in ChildProcessConnection.";

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
            assert mConnectionParams == null
                    : "setupConnection() called before start() in ChildProcessConnection.";

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
     *
     * @param connectionBundle a bundle passed to the service that can be used to pass various
     *     parameters to the service
     * @param clientInterfaces optional client specified interfaces that the child can use to
     *     communicate with the parent process
     * @param binderBox optional binder box the child can use to unpack additional binders
     * @param connectionCallback will be called exactly once after the connection is set up or the
     *     setup fails
     * @param zygoteInfoCallback will be called exactly once after the connection is set up
     */
    public void setupConnection(
            Bundle connectionBundle,
            @Nullable List<IBinder> clientInterfaces,
            @Nullable IBinder binderBox,
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
            mConnectionParams = new ConnectionParams(connectionBundle, clientInterfaces, binderBox);
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
        synchronized (mBindingStateLock) {
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
            boolean childMatches;
            try {
                ApplicationInfo child = mService.getAppInfo();
                ApplicationInfo parent = BuildInfo.getInstance().getBrowserApplicationInfo();
                // Don't compare splitSourceDirs as isolatedSplits/dynamic feature modules/etc make
                // this potentially complicated.
                childMatches =
                        Objects.equals(parent.sourceDir, child.sourceDir)
                                && Arrays.equals(
                                        parent.sharedLibraryFiles, child.sharedLibraryFiles);
            } catch (RemoteException ex) {
                // If the child can't handle getAppInfo then it is old and doesn't match.
                childMatches = false;
            }
            if (!childMatches) {
                // Check if it looks like the browser's package version has been changed since the
                // browser process launched (i.e. if the install somehow did not kill our process)
                PackageInfo latestPackage = PackageUtils.getApplicationPackageInfo(0);
                long latestVersionCode = BuildInfo.packageVersionCode(latestPackage);
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

    private String buildDebugStateString() {
        StringBuilder s = new StringBuilder();
        s.append("bindings:");
        s.append(mWaivedBinding.isBound() ? "W" : " ");
        s.append(mVisibleBinding.isBound() ? "V" : " ");
        s.append(supportNotPerceptibleBinding() && mNotPerceptibleBinding.isBound() ? "N" : " ");
        s.append(mStrongBinding.isBound() ? "S" : " ");
        return s.toString();
    }

    private void onSetupConnectionResultOnLauncherThread(
            int pid, int zygotePid, long zygoteStartupTimeMillis, Bundle relroBundle) {
        assert isRunningOnLauncherThread();

        // The RELRO bundle should be accepted only when establishing the connection. This is to
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
            mZygoteInfoCallback.onReceivedZygoteInfo(this, relroBundle);
        }
        mZygoteInfoCallback = null;

        // Only record the zygote startup time for a process the first time it is sent.  The zygote
        // may get killed and recreated, so keep track of the last PID recorded to avoid double
        // counting. The app may reuse a zygote process if the app is stopped and started again
        // quickly, so the startup time of that zygote may be recorded multiple times. There's not
        // much we can do about that, and it shouldn't be a major issue.
        if (sLastRecordedZygotePid != mZygotePid && hasUsableZygoteInfo()) {
            sLastRecordedZygotePid = mZygotePid;
            RecordHistogram.recordMediumTimesHistogram(
                    "Android.ChildProcessStartTimeV2.Zygote", zygoteStartupTimeMillis);
        }

        if (mConnectionCallback != null) {
            mConnectionCallback.onConnected(this);
        }
        mConnectionCallback = null;
    }

    /** Passes the zygote bundle to the service. */
    public void consumeZygoteBundle(Bundle zygoteBundle) {
        if (mService == null) return;
        try {
            mService.consumeRelroBundle(zygoteBundle);
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
                                Bundle relroBundle) {
                            mLauncherHandler.post(
                                    () -> {
                                        onSetupConnectionResultOnLauncherThread(
                                                pid,
                                                zygotePid,
                                                zygoteStartupTimeMillis,
                                                relroBundle);
                                    });
                        }

                        @Override
                        public void reportExceptionInInit(String exception) {
                            synchronized (mBindingStateLock) {
                                mExceptionInServiceDuringInit = exception;
                            }
                            mLauncherHandler.post(createUnbindRunnable());
                        }

                        @Override
                        public void reportCleanExit() {
                            synchronized (mBindingStateLock) {
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
                        mConnectionParams.mConnectionBundle,
                        parentProcess,
                        mConnectionParams.mClientInterfaces,
                        mConnectionParams.mBinderBox);
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

        boolean success = bindUsingExistingBindings(useStrongBinding);
        boolean usedFallback = sAlwaysFallback && mFallbackServiceName != null;
        boolean canFallback = !sAlwaysFallback && mFallbackServiceName != null;
        if (!success && !usedFallback && canFallback) {
            // Note this error condition is generally transient so `sAlwaysFallback` is
            // not set in this code path.
            retireAndCreateFallbackBindings();
            success = bindUsingExistingBindings(useStrongBinding);
            usedFallback = true;
            canFallback = false;
        }

        if (success && !usedFallback && canFallback) {
            mLauncherHandler.postDelayed(
                    this::checkBindTimeOut, FALLBACK_TIMEOUT_IN_SECONDS * 1000);
        }

        return success;
    }

    private boolean bindUsingExistingBindings(boolean useStrongBinding) {
        assert isRunningOnLauncherThread();

        boolean success;
        if (useStrongBinding) {
            success = mStrongBinding.bindServiceConnection();
            if (success) {
                mStrongBindingCount++;
            }
        } else {
            success = mVisibleBinding.bindServiceConnection();
            if (success) {
                mVisibleBindingCount++;
            }
        }

        if (success) {
            boolean result = mWaivedBinding.bindServiceConnection();
            // One binding already succeeded. Waived binding should succeed too.
            assert result;
            updateBindingState();
        }
        return success;
    }

    private void checkBindTimeOut() {
        assert isRunningOnLauncherThread();
        assert mFallbackServiceName != null;
        if (mDidOnServiceConnected || mServiceDisconnected) {
            return;
        }
        if (mUnbound) {
            return;
        }
        sAlwaysFallback = true;
        retireBindingsAndBindFallback();
    }

    private boolean retireBindingsAndBindFallback() {
        assert mFallbackServiceName != null;
        boolean isStrongBindingBound = mStrongBinding.isBound();
        boolean isVisibleBindingBound = mVisibleBinding.isBound();
        boolean isNotPerceptibleBindingBound =
                supportNotPerceptibleBinding() && mNotPerceptibleBinding.isBound();
        boolean isWaivedBindingBound = mWaivedBinding.isBound();
        retireAndCreateFallbackBindings();
        // Expect all bindings to succeed or fail together. So early out as soon as
        // one binding fails.
        if (isStrongBindingBound) {
            if (!mStrongBinding.bindServiceConnection()) {
                return false;
            }
        }
        if (isVisibleBindingBound) {
            if (!mVisibleBinding.bindServiceConnection()) {
                return false;
            }
        }
        if (isNotPerceptibleBindingBound) {
            if (!mNotPerceptibleBinding.bindServiceConnection()) {
                return false;
            }
        }
        if (isWaivedBindingBound) {
            if (!mWaivedBinding.bindServiceConnection()) {
                return false;
            }
        }
        return true;
    }

    private void retireAndCreateFallbackBindings() {
        assert mFallbackServiceName != null;
        Log.w(TAG, "Fallback to %s", mFallbackServiceName);
        mStrongBinding.retire();
        mVisibleBinding.retire();
        if (supportNotPerceptibleBinding()) {
            mNotPerceptibleBinding.retire();
        }
        mWaivedBinding.retire();
        createBindings(mFallbackServiceName);
    }

    @VisibleForTesting
    protected void unbind() {
        assert isRunningOnLauncherThread();
        mService = null;
        mConnectionParams = null;
        mUnbound = true;
        mStrongBinding.unbindServiceConnection();
        mWaivedBinding.unbindServiceConnection();
        if (supportNotPerceptibleBinding()) {
            mNotPerceptibleBinding.unbindServiceConnection();
        }
        mVisibleBinding.unbindServiceConnection();
        updateBindingState();

        if (mMemoryPressureCallback != null) {
            final MemoryPressureCallback callback = mMemoryPressureCallback;
            ThreadUtils.postOnUiThread(() -> MemoryPressureListener.removeCallback(callback));
            mMemoryPressureCallback = null;
        }
    }

    public void updateGroupImportance(int group, int importanceInGroup) {
        assert isRunningOnLauncherThread();
        assert group != 0 || importanceInGroup == 0;
        if (mGroup != group || mImportanceInGroup != importanceInGroup) {
            mGroup = group;
            mImportanceInGroup = importanceInGroup;
            if (isConnected()) {
                mWaivedBinding.updateGroupImportance(group, importanceInGroup);
            }
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
            mStrongBinding.unbindServiceConnection();
            updateBindingState();
        }
    }

    public boolean isVisibleBindingBound() {
        assert isRunningOnLauncherThread();
        return mVisibleBinding.isBound();
    }

    public int getVisibleBindingCount() {
        assert isRunningOnLauncherThread();
        return mVisibleBindingCount;
    }

    public void addVisibleBinding() {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        if (mVisibleBindingCount == 0) {
            mVisibleBinding.bindServiceConnection();
            updateBindingState();
        }
        mVisibleBindingCount++;
    }

    public void removeVisibleBinding() {
        assert isRunningOnLauncherThread();
        if (!isConnected()) {
            return;
        }
        assert mVisibleBindingCount > 0;
        mVisibleBindingCount--;
        if (mVisibleBindingCount == 0) {
            mVisibleBinding.unbindServiceConnection();
            updateBindingState();
        }
    }

    public boolean isNotPerceptibleBindingBound() {
        assert isRunningOnLauncherThread();
        return supportNotPerceptibleBinding() && mNotPerceptibleBinding.isBound();
    }

    public int getNotPerceptibleBindingCount() {
        assert isRunningOnLauncherThread();
        return mNotPerceptibleBindingCount;
    }

    public void addNotPerceptibleBinding() {
        assert isRunningOnLauncherThread();
        assert supportNotPerceptibleBinding();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        if (mNotPerceptibleBindingCount == 0) {
            mNotPerceptibleBinding.bindServiceConnection();
            updateBindingState();
        }
        mNotPerceptibleBindingCount++;
    }

    public void removeNotPerceptibleBinding() {
        assert isRunningOnLauncherThread();
        assert supportNotPerceptibleBinding();
        if (!isConnected()) {
            return;
        }
        assert mNotPerceptibleBindingCount > 0;
        mNotPerceptibleBindingCount--;
        if (mNotPerceptibleBindingCount == 0) {
            mNotPerceptibleBinding.unbindServiceConnection();
            updateBindingState();
        }
    }

    /**
     * @return the current connection binding state.
     */
    public @ChildBindingState int bindingStateCurrent() {
        // WARNING: this method can be called from a thread other than the launcher thread.
        // Note that it returns the current waived bound only state and is racy. This not really
        // preventable without changing the caller's API, short of blocking.
        synchronized (mBindingStateLock) {
            return mBindingState;
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
        synchronized (mBindingStateLock) {
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
        synchronized (mBindingStateLock) {
            return mKilledByUs;
        }
    }

    /**
     * @return true if the process exited cleanly.
     */
    public boolean hasCleanExit() {
        synchronized (mBindingStateLock) {
            return mCleanExit;
        }
    }

    /**
     * @return the exception string if service threw an exception during init.
     *         null otherwise.
     */
    public @Nullable String getExceptionDuringInit() {
        synchronized (mBindingStateLock) {
            return mExceptionInServiceDuringInit;
        }
    }

    // Should be called any binding is bound or unbound.
    private void updateBindingState() {
        int newBindingState;
        if (mUnbound) {
            newBindingState = ChildBindingState.UNBOUND;
        } else if (mStrongBinding.isBound()) {
            newBindingState = ChildBindingState.STRONG;
        } else if (mVisibleBinding.isBound()) {
            newBindingState = ChildBindingState.VISIBLE;
        } else if (supportNotPerceptibleBinding() && mNotPerceptibleBinding.isBound()) {
            newBindingState = ChildBindingState.NOT_PERCEPTIBLE;
        } else {
            assert mWaivedBinding.isBound();
            newBindingState = ChildBindingState.WAIVED;
        }

        synchronized (mBindingStateLock) {
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

    public void crashServiceForTesting() {
        try {
            mService.forceKill();
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
