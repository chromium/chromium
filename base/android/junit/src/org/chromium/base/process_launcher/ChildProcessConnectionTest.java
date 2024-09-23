// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.or;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.BuildInfo;
import org.chromium.base.ChildBindingState;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;

/** Unit tests for ChildProcessConnection. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class ChildProcessConnectionTest {
    private static class ChildServiceConnectionMock implements ChildServiceConnection {
        private final Intent mBindIntent;
        private final ChildServiceConnectionDelegate mDelegate;
        private boolean mBound;
        private int mGroup;
        private int mImportanceInGroup;
        private boolean mBindResult = true;

        public ChildServiceConnectionMock(
                Intent bindIntent, ChildServiceConnectionDelegate delegate) {
            mBindIntent = bindIntent;
            mDelegate = delegate;
        }

        @Override
        public boolean bindServiceConnection() {
            mBound = mBindResult;
            return mBindResult;
        }

        @Override
        public void unbindServiceConnection() {
            mBound = false;
        }

        @Override
        public boolean isBound() {
            return mBound;
        }

        @Override
        public void updateGroupImportance(int group, int importanceInGroup) {
            mGroup = group;
            mImportanceInGroup = importanceInGroup;
        }

        @Override
        public void retire() {
            mBound = false;
        }

        public void setBindResult(boolean result) {
            mBindResult = result;
        }

        public void notifyServiceConnected(IBinder service) {
            mDelegate.onServiceConnected(service);
        }

        public void notifyServiceDisconnected() {
            mDelegate.onServiceDisconnected();
        }

        public Intent getBindIntent() {
            return mBindIntent;
        }

        public int getGroup() {
            return mGroup;
        }

        public int getImportanceInGroup() {
            return mImportanceInGroup;
        }
    }
    ;

    private final ChildServiceConnectionFactory mServiceConnectionFactory =
            new ChildServiceConnectionFactory() {
                @Override
                public ChildServiceConnection createConnection(
                        Intent bindIntent,
                        int bindFlags,
                        ChildServiceConnectionDelegate delegate,
                        String instanceName) {
                    ChildServiceConnectionMock connection =
                            spy(new ChildServiceConnectionMock(bindIntent, delegate));
                    if (mFirstServiceConnection == null) {
                        mFirstServiceConnection = connection;
                    }
                    mMockConnections.add(connection);
                    return connection;
                }
            };

    @Mock private ChildProcessConnection.ServiceCallback mServiceCallback;

    @Mock private ChildProcessConnection.ConnectionCallback mConnectionCallback;

    @Mock private ChildProcessConnection.ZygoteInfoCallback mZygoteInfoCallback;

    private IChildProcessService mIChildProcessService;

    private Binder mChildProcessServiceBinder;

    private ChildServiceConnectionMock mFirstServiceConnection;
    private final ArrayList<ChildServiceConnectionMock> mMockConnections = new ArrayList<>();

    // Parameters captured from the IChildProcessService.setupConnection() call
    private Bundle mConnectionBundle;
    private IParentProcess mConnectionParentProcess;

    @Before
    public void setUp() throws RemoteException {
        MockitoAnnotations.initMocks(this);

        mIChildProcessService = mock(IChildProcessService.class);
        ApplicationInfo appInfo = BuildInfo.getInstance().getBrowserApplicationInfo();
        when(mIChildProcessService.getAppInfo()).thenReturn(appInfo);
        // Capture the parameters passed to the IChildProcessService.setupConnection() call.
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                mConnectionBundle = (Bundle) invocation.getArgument(0);
                                mConnectionParentProcess =
                                        (IParentProcess) invocation.getArgument(1);
                                return null;
                            }
                        })
                .when(mIChildProcessService)
                .setupConnection(
                        or(isNull(), any(Bundle.class)),
                        or(isNull(), any()),
                        or(isNull(), any()),
                        or(isNull(), any()));

        mChildProcessServiceBinder = new Binder();
        mChildProcessServiceBinder.attachInterface(
                mIChildProcessService, IChildProcessService.class.getName());
    }

    private ChildProcessConnection createDefaultTestConnection() {
        return createTestConnection(
                /* bindToCaller= */ false,
                /* bindAsExternalService= */ false,
                /* serviceBundle= */ null,
                /* useFallback= */ false);
    }

    private ChildProcessConnection createTestConnection(
            boolean bindToCaller,
            boolean bindAsExternalService,
            Bundle serviceBundle,
            boolean useFallback) {
        String packageName = "org.chromium.test";
        String serviceName = "TestService";
        String fallbackServiceName = "TestFallbackService";
        return new ChildProcessConnection(
                /* context= */ null,
                new ComponentName(packageName, serviceName),
                useFallback ? new ComponentName(packageName, fallbackServiceName) : null,
                bindToCaller,
                bindAsExternalService,
                serviceBundle,
                mServiceConnectionFactory,
                /* instanceName= */ null);
    }

    private void sendPid(int pid) throws RemoteException {
        mConnectionParentProcess.finishSetupConnection(
                pid,
                /* zygotePid= */ 0,
                /* zygoteStartupTimeMillis= */ -1,
                /* relroBundle= */ null);
    }

    @Test
    public void testStrongBinding() {
        ChildProcessConnection connection = createDefaultTestConnection();
        connection.start(/* useStrongBinding= */ true, /* serviceCallback= */ null);
        assertTrue(connection.isStrongBindingBound());

        connection = createDefaultTestConnection();
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        assertFalse(connection.isStrongBindingBound());
    }

    @Test
    public void testServiceBundle() {
        Bundle serviceBundle = new Bundle();
        final String intKey = "org.chromium.myInt";
        final int intValue = 34;
        final int defaultValue = -1;
        serviceBundle.putInt(intKey, intValue);
        String stringKey = "org.chromium.myString";
        String stringValue = "thirty four";
        serviceBundle.putString(stringKey, stringValue);

        ChildProcessConnection connection =
                createTestConnection(
                        /* bindToCaller= */ false,
                        /* bindAsExternalService= */ false,
                        serviceBundle,
                        /* useFallback= */ false);
        // Start the connection without the ChildServiceConnection connecting.
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        assertNotNull(mFirstServiceConnection);
        Intent bindIntent = mFirstServiceConnection.getBindIntent();
        assertNotNull(bindIntent);
        assertEquals(intValue, bindIntent.getIntExtra(intKey, defaultValue));
        assertEquals(stringValue, bindIntent.getStringExtra(stringKey));
    }

    @Test
    public void testServiceStartsSuccessfully() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, mServiceCallback);
        Assert.assertTrue(connection.isVisibleBindingBound());
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());

        // The service connects.
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        Assert.assertTrue(connection.didOnServiceConnectedForTesting());
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());
    }

    @Test
    public void testServiceStartsAndFailsToBind() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        // Note we use doReturn so the actual bindServiceConnection() method is not called (it would
        // with when(mFirstServiceConnection.bindServiceConnection()).thenReturn(false).
        doReturn(false).when(mFirstServiceConnection).bindServiceConnection();
        connection.start(/* useStrongBinding= */ false, mServiceCallback);

        Assert.assertFalse(connection.isVisibleBindingBound());
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testServiceStops() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, mServiceCallback);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.stop();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testServiceDisconnects() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, mServiceCallback);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        mFirstServiceConnection.notifyServiceDisconnected();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testNotBoundToCaller() throws RemoteException {
        ChildProcessConnection connection =
                createTestConnection(
                        /* bindToCaller= */ false,
                        /* bindAsExternalService= */ false,
                        /* serviceBundle= */ null,
                        /* useFallback= */ false);
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, mServiceCallback);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        // Service is started and bindToCallback is not called.
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(connection);
        verify(mIChildProcessService, never()).bindToCaller(any());
    }

    @Test
    public void testBoundToCallerSuccess() throws RemoteException {
        ChildProcessConnection connection =
                createTestConnection(
                        /* bindToCaller= */ true,
                        /* bindAsExternalService= */ false,
                        /* serviceBundle= */ null,
                        /* useFallback= */ false);
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, mServiceCallback);
        when(mIChildProcessService.bindToCaller(any())).thenReturn(true);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        // Service is started and bindToCallback is called.
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(connection);
        verify(mIChildProcessService, times(1)).bindToCaller(any());
    }

    @Test
    public void testBoundToCallerFailure() throws RemoteException {
        ChildProcessConnection connection =
                createTestConnection(
                        /* bindToCaller= */ true,
                        /* bindAsExternalService= */ false,
                        /* serviceBundle= */ null,
                        /* useFallback= */ false);
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, mServiceCallback);
        // Pretend bindToCaller returns false, i.e. the service is already bound to a different
        // service.
        when(mIChildProcessService.bindToCaller(any())).thenReturn(false);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        // Service fails to start.
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, times(1)).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(connection);
        verify(mIChildProcessService, times(1)).bindToCaller(any());
    }

    @Test
    public void testSetupConnectionBeforeServiceConnected() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);
        verify(mConnectionCallback, never()).onConnected(any());
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        sendPid(34);
        verify(mConnectionCallback, times(1)).onConnected(connection);
    }

    @Test
    public void testSendPidOnlyWorksOnce() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);
        verify(mConnectionCallback, never()).onConnected(any());
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);

        sendPid(34);
        assertEquals(34, connection.getPid());
        sendPid(543);
        assertEquals(34, connection.getPid());
    }

    @Test
    public void testZygotePidSaved() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);
        verify(mConnectionCallback, never()).onConnected(any());
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);

        mConnectionParentProcess.finishSetupConnection(
                /* pid= */ 123,
                456
                /* zygotePid= */ ,
                /* zygoteStartupTimeMillis= */ 789,
                /* relroBundle= */ null);
        assertTrue(connection.hasUsableZygoteInfo());
        assertEquals(456, connection.getZygotePid());
    }

    @Test
    public void testTwoZygoteInfosOnlyOneValid() throws RemoteException {
        // Set up |connection1|.
        ChildProcessConnection connection1 = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection1.start(/* useStrongBinding= */ true, /* serviceCallback= */ null);
        connection1.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        assertNotNull(mFirstServiceConnection);
        mConnectionParentProcess.finishSetupConnection(
                /* pid= */ 125,
                /* zygotePid= */ 0,
                /* zygoteStartupTimeMillis= */ -1,
                /* relroBundle= */ null);

        // Allow the following setupConnection() to create a new service connection for
        // |connection2|.
        mFirstServiceConnection = null;

        // Set up |connection2|.
        ChildProcessConnection connection2 = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection2.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        connection2.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        assertNotNull(mFirstServiceConnection);

        mConnectionParentProcess.finishSetupConnection(
                126,
                /* zygotePid= */ 300,
                /* zygoteStartupTimeMillis= */ -1,
                /* relroBundle= */ null);
        assertTrue(connection2.hasUsableZygoteInfo());
        assertEquals(300, connection2.getZygotePid());
        assertFalse(connection1.hasUsableZygoteInfo());
    }

    @Test
    public void testInvokesZygoteCallback() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                mZygoteInfoCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);

        Bundle relroBundle = new Bundle();
        mConnectionParentProcess.finishSetupConnection(
                /* pid= */ 123,
                456
                /* zygotePid= */ ,
                /* zygoteStartupTimeMillis= */ 789,
                relroBundle);
        assertTrue(connection.hasUsableZygoteInfo());
        assertEquals(456, connection.getZygotePid());
        verify(mZygoteInfoCallback, times(1)).onReceivedZygoteInfo(connection, relroBundle);

        connection.consumeZygoteBundle(relroBundle);
        verify(mIChildProcessService, times(1)).consumeRelroBundle(relroBundle);
    }

    @Test
    public void testConsumeZygoteBundle() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                mZygoteInfoCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        Bundle relroBundle = new Bundle();
        mConnectionParentProcess.finishSetupConnection(
                /* pid= */ 123,
                456
                /* zygotePid= */ ,
                /* zygoteStartupTimeMillis= */ 789,
                relroBundle);

        verify(mIChildProcessService, never()).consumeRelroBundle(any());
        connection.consumeZygoteBundle(relroBundle);
        verify(mIChildProcessService, times(1)).consumeRelroBundle(relroBundle);
    }

    @Test
    public void testSetupConnectionAfterServiceConnected() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);
        verify(mConnectionCallback, never()).onConnected(any());
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        sendPid(34);
        verify(mConnectionCallback, times(1)).onConnected(connection);
    }

    @Test
    public void testKill() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);
        verify(mConnectionCallback, never()).onConnected(any());
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        sendPid(34);
        verify(mConnectionCallback, times(1)).onConnected(connection);

        // Add strong binding so that connection is oom protected.
        connection.removeVisibleBinding();
        assertEquals(ChildBindingState.WAIVED, connection.bindingStateCurrentOrWhenDied());
        if (ChildProcessConnection.supportNotPerceptibleBinding()) {
            connection.addNotPerceptibleBinding();
            assertEquals(
                    ChildBindingState.NOT_PERCEPTIBLE, connection.bindingStateCurrentOrWhenDied());
        }
        connection.addVisibleBinding();
        assertEquals(ChildBindingState.VISIBLE, connection.bindingStateCurrentOrWhenDied());
        connection.addStrongBinding();
        assertEquals(ChildBindingState.STRONG, connection.bindingStateCurrentOrWhenDied());

        // Kill and verify state.
        connection.kill();
        verify(mIChildProcessService).forceKill();
        assertEquals(ChildBindingState.STRONG, connection.bindingStateCurrentOrWhenDied());
        Assert.assertTrue(connection.isKilledByUs());
    }

    @Test
    public void testUpdateGroupImportanceSmoke() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        when(mIChildProcessService.bindToCaller(any())).thenReturn(true);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.updateGroupImportance(1, 2);
        assertEquals(1, connection.getGroup());
        assertEquals(2, connection.getImportanceInGroup());
        assertEquals(3, mMockConnections.size());
        // Group should be set on the wavied (last) binding.
        ChildServiceConnectionMock mock = mMockConnections.get(mMockConnections.size() - 1);
        assertEquals(1, mock.getGroup());
        assertEquals(2, mock.getImportanceInGroup());
    }

    @Test
    public void testExceptionDuringInit() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, /* serviceCallback= */ null);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);
        verify(mConnectionCallback, never()).onConnected(any());
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        sendPid(34);
        verify(mConnectionCallback, times(1)).onConnected(connection);

        String exceptionString = "test exception string";
        mConnectionParentProcess.reportExceptionInInit(exceptionString);
        ShadowLooper.runUiThreadTasks();
        Assert.assertEquals(exceptionString, connection.getExceptionDuringInit());
        Assert.assertFalse(mFirstServiceConnection.isBound());
    }

    @Test
    public void testFallback() throws RemoteException {
        Bundle serviceBundle = new Bundle();
        final String intKey = "org.chromium.myInt";
        final int intValue = 34;
        serviceBundle.putInt(intKey, intValue);

        ChildProcessConnection connection =
                createTestConnection(
                        /* bindToCaller= */ false,
                        /* bindAsExternalService= */ false,
                        serviceBundle,
                        /* useFallback= */ true);
        assertNotNull(mFirstServiceConnection);
        connection.start(/* useStrongBinding= */ false, mServiceCallback);

        Assert.assertEquals(3, mMockConnections.size());
        boolean anyServiceConnectionBound = false;
        for (ChildServiceConnectionMock serviceConnection : mMockConnections) {
            anyServiceConnectionBound = anyServiceConnectionBound || serviceConnection.isBound();
        }
        Assert.assertTrue(anyServiceConnectionBound);
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());
        {
            Intent bindIntent = mFirstServiceConnection.getBindIntent();
            assertNotNull(bindIntent);
            assertEquals(intValue, bindIntent.getIntExtra(intKey, -1));
            Assert.assertEquals("TestService", bindIntent.getComponent().getClassName());
        }

        connection.setupConnection(
                /* connectionBundle= */ null,
                /* callback= */ null,
                /* binderBox= */ null,
                mConnectionCallback,
                /* zygoteInfoCallback= */ null);

        // Do not call onServiceConnected. Simulate timeout with ShadowLooper.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());

        Assert.assertEquals(6, mMockConnections.size());
        // First 4 should be unbound now.
        for (int i = 0; i < 3; ++i) {
            verify(mMockConnections.get(i), times(1)).retire();
            Assert.assertFalse(mMockConnections.get(i).isBound());
        }
        // New connection for fallback service should be bound.
        ChildServiceConnectionMock boundServiceConnection = null;
        for (int i = 3; i < 6; ++i) {
            if (mMockConnections.get(i).isBound()) {
                boundServiceConnection = mMockConnections.get(i);
            }
            Intent bindIntent = mMockConnections.get(i).getBindIntent();
            assertNotNull(bindIntent);
            assertEquals(intValue, bindIntent.getIntExtra(intKey, -1));
            Assert.assertEquals("TestFallbackService", bindIntent.getComponent().getClassName());
        }
        Assert.assertNotNull(boundServiceConnection);

        // Complete connection.
        boundServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());
    }

    @Test
    public void testFallbackOnBindServiceFailure() throws RemoteException {
        Bundle serviceBundle = new Bundle();
        final String intKey = "org.chromium.myInt";
        final int intValue = 34;
        serviceBundle.putInt(intKey, intValue);

        ChildProcessConnection connection =
                createTestConnection(
                        /* bindToCaller= */ false,
                        /* bindAsExternalService= */ false,
                        serviceBundle,
                        /* useFallback= */ true);
        assertNotNull(mFirstServiceConnection);
        mFirstServiceConnection.setBindResult(false);

        connection.start(/* useStrongBinding= */ false, mServiceCallback);

        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());

        Assert.assertEquals(6, mMockConnections.size());
        // First 4 should be unbound now.
        for (int i = 0; i < 3; ++i) {
            verify(mMockConnections.get(i), times(1)).retire();
            Assert.assertFalse(mMockConnections.get(i).isBound());
        }
        // New connection for fallback service should be bound.
        ChildServiceConnectionMock boundServiceConnection = null;
        int boundConnectionCount = 0;
        for (int i = 3; i < 6; ++i) {
            if (mMockConnections.get(i).isBound()) {
                boundServiceConnection = mMockConnections.get(i);
                boundConnectionCount++;
            }
            Intent bindIntent = mMockConnections.get(i).getBindIntent();
            assertNotNull(bindIntent);
            assertEquals(intValue, bindIntent.getIntExtra(intKey, -1));
            Assert.assertEquals("TestFallbackService", bindIntent.getComponent().getClassName());
        }

        Assert.assertTrue(boundConnectionCount >= 2);
        Assert.assertTrue(connection.isVisibleBindingBound());

        // Complete connection.
        boundServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());
    }
}
