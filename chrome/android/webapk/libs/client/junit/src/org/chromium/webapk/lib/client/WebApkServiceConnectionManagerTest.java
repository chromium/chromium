// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.text.TextUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

import java.util.ArrayList;
import java.util.HashSet;

/** Unit tests for {@link org.chromium.webapk.lib.client.WebApkServiceConnectionManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkServiceConnectionManagerTest {
    private static final String WEBAPK_PACKAGE = "com.webapk.package";

    private static final String CATEGORY_WEBAPK_SERVICE_API = "android.intent.category.WEBAPK_API";

    private ShadowApplication mShadowApplication;
    private WebApkServiceConnectionManager mConnectionManager;

    private static class TestCallback implements WebApkServiceConnectionManager.ConnectionCallback {
        public boolean mGotResult;
        public IBinder mService;

        @Override
        public void onConnected(IBinder service) {
            mGotResult = true;
            mService = service;
        }
    }

    @Before
    public void setUp() {
        mShadowApplication = Shadows.shadowOf(RuntimeEnvironment.application);
        mShadowApplication.setComponentNameAndServiceForBindService(
                new ComponentName(WEBAPK_PACKAGE, ""), Mockito.mock(IBinder.class));
        mConnectionManager =
                new WebApkServiceConnectionManager(
                        TaskTraits.BEST_EFFORT_MAY_BLOCK,
                        CATEGORY_WEBAPK_SERVICE_API,
                        /* action= */ null);
    }

    @After
    public void tearDown() {
        mConnectionManager.disconnectAll(RuntimeEnvironment.application);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    /**
     * Test that a connection request to a WebAPK's service does not create a new connection if one
     * already exists.
     */
    @Test
    public void testAfterConnectionEstablished() {
        TestCallback callback1 = new TestCallback();
        TestCallback callback2 = new TestCallback();

        mConnectionManager.connect(RuntimeEnvironment.application, WEBAPK_PACKAGE, callback1);
        RobolectricUtil.runAllBackgroundAndUi();
        mConnectionManager.connect(RuntimeEnvironment.application, WEBAPK_PACKAGE, callback2);
        RobolectricUtil.runAllBackgroundAndUi();

        // Only one connection should have been created.
        Assert.assertEquals(WEBAPK_PACKAGE, getNextStartedServicePackage());
        Assert.assertEquals(null, getNextStartedServicePackage());

        // Both callbacks should have been called.
        Assert.assertTrue(callback1.mGotResult);
        Assert.assertTrue(callback2.mGotResult);
    }

    /**
     * Test connecting to a WebAPK when Chrome is in the process of establishing a connection to the
     * WebAPK but has not established a connection yet.
     */
    @Test
    public void testConnectWhileConnectionBeingEstablished() {
        // Context for testing {@link Context#bindService()} occurring asynchronously.
        class AsyncBindContext extends ContextWrapper {
            private ServiceConnection mConnection;

            public AsyncBindContext() {
                super(null);
            }

            // Establish pending connection created in {@link #bindService}.
            public void establishServiceConnection() {
                if (mConnection != null) {
                    mConnection.onServiceConnected(
                            new ComponentName(WEBAPK_PACKAGE, "random"),
                            Mockito.mock(IBinder.class));
                }
            }

            @Override
            public Context getApplicationContext() {
                // Need to return real context so that ContextUtils#fetchAppSharedPreferences() does
                // not crash.
                return RuntimeEnvironment.application;
            }

            // Create pending connection.
            @Override
            public boolean bindService(Intent service, ServiceConnection connection, int flags) {
                mConnection = connection;
                return true;
            }
        }

        AsyncBindContext asyncBindContext = new AsyncBindContext();

        TestCallback callback1 = new TestCallback();
        TestCallback callback2 = new TestCallback();
        TestCallback callback3 = new TestCallback();

        mConnectionManager.connect(asyncBindContext, WEBAPK_PACKAGE, callback1);
        mConnectionManager.connect(asyncBindContext, WEBAPK_PACKAGE, callback2);
        RobolectricUtil.runAllBackgroundAndUi();

        mConnectionManager.connect(asyncBindContext, WEBAPK_PACKAGE, callback3);
        RobolectricUtil.runAllBackgroundAndUi();

        // The connection has not been established yet. None of the callbacks should have been
        // called.
        Assert.assertFalse(callback1.mGotResult);
        Assert.assertFalse(callback2.mGotResult);
        Assert.assertFalse(callback3.mGotResult);

        // Establishing the connection should cause all of the callbacks to be called.
        asyncBindContext.establishServiceConnection();
        Assert.assertTrue(callback1.mGotResult);
        Assert.assertTrue(callback2.mGotResult);
        Assert.assertTrue(callback3.mGotResult);
    }

    /**
     * Context which records order of {@link Context#bindService()} and {@link
     * Context#unbindService()} calls.
     */
    private static class BindUnbindRecordingContext extends ContextWrapper {
        private final String mRecordPackage;
        private final ArrayList<Boolean> mStartStopServiceSequence = new ArrayList<>();
        private final HashSet<ServiceConnection> mTrackedConnections = new HashSet<>();

        public BindUnbindRecordingContext(String recordPackage) {
            super(null);
            mRecordPackage = recordPackage;
        }

        public ArrayList<Boolean> getStartStopServiceSequence() {
            return mStartStopServiceSequence;
        }

        @Override
        public Context getApplicationContext() {
            // Need to return real context so that ContextUtils#fetchAppSharedPreferences() does
            // not crash.
            return RuntimeEnvironment.application;
        }

        // Create pending connection.
        @Override
        public boolean bindService(Intent intent, ServiceConnection connection, int flags) {
            connection.onServiceConnected(
                    new ComponentName(mRecordPackage, "random"), Mockito.mock(IBinder.class));
            if (TextUtils.equals(intent.getPackage(), mRecordPackage)) {
                mTrackedConnections.add(connection);
                mStartStopServiceSequence.add(true);
            }
            return true;
        }

        @Override
        public void unbindService(ServiceConnection connection) {
            connection.onServiceDisconnected(new ComponentName(mRecordPackage, "random"));
            if (mTrackedConnections.contains(connection)) {
                mStartStopServiceSequence.add(false);
            }
        }
    }

    /** Test reconnecting to a WebAPK's service. */
    @Test
    public void testConnectDisconnectConnect() {
        final int flagRunBackgroundTasksAfterConnect = 0x1;
        final int flagRunBackgroundTasksAfterDisconnect = 0x2;

        final int[] testCases =
                new int[] {
                    0,
                    flagRunBackgroundTasksAfterConnect,
                    flagRunBackgroundTasksAfterDisconnect,
                    flagRunBackgroundTasksAfterConnect | flagRunBackgroundTasksAfterDisconnect
                };

        for (int testCase : testCases) {
            BindUnbindRecordingContext recordingContext =
                    new BindUnbindRecordingContext(WEBAPK_PACKAGE);
            TestCallback callback1 = new TestCallback();
            TestCallback callback2 = new TestCallback();

            mConnectionManager.connect(recordingContext, WEBAPK_PACKAGE, callback1);
            if ((testCase & flagRunBackgroundTasksAfterConnect) != 0) {
                RobolectricUtil.runAllBackgroundAndUi();
            }
            mConnectionManager.disconnectAll(recordingContext);
            if ((testCase & flagRunBackgroundTasksAfterDisconnect) != 0) {
                RobolectricUtil.runAllBackgroundAndUi();
            }
            mConnectionManager.connect(recordingContext, WEBAPK_PACKAGE, callback2);
            RobolectricUtil.runAllBackgroundAndUi();

            Assert.assertArrayEquals(
                    new Boolean[] {true, false, true},
                    recordingContext.getStartStopServiceSequence().toArray(new Boolean[0]));
            Assert.assertTrue(callback1.mGotResult);
            // |callback1.mService| can be null.
            Assert.assertTrue(callback2.mGotResult);
            Assert.assertNotNull(callback2.mService);

            mConnectionManager.disconnectAll(recordingContext);
            RobolectricUtil.runAllBackgroundAndUi();
        }
    }

    /** Returns the package name of the next started service. */
    public String getNextStartedServicePackage() {
        Intent intent = mShadowApplication.getNextStartedService();
        return (intent == null) ? null : intent.getPackage();
    }
}
