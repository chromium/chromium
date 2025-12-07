// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.gtest_apk;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.Process;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Queue;
import java.util.concurrent.atomic.AtomicBoolean;

/** An Instrumentation that runs tests based on NativeTest. */
public class NativeTestInstrumentationTestRunner extends Instrumentation {
    private static final String EXTRA_NATIVE_TEST_ACTIVITY =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity";
    private static final String EXTRA_SHARD_NANO_TIMEOUT =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardNanoTimeout";
    private static final String EXTRA_SHARD_SIZE_LIMIT =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardSizeLimit";
    private static final String EXTRA_STDOUT_FILE =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.StdoutFile";
    private static final String EXTRA_TEST_LIST_FILE =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.TestList";
    private static final String EXTRA_TEST =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.Test";
    // An extra to indicate if user data dir should be kept when running the
    // given test list. no-op if given a single test.
    private static final String EXTRA_KEEP_USER_DATA_DIR =
            "org.chromium.native_test.NativeTestInstrumentationTestRunner.KeepUserDataDir";

    private static final String TAG = "NativeTestRunner";

    private static final long DEFAULT_SHARD_NANO_TIMEOUT = 60 * 1000000000L;
    // Default to no size limit.
    private static final int DEFAULT_SHARD_SIZE_LIMIT = 0;

