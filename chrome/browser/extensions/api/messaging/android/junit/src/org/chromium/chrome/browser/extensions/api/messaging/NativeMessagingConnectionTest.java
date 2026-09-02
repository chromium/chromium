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

import androidx.annotation.Nullable;

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
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.extensions.api.messaging.NativeMessagingConnection.DisconnectionReason;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link NativeMessagingConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
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
    public void setUp() throws RemoteException {
        mTestContext = new TestContext(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mTestContext);

        IExtensionNativeMessagePort mockPort = Mockito.mock(IExtensionNativeMessagePort.class);
        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onSuccess(mockPort);
                            return null;
                        })
                .when(mMockExtensionService1)
                .connectPort(Mockito.any(), Mockito.any());
        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onSuccess(mockPort);
                            return null;
                        })
                .when(mMockExtensionService2)
                .connectPort(Mockito.any(), Mockito.any());

        mFakeBrowserService =
                new IBrowserNativeMessageService.Stub() {
                    @Override
                    public void connectExtension(
                            String extensionId, Bundle info, IConnectExtensionCallback callback)
                            throws RemoteException {
                        if (EXT_1.equals(extensionId)) {
                            Assert.assertNotNull(info);
                            Assert.assertTrue(info.getBoolean("isVerified"));
                            callback.onSuccess(mMockExtensionService1);
                            return;
                        }
                        if (EXT_2.equals(extensionId)) {
                            Assert.assertNotNull(info);
                            Assert.assertFalse(info.getBoolean("isVerified"));
                            callback.onSuccess(mMockExtensionService2);
                            return;
                        }
                        callback.onError("Failed to connect extension");
                    }
                };
    }

    private static @Nullable String connectExtension(
            NativeMessagingConnection connection, String extensionId, boolean isVerifiedExtension) {
        return connection.addPort(extensionId, isVerifiedExtension, new NativeMessageAndroidPort());
    }

    private static @Nullable String connectExtension(
            NativeMessagingConnection connection, String extensionId) {
        return connectExtension(connection, extensionId, /* isVerifiedExtension= */ true);
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
        String error1 = connectExtension(connection, EXT_1);
        Assert.assertNull(error1);

        var session = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session);

        // 2. Duplicate call should no-op.
        String error2 = connectExtension(connection, EXT_1);
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

        // 2. Connect two distinct extensions (EXT_1 verified, EXT_2 unverified).
        Assert.assertNull(connectExtension(connection, EXT_1, /* isVerifiedExtension= */ true));
        Assert.assertNull(connectExtension(connection, EXT_2, /* isVerifiedExtension= */ false));

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
        Assert.assertNull(connectExtension(connection, EXT_FAIL));

        var reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Extensions.NativeMessaging.Android.DisconnectionReason",
                        DisconnectionReason.CLEAN_UNBIND);
        var durationWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Extensions.NativeMessaging.Android.UnexpectedDisconnectionDuration")
                        .build();

        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // 2. Verify failed session is purged and falling-edge unbind triggers.
        Assert.assertNull(connection.getSessionForTesting(EXT_FAIL));
        Assert.assertFalse(connection.isBound());
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
        reasonWatcher.assertExpected();
        durationWatcher.assertExpected();
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
        Assert.assertNull(connectExtension(connection, EXT_1));
        Assert.assertNull(connectExtension(connection, EXT_FAIL));

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
        Assert.assertNull(connectExtension(connection, EXT_1));
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));

        ShadowSystemClock.advanceBy(5000, TimeUnit.MILLISECONDS);

        var reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Extensions.NativeMessaging.Android.DisconnectionReason",
                        DisconnectionReason.SERVICE_DISCONNECTED);
        var durationWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Extensions.NativeMessaging.Android.UnexpectedDisconnectionDuration");

        // App crashes / disconnects.
        mTestContext.triggerServiceDisconnected();

        Assert.assertFalse(connection.isBound());
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
        reasonWatcher.assertExpected();
        durationWatcher.assertExpected();
    }

    // Test that addPort on an unbound connection returns getUnableToConnectError.
    @Test
    public void testAddPortToUnboundConnection() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        connection.unbind();
        Assert.assertFalse(connection.isBound());

        String error = connectExtension(connection, EXT_1);
        Assert.assertEquals(
                NativeMessagingConnection.getUnableToConnectError(TARGET_PACKAGE), error);
    }

    // Test that onExtensionUnloaded calls closeConnection() and removes the session.
    @Test
    public void testOnExtensionUnloadedCallsCloseConnection() throws RemoteException {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        Assert.assertNull(connectExtension(connection, EXT_1));
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));

        connection.onExtensionUnloaded(EXT_1);

        Mockito.verify(mMockExtensionService1).closeConnection();
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Assert.assertFalse(connection.isBound());
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
    }

    // Test that if an extension is unloaded while authentication is still in-flight,
    // when the external app returns the IExtensionNativeMessageService, closeConnection()
    // is called on it to avoid leaking the session.
    @Test
    public void testOnExtensionUnloadedWhileAuthenticationInFlight() throws RemoteException {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        // Initiate connection, but do not run background tasks yet.
        Assert.assertNull(connectExtension(connection, EXT_1));

        // Extension is unloaded while connectExtension is in-flight on the background thread.
        connection.onExtensionUnloaded(EXT_1);

        // Run background and UI tasks.
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify the app's returned session stub received closeConnection().
        Mockito.verify(mMockExtensionService1).closeConnection();
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Assert.assertFalse(connection.isBound());
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
    }

    // Test that if an external app misbehaves and invokes both onSuccess and onError,
    // or onSuccess multiple times, only the first call is processed and subsequent
    // services are closed.
    @Test
    public void testConnectExtensionCallbackCalledMultipleTimes() throws RemoteException {
        IExtensionNativeMessageService duplicateService =
                Mockito.mock(IExtensionNativeMessageService.class);

        IBrowserNativeMessageService misbehavingService =
                new IBrowserNativeMessageService.Stub() {
                    @Override
                    public void connectExtension(
                            String extensionId, Bundle info, IConnectExtensionCallback callback)
                            throws RemoteException {
                        callback.onSuccess(mMockExtensionService1);
                        callback.onSuccess(duplicateService);
                        callback.onError("Conflicting error message");
                    }
                };

        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(misbehavingService.asBinder());

        Assert.assertNull(connectExtension(connection, EXT_1));
        RobolectricUtil.runAllBackgroundAndUi();

        // First service is retained in session.
        var session = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session);
        Assert.assertEquals(mMockExtensionService1, session.getServiceForTesting());

        // Duplicate service was immediately closed.
        Mockito.verify(duplicateService).closeConnection();
    }

    @Test
    public void testBindServiceTimeout() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        Assert.assertTrue(connection.isBound());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        final String[] closedError = new String[1];
        port.setTestObserver(
                new NativeMessageAndroidPort.TestObserver() {
                    @Override
                    public void onMessageFromApp(String message) {}

                    @Override
                    public void onChannelClosed(String errorMessage) {
                        closedError[0] = errorMessage;
                    }
                });
        Assert.assertNull(connection.addPort(EXT_1, false, port));

        var reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Extensions.NativeMessaging.Android.DisconnectionReason",
                        DisconnectionReason.SERVICE_CONNECTION_TIMED_OUT);
        var durationWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Extensions.NativeMessaging.Android.UnexpectedDisconnectionDuration")
                        .build();

        // Advance time to trigger the timeout.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertFalse(connection.isBound());
        Assert.assertEquals("Unable to connect to " + TARGET_PACKAGE + ".", closedError[0]);
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
        reasonWatcher.assertExpected();
        durationWatcher.assertExpected();
    }

    @Test
    public void testConnectExtensionTimeout() {
        IBrowserNativeMessageService unresponsiveService =
                new IBrowserNativeMessageService.Stub() {
                    @Override
                    public void connectExtension(
                            String extensionId, Bundle info, IConnectExtensionCallback cb) {
                        // Unresponsive: never invokes cb.onSuccess or cb.onError.
                    }
                };

        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(unresponsiveService.asBinder());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        final String[] closedError = new String[1];
        port.setTestObserver(
                new NativeMessageAndroidPort.TestObserver() {
                    @Override
                    public void onMessageFromApp(String message) {}

                    @Override
                    public void onChannelClosed(String errorMessage) {
                        closedError[0] = errorMessage;
                    }
                });

        Assert.assertNull(connection.addPort(EXT_1, true, port));
        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));

        // Advance time to trigger the connectExtension timeout.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Assert.assertEquals("Unable to connect to " + TARGET_PACKAGE + ".", closedError[0]);
        Assert.assertFalse(connection.isBound());
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
    }

    @Test
    public void testOnServiceConnectedAfterTimeoutDoesNotAssignService() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        Assert.assertTrue(connection.isBound());

        // Advance time to trigger the bind timeout.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertFalse(connection.isBound());
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
        Assert.assertNull(connection.getServiceForTesting());

        // Simulate onServiceConnected firing after timeout has unbound the connection.
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        // It should no-op: mService remains null and connection remains unbound.
        Assert.assertNull(connection.getServiceForTesting());
        Assert.assertFalse(connection.isBound());
    }

    // Test that when the last open port is destroyed, the extension session
    // disconnects after IDLE_DISCONNECT_TIMEOUT_MS and unbinds from the app.
    @Test
    public void testIdleDisconnectAfterTimeout() throws RemoteException {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port));
        RobolectricUtil.runAllBackgroundAndUi();

        var session = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session);
        Assert.assertEquals(mMockExtensionService1, session.getServiceForTesting());

        // Close the only open port.
        session.onPortDestroying(port);

        // Advance half of the timeout (30s). Session should still be connected.
        ShadowLooper.idleMainLooper(30, TimeUnit.SECONDS);
        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1, Mockito.never()).closeConnection();
        Assert.assertTrue(connection.isBound());

        // Advance remaining 30 seconds.
        ShadowLooper.idleMainLooper(30, TimeUnit.SECONDS);

        // Session should be disconnected, closeConnection called, and connection unbound.
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1).closeConnection();
        Assert.assertFalse(connection.isBound());
        Mockito.verify(mObserver).onUnbound(TARGET_PACKAGE);
    }

    // Test that if a new port is added while the idle countdown is ticking,
    // the idle disconnect is canceled.
    @Test
    public void testIdleDisconnectCanceledWhenPortAdded() throws RemoteException {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port1));
        RobolectricUtil.runAllBackgroundAndUi();

        var session = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session);

        // Close the only open port.
        session.onPortDestroying(port1);

        // Advance 20 seconds.
        ShadowLooper.idleMainLooper(20, TimeUnit.SECONDS);

        // Add a new port before timeout expires.
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port2));

        // Advance past the original 60-second mark (another 45s -> t = 65s).
        ShadowLooper.idleMainLooper(45, TimeUnit.SECONDS);

        // Session must still be connected because port2 is open.
        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1, Mockito.never()).closeConnection();
        Assert.assertTrue(connection.isBound());
    }

    // Test the 1 -> 0 -> 1 -> 0 edge case:
    // Port1 closed at t=0s.
    // Port2 added at t=20s.
    // Port2 closed at t=25s.
    // At t=60s (original timeout), only 35s have passed since port2 closed, so it
    // must NOT disconnect until t=85s (60s after port2 closed).
    @Test
    public void testIdleDisconnectResetWhenPortAddedAndRemovedAgain() throws RemoteException {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port1));
        RobolectricUtil.runAllBackgroundAndUi();

        var session = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session);

        // t = 0: Close port1.
        session.onPortDestroying(port1);

        // t = 20s: Add port2.
        ShadowLooper.idleMainLooper(20, TimeUnit.SECONDS);
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port2));

        // t = 25s: Close port2.
        ShadowLooper.idleMainLooper(5, TimeUnit.SECONDS);
        session.onPortDestroying(port2);

        // t = 60s (original timeout): only 35s since port2 closed. Must NOT disconnect!
        ShadowLooper.idleMainLooper(35, TimeUnit.SECONDS);
        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1, Mockito.never()).closeConnection();
        Assert.assertTrue(connection.isBound());

        // t = 85s (60s since port2 closed): Must disconnect now!
        ShadowLooper.idleMainLooper(25, TimeUnit.SECONDS);
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1).closeConnection();
        Assert.assertFalse(connection.isBound());
    }

    // Test that if a pending port is destroyed before authentication finishes,
    // when authentication succeeds with 0 ports, the idle disconnect countdown starts.
    @Test
    public void testPendingPortDestroyedBeforeAuthSuccessStartsIdleTimeout()
            throws RemoteException {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        NativeMessageAndroidPort port = new NativeMessageAndroidPort();

        // Add port before service connects (port is pending).
        Assert.assertNull(connection.addPort(EXT_1, true, port));
        var session = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session);

        // Port is destroyed while connection is pending.
        session.onPortDestroying(port);

        // Service connects and auth succeeds, but there are 0 ports.
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1, Mockito.never()).closeConnection();

        // Advance 60s idle timeout.
        ShadowLooper.idleMainLooper(60, TimeUnit.SECONDS);

        // Session should be disconnected.
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1).closeConnection();
        Assert.assertFalse(connection.isBound());
    }

    // Test that closing one port out of multiple does not trigger idle disconnect.
    @Test
    public void testMultiplePortsClosingDoesNotTriggerUntilLastPort() throws RemoteException {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port1));
        Assert.assertNull(connection.addPort(EXT_1, true, port2));
        RobolectricUtil.runAllBackgroundAndUi();

        var session = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session);

        // Close port1, but port2 is still active.
        session.onPortDestroying(port1);

        // Advance 100 seconds. Session should NOT disconnect because port2 is open.
        ShadowLooper.idleMainLooper(100, TimeUnit.SECONDS);
        Assert.assertNotNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1, Mockito.never()).closeConnection();

        // Close port2.
        session.onPortDestroying(port2);

        // Advance 60 seconds.
        ShadowLooper.idleMainLooper(60, TimeUnit.SECONDS);
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1).closeConnection();
    }

    // Test that idle disconnect for one extension does not affect other active extensions.
    @Test
    public void testMultiExtensionIdleDisconnectIsolation() throws RemoteException {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());

        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port1));
        Assert.assertNull(connection.addPort(EXT_2, false, port2));
        RobolectricUtil.runAllBackgroundAndUi();

        var session1 = connection.getSessionForTesting(EXT_1);
        var session2 = connection.getSessionForTesting(EXT_2);
        Assert.assertNotNull(session1);
        Assert.assertNotNull(session2);

        // Close EXT_1's port.
        session1.onPortDestroying(port1);

        // Advance 60 seconds.
        ShadowLooper.idleMainLooper(60, TimeUnit.SECONDS);

        // EXT_1 should be disconnected and its service closed.
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1).closeConnection();

        // EXT_2 should remain connected, and connection remains bound!
        Assert.assertNotNull(connection.getSessionForTesting(EXT_2));
        Mockito.verify(mMockExtensionService2, Mockito.never()).closeConnection();
        Assert.assertTrue(connection.isBound());
        Mockito.verify(mObserver, Mockito.never()).onUnbound(Mockito.any());
    }

    // Test that an extension can reconnect after its session was disconnected due to idle timeout.
    @Test
    public void testReconnectAfterIdleDisconnect() throws RemoteException {
        IExtensionNativeMessageService mockExtensionService1Reconnected =
                Mockito.mock(IExtensionNativeMessageService.class);
        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onSuccess(Mockito.mock(IExtensionNativeMessagePort.class));
                            return null;
                        })
                .when(mockExtensionService1Reconnected)
                .connectPort(Mockito.any(), Mockito.any());
        IBrowserNativeMessageService fakeBrowserService =
                new IBrowserNativeMessageService.Stub() {
                    private int mExt1ConnectCount;

                    @Override
                    public void connectExtension(
                            String extensionId, Bundle info, IConnectExtensionCallback callback)
                            throws RemoteException {
                        if (EXT_1.equals(extensionId)) {
                            mExt1ConnectCount++;
                            if (mExt1ConnectCount == 1) {
                                callback.onSuccess(mMockExtensionService1);
                            } else {
                                callback.onSuccess(mockExtensionService1Reconnected);
                            }
                            return;
                        }
                        if (EXT_2.equals(extensionId)) {
                            callback.onSuccess(mMockExtensionService2);
                            return;
                        }
                        callback.onError("Failed to connect extension");
                    }
                };

        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(fakeBrowserService.asBinder());

        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port1));
        Assert.assertNull(connection.addPort(EXT_2, false, port2));
        RobolectricUtil.runAllBackgroundAndUi();

        var session1 = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(session1);
        Assert.assertEquals(mMockExtensionService1, session1.getServiceForTesting());

        // EXT_1 closes port1 and reaches idle timeout.
        session1.onPortDestroying(port1);
        ShadowLooper.idleMainLooper(60, TimeUnit.SECONDS);

        // EXT_1 is disconnected, service closed, but connection stays bound because EXT_2 is
        // active.
        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mMockExtensionService1).closeConnection();
        Assert.assertTrue(connection.isBound());

        // EXT_1 reconnects with a new port.
        NativeMessageAndroidPort port3 = new NativeMessageAndroidPort();
        Assert.assertNull(connection.addPort(EXT_1, true, port3));
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify a new session was created and connected with the new service stub.
        var reconnectedSession1 = connection.getSessionForTesting(EXT_1);
        Assert.assertNotNull(reconnectedSession1);
        Assert.assertEquals(
                mockExtensionService1Reconnected, reconnectedSession1.getServiceForTesting());
        Assert.assertNotSame(session1, reconnectedSession1);
        Mockito.verify(mockExtensionService1Reconnected, Mockito.never()).closeConnection();

        // Now close the reconnected port and verify the new session has its own functional idle
        // timer.
        reconnectedSession1.onPortDestroying(port3);
        ShadowLooper.idleMainLooper(60, TimeUnit.SECONDS);

        Assert.assertNull(connection.getSessionForTesting(EXT_1));
        Mockito.verify(mockExtensionService1Reconnected).closeConnection();
    }

    // Test that onBindingDied tears down connection and logs DisconnectionReason.BINDING_DIED
    // and duration.
    @Test
    public void testOnBindingDiedTeardownAndHistogram() {
        NativeMessagingConnection connection =
                new NativeMessagingConnection(TARGET_PACKAGE, mObserver);
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        ShadowSystemClock.advanceBy(5000, TimeUnit.MILLISECONDS);

        var reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Extensions.NativeMessaging.Android.DisconnectionReason",
                        DisconnectionReason.BINDING_DIED);
        var durationWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Extensions.NativeMessaging.Android.UnexpectedDisconnectionDuration");

        connection.onBindingDied(null);

        Assert.assertFalse(connection.isBound());
        reasonWatcher.assertExpected();
        durationWatcher.assertExpected();
    }
}
