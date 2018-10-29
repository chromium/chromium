// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcelable;
import android.os.Process;
import android.os.RemoteException;
import android.support.annotation.IntDef;
import android.util.SparseArray;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureLevel;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.base.metrics.RecordHistogram;

import java.util.List;
import java.util.concurrent.Semaphore;

import javax.annotation.concurrent.GuardedBy;

/**
 * This is the base class for child services; the embedding application should contain
 * ProcessService0, 1.. etc subclasses that provide the concrete service entry points, so it can
 * connect to more than one distinct process (i.e. one process per service number, up to limit of
 * N).
 * The embedding application must declare these service instances in the application section
 * of its AndroidManifest.xml, first with some meta-data describing the services:
 *     <meta-data android:name="org.chromium.test_app.SERVICES_NAME"
 *           android:value="org.chromium.test_app.ProcessService"/>
 * and then N entries of the form:
 *     <service android:name="org.chromium.test_app.ProcessServiceX"
 *              android:process=":processX" />
 *
 * Subclasses must also provide a delegate in this class constructor. That delegate is responsible
 * for loading native libraries and running the main entry point of the service.
 */
@JNINamespace("base::android")
@MainDex
public abstract class ChildProcessService extends Service {
    private static final String MAIN_THREAD_NAME = "ChildProcessMain";
    private static final String TAG = "ChildProcessService";

    // Only for a check that create is only called once.
    private static boolean sCreateCalled;

    private final ChildProcessServiceDelegate mDelegate;

    private final Object mBinderLock = new Object();
    private final Object mLibraryInitializedLock = new Object();

    // True if we should enforce that bindToCaller() is called before setupConnection().
    // Only set once in bind(), does not require synchronization.
    private boolean mBindToCallerCheck;

    // PID of the client of this service, set in bindToCaller(), if mBindToCallerCheck is true.
    @GuardedBy("mBinderLock")
    private int mBoundCallingPid;

    // This is the native "Main" thread for the renderer / utility process.
    private Thread mMainThread;

    // Parameters received via IPC, only accessed while holding the mMainThread monitor.
    private String[] mCommandLineParams;

    // File descriptors that should be registered natively.
    private FileDescriptorInfo[] mFdInfos;

    @GuardedBy("mLibraryInitializedLock")
    private boolean mLibraryInitialized;

    // Called once the service is bound and all service related member variables have been set.
    // Only set once in bind(), does not require synchronization.
    private boolean mServiceBound;

    private final Semaphore mActivitySemaphore = new Semaphore(1);

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({SplitApkWorkaroundResult.NOT_RUN, SplitApkWorkaroundResult.NO_ENTRIES,
            SplitApkWorkaroundResult.ONE_ENTRY, SplitApkWorkaroundResult.MULTIPLE_ENTRIES,
            SplitApkWorkaroundResult.TOPLEVEL_EXCEPTION, SplitApkWorkaroundResult.LOOP_EXCEPTION})
    public @interface SplitApkWorkaroundResult {
        int NOT_RUN = 0;
        int NO_ENTRIES = 1;
        int ONE_ENTRY = 2;
        int MULTIPLE_ENTRIES = 3;
        int TOPLEVEL_EXCEPTION = 4;
        int LOOP_EXCEPTION = 5;
        // Keep this one at the end and increment appropriately when adding new results.
        int SPLIT_APK_WORKAROUND_RESULT_COUNT = 6;
    }

    private static @SplitApkWorkaroundResult int sSplitApkWorkaroundResult =
            SplitApkWorkaroundResult.NOT_RUN;

    public static void setSplitApkWorkaroundResult(@SplitApkWorkaroundResult int result) {
        sSplitApkWorkaroundResult = result;
    }

    public ChildProcessService(ChildProcessServiceDelegate delegate) {
        mDelegate = delegate;
    }

