// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

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
import org.chromium.base.test.RobolectricUtil;

/** Unit tests for {@link NativeMessagingConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativeMessagingConnectionTest {
    private static final String TARGET_PACKAGE = "com.example.extensionreceiver";
    private static final String EXT_1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    private static final String EXT_2 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    private static final String EXT_FAIL = "ffffffffffffffffffffffffffffffff";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NativeMessagingConnection.Observer mObserver;
    @Mock private IExtensionNativeMessageService mMockExtensionService1;
    @Mock private IExtensionNativeMessageService mMockExtensionService2;

    private TestContext mTestContext;
    private IBrowserNativeMessageService mFakeBrowserService;

    private static class TestContext extends ContextWrapper {
        private ServiceConnection mServiceConnection;

        public TestContext(Context base) {
            super(base);
        }

        @Override
        public boolean bindService(Intent service, ServiceConnection conn, int flags) {
            mServiceConnection = conn;
            return true;
        }

        public void triggerServiceConnected(IBinder service) {
            mServiceConnection.onServiceConnected(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"), service);
        }

        public void triggerServiceDisconnected() {
            mServiceConnection.onServiceDisconnected(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"));
        }
    }

    @Before
    public void setUp() {
        mTestContext = new TestContext(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mTestContext);

        mFakeBrowserService =
                new IBrowserNativeMessageService.Stub() {
                    @Override
                    public IExtensionNativeMessageService connectExtension(
                            String extensionId, Bundle info) throws RemoteException {
                        if (EXT_1.equals(extensionId)) {
                            return mMockExtensionService1;
                        }
                        if (EXT_2.equals(extensionId)) {
                            return mMockExtensionService2;
                        }
                        return null; // EXT_FAIL returns null.
                    }
                };
    }

    // Test that:
    // - An extension connection can be initiated before the service is
    //   connected, and the connection goes through once the service is
    //   connected.
    // - Attempting to connect the same extension more than once is a no-op.
    @Test
    public void testPreBoundAndDuplicateNoOp() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        Assert.assertTrue(connection.isBound());

        // 1. Request extension before service is connected.
        String error1 = connection.connectExtension(EXT_1);
        Assert.assertNull(error1);

        var session = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session);

        // 2. Duplicate call should no-op.
        String error2 = connection.connectExtension(EXT_1);
        Assert.assertNull(error2);
        Assert.assertEquals(session, connection.getSessionForTesting(EXT_1));

        // 3. Connect package service and flush background tasks.
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(mMockExtensionService1, session.getServiceForTesting());
    }

    // Check that two separate connected extensions will result in distinct
    // ExtensionSessions and IExtensionNativeMessageServices.
    @Test
    public void testPostBoundAndMultiSession() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        Assert.assertTrue(connection.isBound());

        // 1. Service connects first.
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        // 2. Connect two distinct extensions.
        Assert.assertNull(connection.connectExtension(EXT_1));
        Assert.assertNull(connection.connectExtension(EXT_2));

        RobolectricUtil.runAllBackgroundAndUi();

        var session1 = connection.getSessionForTesting(EXT_1);
        var session2 = connection.getSessionForTesting(EXT_2);

        Assert.assertNotNull(session1);
        Assert.assertNotNull(session2);
        Assert.assertNotEquals(session1, session2);

        Assert.assertEquals(mMockExtensionService1, session1.getServiceForTesting());
        Assert.assertEquals(mMockExtensionService2, session2.getServiceForTesting());
    }

    // Test authentication failure for a single extension (the service at the
    // other end returns null or an error instead of an
    // IExtensionNativeMessageService). This also tests that the browser unbinds
    // from the service if there are no more sessions.
    @Test
    public void testSingleExtensionFailureUnbind() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        Assert.assertTrue(connection.isBound());

        // 1. Connect extension that will fail auth (returns null).
        Assert.assertNull(connection.connectExtension(EXT_FAIL));

        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // 2. Verify failed session is purged and falling-edge unbind triggers.
        Assert.assertNull(connection.getSessionForTesting(EXT_FAIL));
        Assert.assertFalse(connection.isBound());
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
    }

    // Test that authentication failure from one extension does not affect the
    // connections for other valid extensions.
    @Test
    public void testMultiExtensionFailureIsolation() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        Assert.assertTrue(connection.isBound());

        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        // Connect valid extension and failing extension.
        Assert.assertNull(connection.connectExtension(EXT_1));
        Assert.assertNull(connection.connectExtension(EXT_FAIL));

        RobolectricUtil.runAllBackgroundAndUi();

        // EXT_1 should remain connected.
        var session1 = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session1);
        Assert.assertEquals(mMockExtensionService1, session1.getServiceForTesting());

        // EXT_FAIL should be purged.
        Assert.assertNull(connection.getSessionForTesting(EXT_FAIL));

        // Connection remains bound because EXT_1 is active.
        Assert.assertTrue(connection.isBound());
        Mockito.verify(mObserver, Mockito.never()).onUnbound(TARGET_PACKAGE);
    }

    // Simulate a service disconnect and verify that the browser cleans up
    // state.
    @Test
    public void testAppServiceCrashTeardown() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        Assert.assertTrue(connection.isBound());

        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        Assert.assertNull(connection.connectExtension(EXT_1));
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));

        // App crashes / disconnects.
        mTestContext.triggerServiceDisconnected();

        Assert.assertFalse(connection.isBound());
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
    }
}
