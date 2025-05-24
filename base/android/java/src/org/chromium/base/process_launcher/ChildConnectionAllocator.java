// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.UserManager;

import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.JavaUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Queue;

/**
 * This class is responsible for allocating and managing connections to child process services.
 * These connections are in a pool (the services are defined in the AndroidManifest.xml).
 */
@NullMarked
public abstract class ChildConnectionAllocator {
    private static final String TAG = "ChildConnAllocator";
    private static final String ZYGOTE_SUFFIX = "0";
    private static final String NON_ZYGOTE_SUFFIX = "1";

    /** Factory interface. Used by tests to specialize created connections. */
    @VisibleForTesting
    public interface ConnectionFactory {
        ChildProcessConnection createConnection(
                Context context,
                ComponentName serviceName,
                @Nullable ComponentName fallbackServiceName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                Bundle serviceBundle,
                @Nullable String instanceName,
                boolean independentFallback,
                boolean isSandboxedForHistograms);
    }

    /** Default implementation of the ConnectionFactory that creates actual connections. */
    private static class ConnectionFactoryImpl implements ConnectionFactory {
        @Override
        public ChildProcessConnection createConnection(
                Context context,
                ComponentName serviceName,
                @Nullable ComponentName fallbackServiceName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                Bundle serviceBundle,
                @Nullable String instanceName,
                boolean independentFallback,
                boolean isSandboxedForHistograms) {
            return new ChildProcessConnection(
                    context,
                    serviceName,
                    fallbackServiceName,
                    bindToCaller,
                    bindAsExternalService,
                    serviceBundle,
                    instanceName,
                    independentFallback,
                    isSandboxedForHistograms);
        }
    }

    // Delay between the call to freeConnection and the connection actually beeing freed.
    private static final long FREE_CONNECTION_DELAY_MILLIS = 1;

    // Max number of connections allocated for variable allocator.
    // Android allocates 100 UIDs for a zygote, but unbinding and killing a service is not
    // synchronous. So leave 2 to leave some time for ActivityManager to respond.
    private static final int MAX_VARIABLE_ALLOCATED = 98;

    // Runnable which will be called when allocator wants to allocate a new connection, but does
    // not have any more free slots. May be null.
    private final @Nullable Runnable mFreeSlotCallback;

    private final Queue<Runnable> mPendingAllocations = new ArrayDeque<>();

    // The handler of the thread on which all interations should happen.
    private final Handler mLauncherHandler;

    /* package */ final String mPackageName;
    /* package */ final String mServiceClassName;
    /* package */ final @Nullable String mFallbackServiceClassName;
    /* package */ final boolean mBindToCaller;
    /* package */ final boolean mBindAsExternalService;
    /* package */ final boolean mUseStrongBinding;
    // This has no functional change and is used only for logging histograms.
    /* package */ final boolean mIsSandboxedForHistograms;

    /* package */ ConnectionFactory mConnectionFactory = new ConnectionFactoryImpl();

    private static void checkServiceExists(
            Context context, String packageName, String serviceClassName) {
        if (!packageName.equals(context.getPackageName())) {
            // If the service isn't in our own package we may need to work
            // around crbug.com/1363832.
            PackageUtils.maybeWorkAroundWebViewPackageVisibility();
        }

        PackageManager packageManager = context.getPackageManager();
        // Check that the service exists.
        try {
            // PackageManager#getServiceInfo() throws an exception if the service does not exist.
            packageManager.getServiceInfo(
                    new ComponentName(packageName, serviceClassName + "0"), 0);
        } catch (PackageManager.NameNotFoundException e) {
            JavaUtils.throwUnchecked(e);
        }
    }

    /**
     * Factory method that retrieves the service name and number of service from the
     * AndroidManifest.xml.
     */
    public static ChildConnectionAllocator create(
            Context context,
            Handler launcherHandler,
            @Nullable Runnable freeSlotCallback,
            String packageName,
            String serviceClassName,
            String numChildServicesManifestKey,
            boolean bindToCaller,
            boolean bindAsExternalService,
            boolean useStrongBinding,
            boolean fallbackToNextSlot,
            boolean isSandboxedForHistograms) {
        int numServices = -1;
        PackageManager packageManager = context.getPackageManager();
        try {
            ApplicationInfo appInfo =
                    packageManager.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
            if (appInfo.metaData != null) {
                numServices = appInfo.metaData.getInt(numChildServicesManifestKey, -1);
            }
        } catch (PackageManager.NameNotFoundException e) {
            JavaUtils.throwUnchecked(e);
        }

        if (numServices < 0) {
            throw new RuntimeException();
        }

        checkServiceExists(context, packageName, serviceClassName);

        return new FixedSizeAllocatorImpl(
                launcherHandler,
                freeSlotCallback,
                packageName,
                serviceClassName,
                bindToCaller,
                bindAsExternalService,
                useStrongBinding,
                fallbackToNextSlot,
                numServices,
                isSandboxedForHistograms);
    }