    // Binder object used by clients for this service.
    private final IChildProcessService.Stub mBinder = new IChildProcessService.Stub() {
        // NOTE: Implement any IChildProcessService methods here.
        @Override
        public boolean bindToCaller() {
            assert mBindToCallerCheck;
            assert mServiceBound;
            synchronized (mBinderLock) {
                int callingPid = Binder.getCallingPid();
                if (mBoundCallingPid == 0) {
                    mBoundCallingPid = callingPid;
                } else if (mBoundCallingPid != callingPid) {
                    Log.e(TAG, "Service is already bound by pid %d, cannot bind for pid %d",
                            mBoundCallingPid, callingPid);
                    return false;
                }
            }
            return true;
        }

        @Override
        public void setupConnection(Bundle args, ICallbackInt pidCallback, List<IBinder> callbacks)
                throws RemoteException {
            assert mServiceBound;
            synchronized (mBinderLock) {
                if (mBindToCallerCheck && mBoundCallingPid == 0) {
                    Log.e(TAG, "Service has not been bound with bindToCaller()");
                    pidCallback.call(-1);
                    return;
                }
            }

            pidCallback.call(Process.myPid());
            processConnectionBundle(args, callbacks);
        }

        @Override
        public void forceKill() {
            assert mServiceBound;
            Process.killProcess(Process.myPid());
        }

        @Override
        public void onMemoryPressure(@MemoryPressureLevel int pressure) {
            // This method is called by the host process when the host process reports pressure
            // to its native side. The key difference between the host process and its services is
            // that the host process polls memory pressure when it gets CRITICAL, and periodically
            // invokes pressure listeners until pressure subsides. (See MemoryPressureMonitor for
            // more info.)
            //
            // Services don't poll, so this side-channel is used to notify services about memory
            // pressure from the host process's POV.
            //
            // However, since both host process and services listen to ComponentCallbacks2, we
            // can't be sure that the host process won't get better signals than their services.
            // I.e. we need to watch out for a situation where a service gets CRITICAL, but the
            // host process gets MODERATE - in this case we need to ignore MODERATE.
            //
            // So we're ignoring pressure from the host process if it's better than the last
            // reported pressure. I.e. the host process can drive pressure up, but it'll go
            // down only when we the service get a signal through ComponentCallbacks2.
            ThreadUtils.postOnUiThread(() -> {
                if (pressure >= MemoryPressureMonitor.INSTANCE.getLastReportedPressure()) {
                    MemoryPressureMonitor.INSTANCE.notifyPressure(pressure);
                }
            });
        }
    };

