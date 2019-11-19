// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.v4.util.ArraySet;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Queue;

/**
 * This class is responsible for allocating and managing connections to child
 * process services. These connections are in a pool (the services are defined
 * in the AndroidManifest.xml).
 */
public abstract class ChildConnectionAllocator {
    private static final String TAG = "ChildConnAllocator";

    /** Factory interface. Used by tests to specialize created connections. */
    @VisibleForTesting
    public interface ConnectionFactory {
        ChildProcessConnection createConnection(Context context, ComponentName serviceName,
                boolean bindToCaller, boolean bindAsExternalService, Bundle serviceBundle,
                String instanceName);
    }

    /** Default implementation of the ConnectionFactory that creates actual connections. */
    private static class ConnectionFactoryImpl implements ConnectionFactory {
        @Override
        public ChildProcessConnection createConnection(Context context, ComponentName serviceName,
                boolean bindToCaller, boolean bindAsExternalService, Bundle serviceBundle,
                String instanceName) {
            return new ChildProcessConnection(context, serviceName, bindToCaller,
                    bindAsExternalService, serviceBundle, instanceName);
        }
    }

    // Delay between the call to freeConnection and the connection actually beeing freed.
    private static final long FREE_CONNECTION_DELAY_MILLIS = 1;

    // The handler of the thread on which all interations should happen.
    private final Handler mLauncherHandler;

    /* package */ final String mPackageName;
    /* package */ final String mServiceClassName;
    /* package */ final boolean mBindToCaller;
    /* package */ final boolean mBindAsExternalService;
    private final boolean mUseStrongBinding;

    /* package */ ConnectionFactory mConnectionFactory = new ConnectionFactoryImpl();

    private static void checkServiceExists(
            Context context, String packageName, String serviceClassName) {
        PackageManager packageManager = context.getPackageManager();
        // Check that the service exists.
        try {
            // PackageManager#getServiceInfo() throws an exception if the service does not exist.
            packageManager.getServiceInfo(
                    new ComponentName(packageName, serviceClassName + "0"), 0);
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Illegal meta data value: the child service doesn't exist");
        }
    }