    public static ChildConnectionAllocator createVariableSize(
            Context context,
            Handler launcherHandler,
            Runnable freeSlotCallback,
            String packageName,
            String serviceClassName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            boolean useStrongBinding,
            boolean isSandboxedForHistograms) {
        checkServiceExists(context, packageName, serviceClassName);

        // OnePlus devices are having trouble with app zygote in combination with dynamic
        // feature modules. See crbug.com/1064314 for details.
        BuildInfo buildInfo = BuildInfo.getInstance();
        boolean disableZygote =
                Build.VERSION.SDK_INT == 29
                        && buildInfo.androidBuildFingerprint.startsWith("OnePlus/");

        if (Build.VERSION.SDK_INT == 29 && !disableZygote) {
            UserManager userManager =
                    (UserManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.USER_SERVICE);
            if (!userManager.isSystemUser()) {
                return new Android10WorkaroundAllocatorImpl(
                        launcherHandler,
                        freeSlotCallback,
                        packageName,
                        serviceClassName,
                        bindToCaller,
                        bindAsExternalService,
                        useStrongBinding,
                        MAX_VARIABLE_ALLOCATED,
                        isSandboxedForHistograms);
            }
        }
        // On low end devices, we do not expect to have many renderers. As a consequence, the fixed
        // costs of the app zygote are not recovered. See https://crbug.com/1044579 for context and
        // experimental results.
        disableZygote = SysUtils.isLowEndDevice() || disableZygote;
        String suffix = disableZygote ? NON_ZYGOTE_SUFFIX : ZYGOTE_SUFFIX;
        String fallbackServiceClassName =
                disableZygote ? null : serviceClassName + NON_ZYGOTE_SUFFIX;
        return new VariableSizeAllocatorImpl(
                launcherHandler,
                freeSlotCallback,
                packageName,
                serviceClassName + suffix,
                fallbackServiceClassName,
                bindToCaller,
                bindAsExternalService,
                useStrongBinding,
                MAX_VARIABLE_ALLOCATED,
                isSandboxedForHistograms);
    }

    /**
     * Factory method used with some tests to create an allocator with values passed in directly
     * instead of being retrieved from the AndroidManifest.xml.
     */
    public static FixedSizeAllocatorImpl createFixedForTesting(
            @Nullable Runnable freeSlotCallback,
            String packageName,
            String serviceClassName,
            int serviceCount,
            boolean bindToCaller,
            boolean bindAsExternalService,
            boolean useStrongBinding,
            boolean fallbackToNextSlot,
            boolean isSandboxedForHistograms) {
        return new FixedSizeAllocatorImpl(
                new Handler(),
                freeSlotCallback,
                packageName,
                serviceClassName,
                bindToCaller,
                bindAsExternalService,
                useStrongBinding,
                fallbackToNextSlot,
                serviceCount,
                isSandboxedForHistograms);
    }

    public static VariableSizeAllocatorImpl createVariableSizeForTesting(
            Handler launcherHandler,
            String packageName,
            @Nullable Runnable freeSlotCallback,
            String serviceClassName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            boolean useStrongBinding,
            int maxAllocated) {
        return new VariableSizeAllocatorImpl(
                launcherHandler,
                freeSlotCallback,
                packageName,
                serviceClassName + ZYGOTE_SUFFIX,
                null,
                bindToCaller,
                bindAsExternalService,
                useStrongBinding,
                maxAllocated,
                /* isSandboxedForHistograms= */ false);
    }

    public static Android10WorkaroundAllocatorImpl createWorkaroundForTesting(
            Handler launcherHandler,
            String packageName,
            @Nullable Runnable freeSlotCallback,
            String serviceClassName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            boolean useStrongBinding,
            int maxAllocated) {
        return new Android10WorkaroundAllocatorImpl(
                launcherHandler,
                freeSlotCallback,
                packageName,
                serviceClassName,
                bindToCaller,
                bindAsExternalService,
                useStrongBinding,
                maxAllocated,
                /* isSandboxedForHistograms= */ false);
    }

