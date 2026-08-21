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
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.Nullable;

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
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link NativeMessageAndroidPort}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativeMessageAndroidPortTest {
    private static final String TARGET_PACKAGE = "com.example.extensionreceiver";
    private static final String EXTENSION_ID = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private IExtensionNativeMessageService mMockExtensionService;

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
    }

    private static class FakeNativeMessagePort extends IExtensionNativeMessagePort.Stub {
        public final List<String> receivedMessages = new ArrayList<>();
        public final @Nullable IExtensionNativeMessageCallback callback;
        public boolean isDisconnected;
        public boolean shouldThrowOnPostMessage;
        public int failAfterMessageCount = -1;

        public FakeNativeMessagePort(@Nullable IExtensionNativeMessageCallback callback) {
            this.callback = callback;
        }

        @Override
        public void postMessage(String message) throws RemoteException {
            if (shouldThrowOnPostMessage || receivedMessages.size() == failAfterMessageCount) {
                throw new DeadObjectException("Target process is dead.");
            }
            receivedMessages.add(message);
        }

        @Override
        public void disconnect() {
            isDisconnected = true;
        }
    }

    private static class TestPortObserver
            implements NativeMessageAndroidPort.Observer, NativeMessageAndroidPort.TestObserver {
        public final List<String> receivedMessages = new ArrayList<>();
        public @Nullable String closedError;
        public boolean isPortDestroyed;

        @Override
        public void onPortDestroying(NativeMessageAndroidPort port) {
            isPortDestroyed = true;
        }

        @Override
        public void onMessageFromApp(String message) {
            receivedMessages.add(message);
        }

        @Override
        public void onChannelClosed(String errorMessage) {
            closedError = errorMessage;
        }
    }

    @Before
    public void setUp() {
        Mockito.when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mTestContext = new TestContext(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mTestContext);

        mFakeBrowserService =
                new IBrowserNativeMessageService.Stub() {
                    @Override
                    public IExtensionNativeMessageService connectExtension(
                            String extensionId, Bundle info) throws RemoteException {
                        if (EXTENSION_ID.equals(extensionId)) {
                            return mMockExtensionService;
                        }
                        return null;
                    }
                };
    }

    @After
    public void tearDown() {
        NativeMessagingManager.getForProfile(mProfile).destroy();
    }

    private @Nullable String connectToApp(NativeMessageAndroidPort port) {
        return port.connectToApp(
                mProfile, TARGET_PACKAGE, EXTENSION_ID, /* isVerifiedExtension= */ true);
    }

    // Test that messages queued in NativeMessageAndroidPort before the app connects
    // are flushed in FIFO order once connection is established, and bidirectional
    // replies from the app are delivered.
    @Test
    public void testConnectToAppAndAddPort() throws Exception {
        List<FakeNativeMessagePort> createdPorts = new ArrayList<>();
        Mockito.when(mMockExtensionService.connectPort(Mockito.any()))
                .thenAnswer(
                        invocation -> {
                            FakeNativeMessagePort fakePort =
                                    new FakeNativeMessagePort(invocation.getArgument(0));
                            createdPorts.add(fakePort);
                            return fakePort;
                        });

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        // 1. Queue messages before service is connected.
        port.forwardMessageToApp("msg_1");
        port.forwardMessageToApp("msg_2");

        Assert.assertNull(connectToApp(port));

        // 2. Service connects and completes authentication.
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // 3. Verify port connected and flushed pending messages in FIFO order.
        Assert.assertEquals(1, createdPorts.size());
        Assert.assertEquals(List.of("msg_1", "msg_2"), createdPorts.get(0).receivedMessages);

        // 4. Test bidirectional reply from the app.
        createdPorts.get(0).callback.onMessage("reply_from_app");
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(List.of("reply_from_app"), portObserver.receivedMessages);
    }

    // Test adding a second port to an already-connected extension session, verifying
    // it connects immediately without re-authenticating and maintains isolated messaging.
    @Test
    public void testAddPortToConnectedApp() throws Exception {
        List<FakeNativeMessagePort> createdPorts = new ArrayList<>();
        Mockito.when(mMockExtensionService.connectPort(Mockito.any()))
                .thenAnswer(
                        invocation -> {
                            FakeNativeMessagePort fakePort =
                                    new FakeNativeMessagePort(invocation.getArgument(0));
                            createdPorts.add(fakePort);
                            return fakePort;
                        });

        // 1. Establish session first with port 1.
        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        Assert.assertNull(connectToApp(port1));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, createdPorts.size());

        // 2. Add port 2 to the already-connected session.
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        TestPortObserver port2Observer = new TestPortObserver();
        port2.setTestObserver(port2Observer);

        Assert.assertNull(connectToApp(port2));
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(2, createdPorts.size());

        // 3. Port 2 sends a message directly.
        port2.forwardMessageToApp("msg_port2");
        Assert.assertEquals(List.of("msg_port2"), createdPorts.get(1).receivedMessages);
        Assert.assertTrue(createdPorts.get(0).receivedMessages.isEmpty());

        // 4. App replies specifically to port 2.
        createdPorts.get(1).callback.onMessage("reply_to_port2");
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(List.of("reply_to_port2"), port2Observer.receivedMessages);
    }

    // Test that if the target app returns null from connectPort, the port is removed
    // from active ports and closed with an error.
    @Test
    public void testAppRejectsPortCreation() throws Exception {
        Mockito.when(mMockExtensionService.connectPort(Mockito.any())).thenReturn(null);

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(
                "Could not connect port to " + TARGET_PACKAGE + ".", portObserver.closedError);
    }

    // Test that if a port is destroyed while connectPortInBackground is in flight,
    // onConnectPortResult detects the cancellation and immediately disconnects the remote stub.
    @Test
    public void testPortDestroyedWhileConnectingCancelsRemotePort() throws Exception {
        FakeNativeMessagePort fakeAppPort = new FakeNativeMessagePort(null);
        Mockito.when(mMockExtensionService.connectPort(Mockito.any())).thenReturn(fakeAppPort);

        // 1. Establish and authenticate session first.
        NativeMessageAndroidPort setupPort = new NativeMessageAndroidPort();
        Assert.assertNull(connectToApp(setupPort));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // 2. Add new port to the connected session (initiates connectPortInBackground).
        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        Assert.assertNull(connectToApp(port));

        // 3. Destroy port while connectPortInBackground is in-flight before tasks run.
        port.destroy();

        RobolectricUtil.runAllBackgroundAndUi();

        // 4. The remote stub should have been told to disconnect immediately upon connect.
        Assert.assertTrue(fakeAppPort.isDisconnected);
    }

    // Test that when the companion app disconnects a port via callback.onDisconnect(),
    // the channel is closed, the port is notified, and no redundant disconnect()
    // is sent to the app.
    @Test
    public void testAppDisconnectsPort() throws Exception {
        List<FakeNativeMessagePort> createdPorts = new ArrayList<>();
        Mockito.when(mMockExtensionService.connectPort(Mockito.any()))
                .thenAnswer(
                        invocation -> {
                            FakeNativeMessagePort fakePort =
                                    new FakeNativeMessagePort(invocation.getArgument(0));
                            createdPorts.add(fakePort);
                            return fakePort;
                        });

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, createdPorts.size());
        FakeNativeMessagePort fakeAppPort = createdPorts.get(0);

        // App disconnects the port.
        fakeAppPort.callback.onDisconnect();
        RobolectricUtil.runAllBackgroundAndUi();

        // Channel should be closed with empty error string (app-initiated disconnect).
        Assert.assertEquals("", portObserver.closedError);
        // Ensure Chrome did not ping the app back with disconnect().
        Assert.assertFalse(fakeAppPort.isDisconnected);
    }

    // Test that when Chrome/browser destroys an active port (e.g. extension closes port),
    // it sends a disconnect() signal to the companion app and cleans up state.
    @Test
    public void testBrowserDisconnectsPort() throws Exception {
        List<FakeNativeMessagePort> createdPorts = new ArrayList<>();
        Mockito.when(mMockExtensionService.connectPort(Mockito.any()))
                .thenAnswer(
                        invocation -> {
                            FakeNativeMessagePort fakePort =
                                    new FakeNativeMessagePort(invocation.getArgument(0));
                            createdPorts.add(fakePort);
                            return fakePort;
                        });

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, createdPorts.size());
        FakeNativeMessagePort fakeAppPort = createdPorts.get(0);

        // Browser destroys the port.
        port.destroy();

        // Verify the app's remote port received disconnect().
        Assert.assertTrue(fakeAppPort.isDisconnected);
    }

    // Test that if posting a message to the external app fails with a RemoteException
    // (e.g. app process died), the port immediately closes the channel with an error.
    @Test
    public void testPostMessageRemoteExceptionClosesChannel() throws Exception {
        FakeNativeMessagePort fakeAppPort = new FakeNativeMessagePort(null);
        Mockito.when(mMockExtensionService.connectPort(Mockito.any())).thenReturn(fakeAppPort);

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // Configure fake app port to throw DeadObjectException when postMessage is called.
        fakeAppPort.shouldThrowOnPostMessage = true;

        // Sending a message fails and should trigger channel closure.
        port.forwardMessageToApp("msg_fail");
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(
                "Error when communicating with the native messaging host.",
                portObserver.closedError);
    }

    // Test that if sending queued pending messages fails mid-flush (e.g. on message #2),
    // previous messages are delivered, the failure closes the channel, and remaining
    // pending messages are aborted.
    @Test
    public void testFlushPendingMessagesFails() throws Exception {
        FakeNativeMessagePort fakeAppPort = new FakeNativeMessagePort(null);
        fakeAppPort.failAfterMessageCount = 1;
        Mockito.when(mMockExtensionService.connectPort(Mockito.any())).thenReturn(fakeAppPort);

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        // Queue multiple messages while waiting for connection.
        port.forwardMessageToApp("pending_1");
        port.forwardMessageToApp("pending_2");
        port.forwardMessageToApp("pending_3");

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify only message 1 was received, and message 3 was aborted after message 2 failed.
        Assert.assertEquals(List.of("pending_1"), fakeAppPort.receivedMessages);
        Assert.assertEquals(
                "Error when communicating with the native messaging host.",
                portObserver.closedError);
    }

    // Test that if the external app synchronously calls callback.onMessage during connectPort
    // (before returning the remote port), the message is delivered to the port without being
    // dropped.
    @Test
    public void testAppSendsMessageSynchronouslyDuringConnectPort() throws Exception {
        Mockito.when(mMockExtensionService.connectPort(Mockito.any()))
                .thenAnswer(
                        invocation -> {
                            IExtensionNativeMessageCallback callback = invocation.getArgument(0);
                            callback.onMessage("synchronous_msg");
                            return new FakeNativeMessagePort(callback);
                        });

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(List.of("synchronous_msg"), portObserver.receivedMessages);
    }
}