    private final Handler mHandler = new Handler();
    private final Handler mStartHandler = new Handler();
    private final Bundle mLogBundle = new Bundle();
    private final SparseArray<ShardMonitor> mMonitors = new SparseArray<ShardMonitor>();
    private String mNativeTestActivity;
    private TestStatusReceiver mReceiver;
    private final Queue<ShardMetadata> mShards = new ArrayDeque<ShardMetadata>();
    private long mShardNanoTimeout = DEFAULT_SHARD_NANO_TIMEOUT;
    private int mShardSizeLimit = DEFAULT_SHARD_SIZE_LIMIT;
    private File mStdoutFile;
    private Bundle mTransparentArguments;
    private boolean mKeepUserDataDir;
    private final ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {}

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    getContext().unbindService(this);
                }
            };

    @Override
    public void onCreate(Bundle arguments) {
        mTransparentArguments = new Bundle(arguments);

        mNativeTestActivity = arguments.getString(EXTRA_NATIVE_TEST_ACTIVITY);
        if (mNativeTestActivity == null) {
            Log.e(
                    TAG,
                    "Unable to find org.chromium.native_test.NativeUnitTestActivity extra on "
                            + "NativeTestInstrumentationTestRunner launch intent.");
            finish(Activity.RESULT_CANCELED, new Bundle());
            return;
        }
        mTransparentArguments.remove(EXTRA_NATIVE_TEST_ACTIVITY);

        String shardNanoTimeout = arguments.getString(EXTRA_SHARD_NANO_TIMEOUT);
        if (shardNanoTimeout != null) mShardNanoTimeout = Long.parseLong(shardNanoTimeout);
        mTransparentArguments.remove(EXTRA_SHARD_NANO_TIMEOUT);

        String shardSizeLimit = arguments.getString(EXTRA_SHARD_SIZE_LIMIT);
        if (shardSizeLimit != null) mShardSizeLimit = Integer.parseInt(shardSizeLimit);
        mTransparentArguments.remove(EXTRA_SHARD_SIZE_LIMIT);

        String stdoutFile = arguments.getString(EXTRA_STDOUT_FILE);
        if (stdoutFile != null) {
            mStdoutFile = new File(stdoutFile);
        } else {
            try {
                mStdoutFile =
                        File.createTempFile(
                                ".temp_stdout_", ".txt", Environment.getExternalStorageDirectory());
                Log.i(TAG, "stdout file created: " + mStdoutFile.getAbsolutePath());
            } catch (IOException e) {
                Log.e(TAG, "Unable to create temporary stdout file.", e);
                finish(Activity.RESULT_CANCELED, new Bundle());
                return;
            }
        }

        mTransparentArguments.remove(EXTRA_STDOUT_FILE);

        mKeepUserDataDir = arguments.containsKey(EXTRA_KEEP_USER_DATA_DIR);
        if (mKeepUserDataDir) {
            Log.i(TAG, "user data dir will be kept between the given tests.");
        }
        mTransparentArguments.remove(EXTRA_KEEP_USER_DATA_DIR);

        String singleTest = arguments.getString(EXTRA_TEST);
        if (singleTest != null) {
            mShards.add(new ShardMetadata(singleTest, false));
        }

        String testListFilePath = arguments.getString(EXTRA_TEST_LIST_FILE);
        if (testListFilePath != null) {
            File testListFile = new File(testListFilePath);

            boolean isFirstTest = true;
            try {
                BufferedReader testListFileReader =
                        new BufferedReader(new FileReader(testListFile));

                String test;
                ArrayList<String> workingShard = new ArrayList<String>();
                while ((test = testListFileReader.readLine()) != null) {
                    // For multiple tests passed via a test list file, data
                    // in user data dir should:
                    //  - Be cleaned before the 1st test, and
                    //  - Kept in the followed tests.
                    boolean keepUserDataDirForTest = mKeepUserDataDir && !isFirstTest;
                    isFirstTest = false;
                    workingShard.add(test);
                    if (workingShard.size() == mShardSizeLimit) {
                        mShards.add(
                                new ShardMetadata(
                                        TextUtils.join(":", workingShard), keepUserDataDirForTest));
                        workingShard = new ArrayList<String>();
                    }
                }

                if (!workingShard.isEmpty()) {
                    boolean keepUserDataDirForTest = mKeepUserDataDir && !isFirstTest;
                    mShards.add(
                            new ShardMetadata(
                                    TextUtils.join(":", workingShard), keepUserDataDirForTest));
                }

                testListFileReader.close();
            } catch (IOException e) {
                Log.e(TAG, "Error reading " + testListFile.getAbsolutePath(), e);
            }
        }
        mTransparentArguments.remove(EXTRA_TEST_LIST_FILE);

        start();
    }

    @Override
    @SuppressLint("DefaultLocale")
    public void onStart() {
        super.onStart();

        mReceiver = new TestStatusReceiver();
        mReceiver.register(getContext());
        mReceiver.registerCallback(
                new TestStatusReceiver.TestRunCallback() {
                    @Override
                    public void testRunStarted(int pid) {
                        mStartHandler.removeCallbacksAndMessages(null);
                        if (pid != Process.myPid()) {
                            ShardMonitor m =
                                    new ShardMonitor(pid, System.nanoTime() + mShardNanoTimeout);
                            mMonitors.put(pid, m);
                            mHandler.post(m);
                        }
                    }

                    @Override
                    public void testRunFinished(int pid) {
                        ShardMonitor m = mMonitors.get(pid);
                        if (m != null) {
                            m.stopped();
                            mMonitors.remove(pid);
                        }
                        mHandler.post(new ShardEnder(pid));
                    }

                    @Override
                    public void uncaughtException(int pid, String stackTrace) {
                        mLogBundle.putString(
                                Instrumentation.REPORT_KEY_STREAMRESULT,
                                String.format(
                                        "Uncaught exception in test process (pid: %d)%n%s%n",
                                        pid, stackTrace));
                        sendStatus(0, mLogBundle);
                    }
                });

        mHandler.post(new ShardStarter());
    }

    /** Stores the metadata for a test shard. */
    private static class ShardMetadata {
        private final String mGtestFilter;
        private final boolean mKeepUserDataDir;

        public ShardMetadata(String gtestFilter, boolean keepUserDataDir) {
            mGtestFilter = gtestFilter;
            mKeepUserDataDir = keepUserDataDir;
        }

        public String getGtestFilter() {
            return mGtestFilter;
        }

        public boolean shouldKeepUserDataDir() {
            return mKeepUserDataDir;
        }
    }

    /** Monitors a test shard's execution. */
    private class ShardMonitor implements Runnable {
        private static final int MONITOR_FREQUENCY_MS = 1000;

        private final long mExpirationNanoTime;
        private final int mPid;
        private final AtomicBoolean mStopped;

        public ShardMonitor(int pid, long expirationNanoTime) {
            mPid = pid;
            mExpirationNanoTime = expirationNanoTime;
            mStopped = new AtomicBoolean(false);
        }

        public void stopped() {
            mStopped.set(true);
        }

        @Override
        public void run() {
            if (mStopped.get()) {
                return;
            }

            if (isAppProcessAlive(getContext(), mPid)) {
                if (System.nanoTime() > mExpirationNanoTime) {
                    Log.e(TAG, String.format("Test process %d timed out.", mPid));
                    mHandler.post(new ShardEnder(mPid));
                    return;
                } else {
                    mHandler.postDelayed(this, MONITOR_FREQUENCY_MS);
                    return;
                }
            }

            Log.e(TAG, String.format("Test process %d died unexpectedly.", mPid));
            mHandler.post(new ShardEnder(mPid));
        }
    }

    private static boolean isAppProcessAlive(Context context, int pid) {
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.RunningAppProcessInfo processInfo :
                activityManager.getRunningAppProcesses()) {
            if (processInfo.pid == pid) return true;
        }
        return false;
    }

    protected Intent createShardMainIntent() {
        Intent i = new Intent(Intent.ACTION_MAIN);
        i.setComponent(new ComponentName(getContext().getPackageName(), mNativeTestActivity));
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Work around a framework issue where sometimes if we start an Activity too quickly after
        // killing the Activity's old process the Activity launches into the old task and fails
        // to come to the foreground.
        i.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
        i.putExtras(mTransparentArguments);
        if (mShards != null && !mShards.isEmpty()) {
            ShardMetadata shardMetadata = mShards.remove();
            i.putExtra(NativeTestIntent.EXTRA_GTEST_FILTER, shardMetadata.getGtestFilter());
            i.putExtra(
                    NativeTestIntent.EXTRA_KEEP_USER_DATA_DIR,
                    shardMetadata.shouldKeepUserDataDir());
        }
        i.putExtra(NativeTestIntent.EXTRA_STDOUT_FILE, mStdoutFile.getAbsolutePath());
        return i;
    }

    private Intent getProcessKeepaliveServiceIntent() {
        Intent i = new Intent();
        i.setComponent(
                new ComponentName(
                        getContext().getPackageName(),
                        "org.chromium.build.gtest_apk.ProcessKeepaliveService"));
        return i;
    }

    /** Starts the NativeTest Activity. */
    private class ShardStarter implements Runnable {
        private static final int WAIT_FOR_ACTIVITY_MILLIS = 30000;

        @Override
        public void run() {
            getContext().startActivity(createShardMainIntent());
            // Start a service to keep the test process alive after the Activity finishes. Some
            // Android 16+ devices seem to otherwise kill the process immediately after the Activity
            // is destroyed.
            getContext()
                    .bindService(
                            getProcessKeepaliveServiceIntent(),
                            mConnection,
                            Context.BIND_AUTO_CREATE);

            mStartHandler.postDelayed(
                    () -> {
                        Log.e(TAG, "Test activity failed to start.");
                        finish(Activity.RESULT_CANCELED, new Bundle());
                    },
                    WAIT_FOR_ACTIVITY_MILLIS);
        }
    }

    private class ShardEnder implements Runnable {
        private static final int WAIT_FOR_DEATH_MILLIS = 10;

        private final int mPid;

        public ShardEnder(int pid) {
            mPid = pid;
        }

        @Override
        public void run() {
            getContext().unbindService(mConnection);

            if (mPid != Process.myPid()) {
                Process.killProcess(mPid);
                try {
                    // Always wait at least once to give the OS time to kill the process. Otherwise
                    // the process occasionally fails to start correctly.
                    Thread.sleep(WAIT_FOR_DEATH_MILLIS);
                    while (isAppProcessAlive(getContext(), mPid)) {
                        Thread.sleep(WAIT_FOR_DEATH_MILLIS);
                    }
                } catch (InterruptedException e) {
                    Log.e(TAG, String.format("%d may still be alive.", mPid), e);
                }
            }
            if (mShards != null && !mShards.isEmpty()) {
                mHandler.post(new ShardStarter());
            } else {
                finish(Activity.RESULT_OK, new Bundle());
            }
        }
    }
}