    private ChildConnectionAllocator(
            Handler launcherHandler,
            @Nullable Runnable freeSlotCallback,
            String packageName,
            String serviceClassName,
            @Nullable String fallbackServiceClassName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            boolean useStrongBinding,
            boolean isSandboxedForHistograms) {
        mLauncherHandler = launcherHandler;
        assert isRunningOnLauncherThread();
        mFreeSlotCallback = freeSlotCallback;
        mPackageName = packageName;
        mServiceClassName = serviceClassName;
        mFallbackServiceClassName = fallbackServiceClassName;
        mBindToCaller = bindToCaller;
        mBindAsExternalService = bindAsExternalService;
        mUseStrongBinding = useStrongBinding;
        mIsSandboxedForHistograms = isSandboxedForHistograms;
    }

    /**
     * @return a bound connection, or null if there are no free slots.
     */
    public @Nullable ChildProcessConnection allocate(
            Context context,
            Bundle serviceBundle,
            final ChildProcessConnection.ServiceCallback serviceCallback) {
        assert isRunningOnLauncherThread();

        // Wrap the service callbacks so that:
        // - we can intercept onChildProcessDied and clean-up connections
        // - the callbacks are actually posted so that this method will return before the callbacks
        //   are called (so that the caller may set any reference to the returned connection before
        //   any callback logic potentially tries to access that connection).
        ChildProcessConnection.ServiceCallback serviceCallbackWrapper =
                new ChildProcessConnection.ServiceCallback() {
                    @Override
                    public void onChildStarted() {
                        assert isRunningOnLauncherThread();
                        if (serviceCallback != null) {
                            mLauncherHandler.post(
                                    new Runnable() {
                                        @Override
                                        public void run() {
                                            serviceCallback.onChildStarted();
                                        }
                                    });
                        }
                    }

                    @Override
                    public void onChildStartFailed(final ChildProcessConnection connection) {
                        assert isRunningOnLauncherThread();
                        if (serviceCallback != null) {
                            mLauncherHandler.post(
                                    new Runnable() {
                                        @Override
                                        public void run() {
                                            serviceCallback.onChildStartFailed(connection);
                                        }
                                    });
                        }
                        freeConnectionWithDelay(connection);
                    }

                    @Override
                    public void onChildProcessDied(final ChildProcessConnection connection) {
                        assert isRunningOnLauncherThread();
                        if (serviceCallback != null) {
                            mLauncherHandler.post(
                                    new Runnable() {
                                        @Override
                                        public void run() {
                                            serviceCallback.onChildProcessDied(connection);
                                        }
                                    });
                        }
                        freeConnectionWithDelay(connection);
                    }

                    private void freeConnectionWithDelay(final ChildProcessConnection connection) {
                        // Freeing a service should be delayed. This is so that we avoid immediately
                        // reusing the freed service (see http://crbug.com/164069): the framework
                        // might keep a service process alive when it's been unbound for a short
                        // time. If a new connection to the same service is bound at that point, the
                        // process is reused and bad things happen (mostly static variables are set
                        // when we don't expect them to).
                        mLauncherHandler.postDelayed(
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        free(connection);
                                    }
                                },
                                FREE_CONNECTION_DELAY_MILLIS);
                    }
                };

        return doAllocate(context, serviceBundle, serviceCallbackWrapper);
    }

    /** Free connection allocated by this allocator. */
    private void free(ChildProcessConnection connection) {
        assert isRunningOnLauncherThread();
        doFree(connection);

        if (mPendingAllocations.isEmpty()) return;
        mPendingAllocations.remove().run();
        if (!mPendingAllocations.isEmpty() && mFreeSlotCallback != null) {
            mFreeSlotCallback.run();
        }
    }

    public final void queueAllocation(Runnable runnable) {
        assert isRunningOnLauncherThread();
        boolean wasEmpty = mPendingAllocations.isEmpty();
        mPendingAllocations.add(runnable);
        if (wasEmpty && mFreeSlotCallback != null) mFreeSlotCallback.run();
    }

    /** May return -1 if size is not fixed. */
    public abstract int getMaxNumberOfAllocations();

    @VisibleForTesting
    public abstract boolean anyConnectionAllocated();

    /** @return the count of connections managed by the allocator */
    public abstract int allocatedConnectionsCountForTesting();

    public void setConnectionFactoryForTesting(ConnectionFactory connectionFactory) {
        var oldValue = mConnectionFactory;
        mConnectionFactory = connectionFactory;
        ResettersForTesting.register(() -> mConnectionFactory = oldValue);
    }

    private boolean isRunningOnLauncherThread() {
        return mLauncherHandler.getLooper() == Looper.myLooper();
    }

    /* package */ abstract @Nullable ChildProcessConnection doAllocate(
            Context context,
            Bundle serviceBundle,
            ChildProcessConnection.ServiceCallback serviceCallback);

    /* package */ abstract void doFree(ChildProcessConnection connection);

    /** Implementation class accessed directly by tests. */
    @VisibleForTesting
    public static class FixedSizeAllocatorImpl extends ChildConnectionAllocator {
        // Connections to services. Indices of the array correspond to the service numbers.
        private final @Nullable ChildProcessConnection[] mChildProcessConnections;

        // The list of free (not bound) service indices.
        private final ArrayList<Integer> mFreeConnectionIndices;

        private final @Nullable Map<ChildProcessConnection, Integer> mFallbackSlots;

        private FixedSizeAllocatorImpl(
                Handler launcherHandler,
                @Nullable Runnable freeSlotCallback,
                String packageName,
                String serviceClassName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                boolean useStrongBinding,
                boolean fallbackToNextSlot,
                int numChildServices,
                boolean isSandboxedForHistograms) {
            super(
                    launcherHandler,
                    freeSlotCallback,
                    packageName,
                    serviceClassName,
                    null,
                    bindToCaller,
                    bindAsExternalService,
                    useStrongBinding,
                    isSandboxedForHistograms);

            mChildProcessConnections = new ChildProcessConnection[numChildServices];

            mFreeConnectionIndices = new ArrayList<Integer>(numChildServices);
            for (int i = 0; i < numChildServices; i++) {
                mFreeConnectionIndices.add(i);
            }

            if (fallbackToNextSlot) {
                mFallbackSlots = new HashMap<>();
            } else {
                mFallbackSlots = null;
            }
        }

        @Override
        /* package */ @Nullable
        ChildProcessConnection doAllocate(
                Context context,
                Bundle serviceBundle,
                ChildProcessConnection.ServiceCallback serviceCallback) {
            if (mFreeConnectionIndices.isEmpty()) {
                Log.w(TAG, "Ran out of services to allocate.");
                return null;
            }
            if (mFallbackSlots != null && mFreeConnectionIndices.size() < 2) {
                Log.w(TAG, "Ran out of services for fallback.");
                return null;
            }
            int slot = mFreeConnectionIndices.remove(0);
            assert mChildProcessConnections[slot] == null;
            ComponentName serviceName = new ComponentName(mPackageName, mServiceClassName + slot);
            int fallbackSlot = -1;
            ComponentName fallbackServiceName = null;
            if (mFallbackSlots != null) {
                fallbackSlot = mFreeConnectionIndices.remove(0);
                fallbackServiceName =
                        new ComponentName(mPackageName, mServiceClassName + fallbackSlot);
            }

            ChildProcessConnection connection =
                    mConnectionFactory.createConnection(
                            context,
                            serviceName,
                            fallbackServiceName,
                            mBindToCaller,
                            mBindAsExternalService,
                            serviceBundle,
                            /* instanceName= */ null,
                            /* independentFallback= */ true,
                            mIsSandboxedForHistograms);
            mChildProcessConnections[slot] = connection;
            if (mFallbackSlots != null) {
                mFallbackSlots.put(connection, fallbackSlot);
            }
            Log.d(
                    TAG,
                    "Allocator allocated and bound a connection, name: %s, slot: %d fallback:%d",
                    mServiceClassName,
                    slot,
                    fallbackSlot);
            connection.start(mUseStrongBinding, serviceCallback);
            return connection;
        }

        @Override
        /* package */ void doFree(ChildProcessConnection connection) {
            // mChildProcessConnections is relatively short (40 items at max at this point).
            // We are better of iterating than caching in a map.
            int slot = Arrays.asList(mChildProcessConnections).indexOf(connection);
            if (slot == -1) {
                Log.e(TAG, "Unable to find connection to free.");
                assert false;
            } else {
                mChildProcessConnections[slot] = null;
                assert !mFreeConnectionIndices.contains(slot);
                mFreeConnectionIndices.add(slot);

                int fallbackSlot = -1;
                if (mFallbackSlots != null) {
                    fallbackSlot = assumeNonNull(mFallbackSlots.remove(connection));
                    assert !mFreeConnectionIndices.contains(fallbackSlot);
                    mFreeConnectionIndices.add(fallbackSlot);
                }
                Log.d(
                        TAG,
                        "Allocator freed a connection, name: %s, slot: %d fallback:%d",
                        mServiceClassName,
                        slot,
                        fallbackSlot);
            }
        }

        @VisibleForTesting
        public boolean isFreeConnectionAvailable() {
            return !mFreeConnectionIndices.isEmpty();
        }

        @Override
        public int getMaxNumberOfAllocations() {
            if (mFallbackSlots != null) {
                return mChildProcessConnections.length / 2;
            } else {
                return mChildProcessConnections.length;
            }
        }

        @Override
        public int allocatedConnectionsCountForTesting() {
            if (mFallbackSlots != null) {
                return (mChildProcessConnections.length - mFreeConnectionIndices.size()) / 2;
            } else {
                return mChildProcessConnections.length - mFreeConnectionIndices.size();
            }
        }

        public @Nullable ChildProcessConnection getChildProcessConnectionAtSlotForTesting(
                int slotNumber) {
            return mChildProcessConnections[slotNumber];
        }

        @Override
        public boolean anyConnectionAllocated() {
            return mFreeConnectionIndices.size() < mChildProcessConnections.length;
        }
    }

    @VisibleForTesting
    /* package */ static class VariableSizeAllocatorImpl extends ChildConnectionAllocator {
        private final int mMaxAllocated;
        private final ArraySet<ChildProcessConnection> mAllocatedConnections = new ArraySet<>();
        private int mNextInstance;

        // Note |serviceClassName| includes the service suffix.
        private VariableSizeAllocatorImpl(
                Handler launcherHandler,
                @Nullable Runnable freeSlotCallback,
                String packageName,
                String serviceClassName,
                @Nullable String fallbackServiceClassName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                boolean useStrongBinding,
                int maxAllocated,
                boolean isSandboxedForHistograms) {
            super(
                    launcherHandler,
                    freeSlotCallback,
                    packageName,
                    serviceClassName,
                    fallbackServiceClassName,
                    bindToCaller,
                    bindAsExternalService,
                    useStrongBinding,
                    isSandboxedForHistograms);
            assert maxAllocated > 0;
            mMaxAllocated = maxAllocated;
        }

        @Override
        /* package */ @Nullable
        ChildProcessConnection doAllocate(
                Context context,
                Bundle serviceBundle,
                ChildProcessConnection.ServiceCallback serviceCallback) {
            ChildProcessConnection connection = allocate(context, serviceBundle);
            if (connection == null) return null;
            mAllocatedConnections.add(connection);
            connection.start(mUseStrongBinding, serviceCallback);
            return connection;
        }

        /* package */ @Nullable
        ChildProcessConnection tryAllocate(
                Context context,
                Bundle serviceBundle,
                ChildProcessConnection.ServiceCallback serviceCallback) {
            ChildProcessConnection connection = allocate(context, serviceBundle);
            if (connection == null) return null;
            boolean startResult = connection.tryStart(mUseStrongBinding, serviceCallback);
            if (!startResult) return null;
            mAllocatedConnections.add(connection);
            return connection;
        }

        private @Nullable ChildProcessConnection allocate(Context context, Bundle serviceBundle) {
            if (mAllocatedConnections.size() >= mMaxAllocated) {
                Log.w(TAG, "Ran out of UIDs to allocate.");
                return null;
            }
            ComponentName serviceName = new ComponentName(mPackageName, mServiceClassName);
            ComponentName fallbackServiceName = null;
            if (mFallbackServiceClassName != null) {
                fallbackServiceName = new ComponentName(mPackageName, mFallbackServiceClassName);
            }
            String instanceName = Integer.toString(mNextInstance);
            mNextInstance++;
            ChildProcessConnection connection =
                    mConnectionFactory.createConnection(
                            context,
                            serviceName,
                            fallbackServiceName,
                            mBindToCaller,
                            mBindAsExternalService,
                            serviceBundle,
                            instanceName,
                            /* independentFallback= */ false,
                            mIsSandboxedForHistograms);
            assert connection != null;
            return connection;
        }

        @Override
        /* package */ void doFree(ChildProcessConnection connection) {
            boolean result = mAllocatedConnections.remove(connection);
            assert result;
        }

        /* package */ boolean wasConnectionAllocated(ChildProcessConnection connection) {
            return mAllocatedConnections.contains(connection);
        }

        @Override
        public int getMaxNumberOfAllocations() {
            return -1;
        }

        @Override
        public int allocatedConnectionsCountForTesting() {
            return mAllocatedConnections.size();
        }

        @Override
        public boolean anyConnectionAllocated() {
            return mAllocatedConnections.size() > 0;
        }
    }

    /**
     * Workaround allocator for Android 10 bug.
     * Android 10 has a bug that UID used for non-primary user cannot be freed correctly,
     * eventually exhausting the pool of UIDs for isolated services. There is a global pool of
     * 1000 UIDs, and each app zygote has a smaller pool of 100; the bug appplies to both cases.
     * The leaked UID in the app zygote pool are released when the zygote is killed; leaked UIDs in
     * the global pool are released when the device is rebooted. So way to slightly delay until the
     * device needs to be rebooted is to use up the app zygote pool first before using the
     * non-zygote global pool.
     */
    private static class Android10WorkaroundAllocatorImpl extends ChildConnectionAllocator {
        private final VariableSizeAllocatorImpl mZygoteAllocator;
        private final VariableSizeAllocatorImpl mNonZygoteAllocator;

        private Android10WorkaroundAllocatorImpl(
                Handler launcherHandler,
                @Nullable Runnable freeSlotCallback,
                String packageName,
                String serviceClassName,
                boolean bindToCaller,
                boolean bindAsExternalService,
                boolean useStrongBinding,
                int maxAllocated,
                boolean isSandboxedForHistograms) {
            super(
                    launcherHandler,
                    freeSlotCallback,
                    packageName,
                    serviceClassName,
                    null,
                    bindToCaller,
                    bindAsExternalService,
                    useStrongBinding,
                    isSandboxedForHistograms);
            mZygoteAllocator =
                    new VariableSizeAllocatorImpl(
                            launcherHandler,
                            freeSlotCallback,
                            packageName,
                            serviceClassName + ZYGOTE_SUFFIX,
                            null,
                            bindToCaller,
                            bindAsExternalService,
                            useStrongBinding,
                            maxAllocated,
                            isSandboxedForHistograms);
            mNonZygoteAllocator =
                    new VariableSizeAllocatorImpl(
                            launcherHandler,
                            freeSlotCallback,
                            packageName,
                            serviceClassName + NON_ZYGOTE_SUFFIX,
                            null,
                            bindToCaller,
                            bindAsExternalService,
                            useStrongBinding,
                            maxAllocated,
                            isSandboxedForHistograms);
        }

        @Override
        /* package */ @Nullable
        ChildProcessConnection doAllocate(
                Context context,
                Bundle serviceBundle,
                ChildProcessConnection.ServiceCallback serviceCallback) {
            ChildProcessConnection connection =
                    mZygoteAllocator.tryAllocate(context, serviceBundle, serviceCallback);
            if (connection != null) return connection;
            return mNonZygoteAllocator.doAllocate(context, serviceBundle, serviceCallback);
        }

        @Override
        /* package */ void doFree(ChildProcessConnection connection) {
            if (mZygoteAllocator.wasConnectionAllocated(connection)) {
                mZygoteAllocator.doFree(connection);
            } else if (mNonZygoteAllocator.wasConnectionAllocated(connection)) {
                mNonZygoteAllocator.doFree(connection);
            } else {
                assert false;
            }
        }

        @Override
        public int getMaxNumberOfAllocations() {
            return -1;
        }

        @Override
        public int allocatedConnectionsCountForTesting() {
            return mZygoteAllocator.allocatedConnectionsCountForTesting()
                    + mNonZygoteAllocator.allocatedConnectionsCountForTesting();
        }

        @Override
        public boolean anyConnectionAllocated() {
            return mZygoteAllocator.anyConnectionAllocated()
                    || mNonZygoteAllocator.anyConnectionAllocated();
        }

        @Override
        public void setConnectionFactoryForTesting(ConnectionFactory connectionFactory) {
            super.setConnectionFactoryForTesting(connectionFactory);
            mZygoteAllocator.setConnectionFactoryForTesting(connectionFactory);
            mNonZygoteAllocator.setConnectionFactoryForTesting(connectionFactory);
        }
    }
}
