// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link NativeMessagingManager} and {@link NativeMessagingConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativeMessagingManagerTest {
    private static final String TARGET_PACKAGE = "com.example.extensionreceiver";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    private TestContext mTestContext;
    private NativeMessagingManager mManager;

    private static class TestContext extends ContextWrapper {
        private ServiceConnection mServiceConnection;
        private Intent mBindIntent;
        private boolean mBindServiceResult = true;

        public TestContext(Context base) {
            super(base);
        }

        public void setBindServiceResult(boolean result) {
            mBindServiceResult = result;
        }

        @Override
        public boolean bindService(Intent service, ServiceConnection conn, int flags) {
            mBindIntent = service;
            mServiceConnection = conn;
            return mBindServiceResult;
        }

        public void triggerServiceConnected(IBinder service) {
            mServiceConnection.onServiceConnected(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"), service);
        }

        public void triggerServiceDisconnected() {
            mServiceConnection.onServiceDisconnected(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"));
        }

        public void triggerNullBinding() {
            mServiceConnection.onNullBinding(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"));
        }
    }

    @Before
    public void setUp() {
        Mockito.when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mTestContext = new TestContext(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mTestContext);
        mManager = NativeMessagingManager.getForProfile(mProfile);
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            mManager.destroy();
        }
    }

    @Test
    public void testConnectAndDisconnectService() {
        String error = mManager.connect(TARGET_PACKAGE);
        Assert.assertNull(error);

        NativeMessagingConnection connection = mManager.getConnectionForTesting(TARGET_PACKAGE);
        Assert.assertNotNull(connection);
        Assert.assertTrue(connection.isBound());
        Assert.assertNull(connection.getServiceForTesting());

        IBrowserNativeMessageService fakeService = new IBrowserNativeMessageService.Stub() {};
        mTestContext.triggerServiceConnected(fakeService.asBinder());

        IBrowserNativeMessageService service = connection.getServiceForTesting();
        Assert.assertNotNull(service);

        // Simulate app service crash / disconnect.
        mTestContext.triggerServiceDisconnected();

        Assert.assertFalse(connection.isBound());
        Assert.assertNull(connection.getServiceForTesting());
        Assert.assertNull(mManager.getConnectionForTesting(TARGET_PACKAGE));
    }

    @Test
    public void testConnectNullBinding() {
        String error = mManager.connect(TARGET_PACKAGE);
        Assert.assertNull(error);

        NativeMessagingConnection connection = mManager.getConnectionForTesting(TARGET_PACKAGE);
        Assert.assertNotNull(connection);
        Assert.assertTrue(connection.isBound());

        mTestContext.triggerNullBinding();

        Assert.assertFalse(connection.isBound());
        Assert.assertNull(mManager.getConnectionForTesting(TARGET_PACKAGE));
    }

    @Test
    public void testConnectAppDoesNotExist() {
        mTestContext.setBindServiceResult(false);

        String error = mManager.connect("com.nonexistent.app");
        Assert.assertNotNull(error);
        Assert.assertEquals("Error: Unable to connect to com.nonexistent.app", error);
        Assert.assertNull(mManager.getConnectionForTesting("com.nonexistent.app"));
    }
}