    /**
     * Loads Chrome's native libraries and initializes a ChildProcessService.
     */
    // For sCreateCalled check.
    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "Creating new ChildProcessService pid=%d", Process.myPid());
        if (sCreateCalled) {
            throw new RuntimeException("Illegal child process reuse.");
        }
        sCreateCalled = true;

        // Initialize the context for the application that owns this ChildProcessService object.
        ContextUtils.initApplicationContext(getApplicationContext());

        mDelegate.onServiceCreated();

        mMainThread = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    // CommandLine must be initialized before everything else.
                    synchronized (mMainThread) {
                        while (mCommandLineParams == null) {
                            mMainThread.wait();
                        }
                    }
                    assert mServiceBound;
                    CommandLine.init(mCommandLineParams);

                    if (CommandLine.getInstance().hasSwitch(
                                BaseSwitches.RENDERER_WAIT_FOR_JAVA_DEBUGGER)) {
                        android.os.Debug.waitForDebugger();
                    }

                    boolean nativeLibraryLoaded = false;
                    try {
                        nativeLibraryLoaded = mDelegate.loadNativeLibrary(getApplicationContext());
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to load native library.", e);
                    }
                    if (!nativeLibraryLoaded) {
                        System.exit(-1);
                    }

                    synchronized (mLibraryInitializedLock) {
                        mLibraryInitialized = true;
                        mLibraryInitializedLock.notifyAll();
                    }
                    synchronized (mMainThread) {
                        mMainThread.notifyAll();
                        while (mFdInfos == null) {
                            mMainThread.wait();
                        }
                    }

                    SparseArray<String> idsToKeys = mDelegate.getFileDescriptorsIdsToKeys();

                    int[] fileIds = new int[mFdInfos.length];
                    String[] keys = new String[mFdInfos.length];
                    int[] fds = new int[mFdInfos.length];
                    long[] regionOffsets = new long[mFdInfos.length];
                    long[] regionSizes = new long[mFdInfos.length];
                    for (int i = 0; i < mFdInfos.length; i++) {
                        FileDescriptorInfo fdInfo = mFdInfos[i];
                        String key = idsToKeys != null ? idsToKeys.get(fdInfo.id) : null;
                        if (key != null) {
                            keys[i] = key;
                        } else {
                            fileIds[i] = fdInfo.id;
                        }
                        fds[i] = fdInfo.fd.detachFd();
                        regionOffsets[i] = fdInfo.offset;
                        regionSizes[i] = fdInfo.size;
                    }
                    nativeRegisterFileDescriptors(keys, fileIds, fds, regionOffsets, regionSizes);

                    mDelegate.onBeforeMain();
                    if (ContextUtils.isIsolatedProcess()) {
                        RecordHistogram.recordEnumeratedHistogram(
                                "Android.WebView.SplitApkWorkaroundResult",
                                sSplitApkWorkaroundResult,
                                SplitApkWorkaroundResult.SPLIT_APK_WORKAROUND_RESULT_COUNT);
                    }
                    if (mActivitySemaphore.tryAcquire()) {
                        mDelegate.runMain();
                        nativeExitChildProcess();
                    }
                } catch (InterruptedException e) {
                    Log.w(TAG, "%s startup failed: %s", MAIN_THREAD_NAME, e);
                }
            }
        }, MAIN_THREAD_NAME);
        mMainThread.start();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "Destroying ChildProcessService pid=%d", Process.myPid());
        if (mActivitySemaphore.tryAcquire()) {
            // TODO(crbug.com/457406): This is a bit hacky, but there is no known better solution
            // as this service will get reused (at least if not sandboxed).
            // In fact, we might really want to always exit() from onDestroy(), not just from
            // the early return here.
            System.exit(0);
            return;
        }
        synchronized (mLibraryInitializedLock) {
            try {
                while (!mLibraryInitialized) {
                    // Avoid a potential race in calling through to native code before the library
                    // has loaded.
                    mLibraryInitializedLock.wait();
                }
            } catch (InterruptedException e) {
                // Ignore
            }
        }
        mDelegate.onDestroy();
    }

    /*
     * Returns the communication channel to the service. Note that even if multiple clients were to
     * connect, we should only get one call to this method. So there is no need to synchronize
     * member variables that are only set in this method and accessed from binder methods, as binder
     * methods can't be called until this method returns.
     * @param intent The intent that was used to bind to the service.
     * @return the binder used by the client to setup the connection.
     */
    @Override
    public IBinder onBind(Intent intent) {
        assert !mServiceBound;

        // We call stopSelf() to request that this service be stopped as soon as the client unbinds.
        // Otherwise the system may keep it around and available for a reconnect. The child
        // processes do not currently support reconnect; they must be initialized from scratch every
        // time.
        stopSelf();

        mBindToCallerCheck =
                intent.getBooleanExtra(ChildProcessConstants.EXTRA_BIND_TO_CALLER, false);
        mServiceBound = true;
        mDelegate.onServiceBound(intent);
        // Don't block bind() with any extra work, post it to the application thread instead.
        new Handler(Looper.getMainLooper())
                .post(() -> mDelegate.preloadNativeLibrary(getApplicationContext()));
        return mBinder;
    }

    private void processConnectionBundle(Bundle bundle, List<IBinder> clientInterfaces) {
        // Required to unparcel FileDescriptorInfo.
        ClassLoader classLoader = getApplicationContext().getClassLoader();
        bundle.setClassLoader(classLoader);
        synchronized (mMainThread) {
            if (mCommandLineParams == null) {
                mCommandLineParams =
                        bundle.getStringArray(ChildProcessConstants.EXTRA_COMMAND_LINE);
                mMainThread.notifyAll();
            }
            // We must have received the command line by now
            assert mCommandLineParams != null;
            Parcelable[] fdInfosAsParcelable =
                    bundle.getParcelableArray(ChildProcessConstants.EXTRA_FILES);
            if (fdInfosAsParcelable != null) {
                // For why this arraycopy is necessary:
                // http://stackoverflow.com/questions/8745893/i-dont-get-why-this-classcastexception-occurs
                mFdInfos = new FileDescriptorInfo[fdInfosAsParcelable.length];
                System.arraycopy(fdInfosAsParcelable, 0, mFdInfos, 0, fdInfosAsParcelable.length);
            }
            mDelegate.onConnectionSetup(bundle, clientInterfaces);
            mMainThread.notifyAll();
        }
    }

    /**
     * Helper for registering FileDescriptorInfo objects with GlobalFileDescriptors or
     * FileDescriptorStore.
     * This includes the IPC channel, the crash dump signals and resource related
     * files.
     */
    private static native void nativeRegisterFileDescriptors(
            String[] keys, int[] id, int[] fd, long[] offset, long[] size);

    /**
     * Force the child process to exit.
     */
    private static native void nativeExitChildProcess();
}