    /**
     * Factory method that retrieves the service name and number of service from the
     * AndroidManifest.xml.
     */
    public static ChildConnectionAllocator create(Context context, Handler launcherHandler,
            Runnable freeSlotCallback, String packageName, String serviceClassName,
            String numChildServicesManifestKey, boolean bindToCaller, boolean bindAsExternalService,
            boolean useStrongBinding) {
        int numServices = -1;
        PackageManager packageManager = context.getPackageManager();
        try {
            ApplicationInfo appInfo =
                    packageManager.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
            if (appInfo.metaData != null) {
                numServices = appInfo.metaData.getInt(numChildServicesManifestKey, -1);
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Could not get application info.");
        }

        if (numServices < 0) {
            throw new RuntimeException("Illegal meta data value for number of child services");
        }

        checkServiceExists(context, packageName, serviceClassName);

        return new FixedSizeAllocatorImpl(launcherHandler, freeSlotCallback, packageName,
                serviceClassName, bindToCaller, bindAsExternalService, useStrongBinding,
                numServices);
    }

    public static ChildConnectionAllocator createVariableSize(Context context,
            Handler launcherHandler, String packageName, String serviceClassName,
            boolean bindToCaller, boolean bindAsExternalService, boolean useStrongBinding) {
        checkServiceExists(context, packageName, serviceClassName);
        return new VariableSizeAllocatorImpl(launcherHandler, packageName, serviceClassName,
                bindToCaller, bindAsExternalService, useStrongBinding);
    }

    /**
     * Factory method used with some tests to create an allocator with values passed in directly
     * instead of being retrieved from the AndroidManifest.xml.
     */
    @VisibleForTesting
    public static FixedSizeAllocatorImpl createFixedForTesting(Runnable freeSlotCallback,
            String packageName, String serviceClassName, int serviceCount, boolean bindToCaller,
            boolean bindAsExternalService, boolean useStrongBinding) {
        return new FixedSizeAllocatorImpl(new Handler(), freeSlotCallback, packageName,
                serviceClassName, bindToCaller, bindAsExternalService, useStrongBinding,
                serviceCount);
    }

    @VisibleForTesting
    public static VariableSizeAllocatorImpl createVariableSizeForTesting(Handler launcherHandler,
            String packageName, String serviceClassName, boolean bindToCaller,
            boolean bindAsExternalService, boolean useStrongBinding) {
        return new VariableSizeAllocatorImpl(launcherHandler, packageName, serviceClassName + "0",
                bindToCaller, bindAsExternalService, useStrongBinding);
    }

    private ChildConnectionAllocator(Handler launcherHandler, String packageName,
            String serviceClassName, boolean bindToCaller, boolean bindAsExternalService,
            boolean useStrongBinding) {
        mLauncherHandler = launcherHandler;
        assert isRunningOnLauncherThread();
        mPackageName = packageName;
        mServiceClassName = serviceClassName;
        mBindToCaller = bindToCaller;
        mBindAsExternalService = bindAsExternalService;
        mUseStrongBinding = useStrongBinding;
    }

    /** @return a bound connection, or null if there are no free slots. */
    public ChildProcessConnection allocate(Context context, Bundle serviceBundle,
            final ChildProcessConnection.ServiceCallback serviceCallback) {
        assert isRunningOnLauncherThread();

        ChildProcessConnection connection = doAllocate(context, serviceBundle);
        if (connection == null) return null;

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
                            mLauncherHandler.post(new Runnable() {
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
                            mLauncherHandler.post(new Runnable() {
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
                            mLauncherHandler.post(new Runnable() {
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
                        mLauncherHandler.postDelayed(new Runnable() {
                            @Override
                            public void run() {
                                free(connection);
                            }
                        }, FREE_CONNECTION_DELAY_MILLIS);
                    }
                };

        connection.start(mUseStrongBinding, serviceCallbackWrapper);
        return connection;
    }

    /** Free connection allocated by this allocator. */
    private void free(ChildProcessConnection connection) {
        assert isRunningOnLauncherThread();
        doFree(connection);
    }

    // Can only be called once all slots are full, ie when allocate returns null. Note
    // this should not be called in if created with createVariableSize.
    // The callback will be called when a slot becomes free, and should synchronous call
    // allocate to take the slot.
    public void queueAllocation(Runnable runnable) {
        assert isRunningOnLauncherThread();
        doQueueAllocation(runnable);
    }

    /** May return -1 if size is not fixed. */
    public abstract int getNumberOfServices();

    @VisibleForTesting
    public abstract boolean anyConnectionAllocated();

    /** @return the count of connections managed by the allocator */
    @VisibleForTesting
    public abstract int allocatedConnectionsCountForTesting();

    @VisibleForTesting
    public void setConnectionFactoryForTesting(ConnectionFactory connectionFactory) {
        mConnectionFactory = connectionFactory;
    }

    private boolean isRunningOnLauncherThread() {
        return mLauncherHandler.getLooper() == Looper.myLooper();
    }

    /* package */ abstract ChildProcessConnection doAllocate(Context context, Bundle serviceBundle);
    /* package */ abstract void doFree(ChildProcessConnection connection);
    /* package */ abstract void doQueueAllocation(Runnable runnable);

    /** Implementation class accessed directly by tests. */
    @VisibleForTesting
    public static class FixedSizeAllocatorImpl extends ChildConnectionAllocator {
        // Runnable which will be called when allocator wants to allocate a new connection, but does
        // not have any more free slots. May be null.
        private final Runnable mFreeSlotCallback;

        // Connections to services. Indices of the array correspond to the service numbers.
        private final ChildProcessConnection[] mChildProcessConnections;

        // The list of free (not bound) service indices.
        private final ArrayList<Integer> mFreeConnectionIndices;

        private final Queue<Runnable> mPendingAllocations = new ArrayDeque<>();

        private FixedSizeAllocatorImpl(Handler launcherHandler, Runnable freeSlotCallback,
                String packageName, String serviceClassName, boolean bindToCaller,
                boolean bindAsExternalService, boolean useStrongBinding, int numChildServices) {
            super(launcherHandler, packageName, serviceClassName, bindToCaller,
                    bindAsExternalService, useStrongBinding);

            mFreeSlotCallback = freeSlotCallback;
            mChildProcessConnections = new ChildProcessConnection[numChildServices];

            mFreeConnectionIndices = new ArrayList<Integer>(numChildServices);
            for (int i = 0; i < numChildServices; i++) {
                mFreeConnectionIndices.add(i);
            }
        }

        @Override
        /* package */ ChildProcessConnection doAllocate(Context context, Bundle serviceBundle) {
            if (mFreeConnectionIndices.isEmpty()) {
                Log.d(TAG, "Ran out of services to allocate.");
                return null;
            }
            int slot = mFreeConnectionIndices.remove(0);
            assert mChildProcessConnections[slot] == null;
            ComponentName serviceName = new ComponentName(mPackageName, mServiceClassName + slot);

            ChildProcessConnection connection =
                    mConnectionFactory.createConnection(context, serviceName, mBindToCaller,
                            mBindAsExternalService, serviceBundle, null /* instanceName */);
            mChildProcessConnections[slot] = connection;
            Log.d(TAG, "Allocator allocated and bound a connection, name: %s, slot: %d",
                    mServiceClassName, slot);
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
                Log.d(TAG, "Allocator freed a connection, name: %s, slot: %d", mServiceClassName,
                        slot);
            }

            if (mPendingAllocations.isEmpty()) return;
            mPendingAllocations.remove().run();
            assert mFreeConnectionIndices.isEmpty();
            if (!mPendingAllocations.isEmpty() && mFreeSlotCallback != null) {
                mFreeSlotCallback.run();
            }
        }

        @Override
        /* package */ void doQueueAllocation(Runnable runnable) {
            assert mFreeConnectionIndices.isEmpty();
            boolean wasEmpty = mPendingAllocations.isEmpty();
            mPendingAllocations.add(runnable);
            if (wasEmpty && mFreeSlotCallback != null) mFreeSlotCallback.run();
        }

        @Override
        public int getNumberOfServices() {
            return mChildProcessConnections.length;
        }

        @Override
        public int allocatedConnectionsCountForTesting() {
            return mChildProcessConnections.length - mFreeConnectionIndices.size();
        }

        @VisibleForTesting
        public ChildProcessConnection getChildProcessConnectionAtSlotForTesting(int slotNumber) {
            return mChildProcessConnections[slotNumber];
        }

        @VisibleForTesting
        public boolean isFreeConnectionAvailable() {
            return !mFreeConnectionIndices.isEmpty();
        }

        @Override
        public boolean anyConnectionAllocated() {
            return mFreeConnectionIndices.size() < mChildProcessConnections.length;
        }
    }

    @VisibleForTesting
    /* package */ static class VariableSizeAllocatorImpl extends ChildConnectionAllocator {
        private final ArraySet<ChildProcessConnection> mAllocatedConnections = new ArraySet<>();
        private int mNextInstance;

        private VariableSizeAllocatorImpl(Handler launcherHandler, String packageName,
                String serviceClassName, boolean bindToCaller, boolean bindAsExternalService,
                boolean useStrongBinding) {
            super(launcherHandler, packageName, serviceClassName + "0", bindToCaller,
                    bindAsExternalService, useStrongBinding);
        }

        @Override
        /* package */ ChildProcessConnection doAllocate(Context context, Bundle serviceBundle) {
            ComponentName serviceName = new ComponentName(mPackageName, mServiceClassName);
            String instanceName = Integer.toString(mNextInstance);
            mNextInstance++;
            ChildProcessConnection connection =
                    mConnectionFactory.createConnection(context, serviceName, mBindToCaller,
                            mBindAsExternalService, serviceBundle, instanceName);
            assert connection != null;
            mAllocatedConnections.add(connection);
            return connection;
        }

        @Override
        /* package */ void doFree(ChildProcessConnection connection) {
            mAllocatedConnections.remove(connection);
        }

        @Override
        /* package */ void doQueueAllocation(Runnable runnable) {
            assert false;
        }

        @Override
        public int getNumberOfServices() {
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
}
