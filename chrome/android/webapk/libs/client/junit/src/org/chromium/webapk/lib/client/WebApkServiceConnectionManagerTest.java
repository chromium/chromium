// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for {@link org.chromium.webapk.lib.client.WebApkServiceConnectionManager}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
public class WebApkServiceConnectionManagerTest {

    private static final String WEB_APK_PACKAGE = "com.webapk.package";

    private static final String CATEGORY_WEBAPK_SERVICE_API = "android.intent.category.WEBAPK_API";

    private ShadowApplication mShadowApplication;
    private WebApkServiceConnectionManager mConnectionManager;

    private class TestCallback implements WebApkServiceConnectionManager.ConnectionCallback {
        public boolean mGotResult;

        @Override
        public void onConnected(IBinder service) {
            mGotResult = true;
        }
    }

    @Before
    public void setUp() {
        mShadowApplication = Shadows.shadowOf(RuntimeEnvironment.application);
        mConnectionManager =
                new WebApkServiceConnectionManager(CATEGORY_WEBAPK_SERVICE_API, null /* action*/);
        mShadowApplication.setComponentNameAndServiceForBindService(
                new ComponentName(WEB_APK_PACKAGE, ""), Mockito.mock(IBinder.class));
    }

    /**
     * Test that a connection request to a WebAPK's service does not create a new connection if one
     * already exists.
     */
    @Test
    public void testAfterConnectionEstablished() throws Exception {
        TestCallback callback1 = new TestCallback();
        TestCallback callback2 = new TestCallback();

        mConnectionManager.connect(RuntimeEnvironment.application, WEB_APK_PACKAGE, callback1);
        mConnectionManager.connect(RuntimeEnvironment.application, WEB_APK_PACKAGE, callback2);

        // Only one connection should have been created.
        Assert.assertEquals(WEB_APK_PACKAGE, getNextStartedServicePackage());
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
    public void testConnectWhileConnectionBeingEstablished() throws Exception {
        // Context for testing {@link Context#bindService()} occurring asynchronously.
        class AsyncBindContext extends ContextWrapper {
            private ServiceConnection mConnection;

            public AsyncBindContext() {
                super(null);
            }

            // Establish pending connection created in {@link #bindService}.
            public void establishServiceConnection() {
                if (mConnection != null) {
                    mConnection.onServiceConnected(null, null);
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
        mConnectionManager.connect(asyncBindContext, WEB_APK_PACKAGE, callback1);
        mConnectionManager.connect(asyncBindContext, WEB_APK_PACKAGE, callback2);

        // The connection has not been established yet. Neither of the callbacks should have been
        // called.
        Assert.assertFalse(callback1.mGotResult);
        Assert.assertFalse(callback2.mGotResult);

        // Establishing the connection should cause both callbacks to be called.
        asyncBindContext.establishServiceConnection();
        Assert.assertTrue(callback1.mGotResult);
        Assert.assertTrue(callback2.mGotResult);
    }

    /**
     * Test reconnecting to a WebAPK's service.
     */
    @Test
    public void testDisconnectConnect() throws Exception {
        mConnectionManager.connect(
                RuntimeEnvironment.application, WEB_APK_PACKAGE, new TestCallback());
        Assert.assertEquals(WEB_APK_PACKAGE, getNextStartedServicePackage());
        Assert.assertEquals(null, getNextStartedServicePackage());

        mConnectionManager.disconnectAll(RuntimeEnvironment.application);
        mConnectionManager.connect(
                RuntimeEnvironment.application, WEB_APK_PACKAGE, new TestCallback());
        Assert.assertEquals(WEB_APK_PACKAGE, getNextStartedServicePackage());
        Assert.assertEquals(null, getNextStartedServicePackage());
    }

    /**
     * Returns the package name of the most recently started service.
     */
    public String getNextStartedServicePackage() {
        Intent intent = mShadowApplication.getNextStartedService();
        return (intent == null) ? null : intent.getPackage();
    }
}
