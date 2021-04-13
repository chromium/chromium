// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.or;
import static org.mockito.Mockito.any;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ChildBindingState;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;

/** Unit tests for ChildProcessConnection. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChildProcessConnectionTest {
    private static class ChildServiceConnectionMock implements ChildServiceConnection {
        private final Intent mBindIntent;
        private final ChildServiceConnectionDelegate mDelegate;
        private boolean mBound;
        private int mGroup;
        private int mImportanceInGroup;

        public ChildServiceConnectionMock(
                Intent bindIntent, ChildServiceConnectionDelegate delegate) {
            mBindIntent = bindIntent;
            mDelegate = delegate;
        }

        @Override
        public boolean bindServiceConnection() {
            mBound = true;
            return true;
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
    };

    private final ChildServiceConnectionFactory mServiceConnectionFactory =
            new ChildServiceConnectionFactory() {
                @Override
                public ChildServiceConnection createConnection(Intent bindIntent, int bindFlags,
                        ChildServiceConnectionDelegate delegate, String instanceName) {
                    ChildServiceConnectionMock connection =
                            spy(new ChildServiceConnectionMock(bindIntent, delegate));
                    if (mFirstServiceConnection == null) {
                        mFirstServiceConnection = connection;
                    }
                    mMockConnections.add(connection);
                    return connection;
                }
            };

    @Mock
    private ChildProcessConnection.ServiceCallback mServiceCallback;

    @Mock
    private ChildProcessConnection.ConnectionCallback mConnectionCallback;

    private IChildProcessService mIChildProcessService;

    private Binder mChildProcessServiceBinder;

    private ChildServiceConnectionMock mFirstServiceConnection;
    private final ArrayList<ChildServiceConnectionMock> mMockConnections = new ArrayList<>();

    // Parameters captured from the IChildProcessService.setupConnection() call
    private Bundle mConnectionBundle;
    private IParentProcess mConnectionParentProcess;
    private IBinder mConnectionIBinderCallback;

    @Before
    public void setUp() throws RemoteException {
        MockitoAnnotations.initMocks(this);

        mIChildProcessService = mock(IChildProcessService.class);
        // Capture the parameters passed to the IChildProcessService.setupConnection() call.
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mConnectionBundle = (Bundle) invocation.getArgument(0);
                mConnectionParentProcess = (IParentProcess) invocation.getArgument(1);
                mConnectionIBinderCallback = (IBinder) invocation.getArgument(2);
                return null;
            }
        })
                .when(mIChildProcessService)
                .setupConnection(
                        or(isNull(), any(Bundle.class)), or(isNull(), any()), or(isNull(), any()));

        mChildProcessServiceBinder = new Binder();
        mChildProcessServiceBinder.attachInterface(
                mIChildProcessService, IChildProcessService.class.getName());
    }

    private ChildProcessConnection createDefaultTestConnection() {
        return createTestConnection(false /* bindToCaller */, false /* bindAsExternalService */,
                null /* serviceBundle */, false /* useFallback */);
    }

    private ChildProcessConnection createTestConnection(boolean bindToCaller,
            boolean bindAsExternalService, Bundle serviceBundle, boolean useFallback) {
        String packageName = "org.chromium.test";
        String serviceName = "TestService";
        String fallbackServiceName = "TestFallbackService";
        return new ChildProcessConnection(null /* context */,
                new ComponentName(packageName, serviceName),
                useFallback ? new ComponentName(packageName, fallbackServiceName) : null,
                bindToCaller, bindAsExternalService, serviceBundle, mServiceConnectionFactory,
                null /* instanceName */);
    }

    @Test
    public void testStrongBinding() {
        ChildProcessConnection connection = createDefaultTestConnection();
        connection.start(true /* useStrongBinding */, null /* serviceCallback */);
        assertTrue(connection.isStrongBindingBound());

        connection = createDefaultTestConnection();
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
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

        ChildProcessConnection connection = createTestConnection(false /* bindToCaller */,
                false /* bindAsExternalService */, serviceBundle, false /* useFallback */);
        // Start the connection without the ChildServiceConnection connecting.
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
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
        connection.start(false /* useStrongBinding */, mServiceCallback);
        Assert.assertTrue(connection.isModerateBindingBound());
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());

        // The service connects.
        mFirstServiceConnection.notifyServiceConnected(null /* iBinder */);
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
        connection.start(false /* useStrongBinding */, mServiceCallback);

        Assert.assertFalse(connection.isModerateBindingBound());
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testServiceStops() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
        mFirstServiceConnection.notifyServiceConnected(null /* iBinder */);
        connection.stop();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testServiceDisconnects() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
        mFirstServiceConnection.notifyServiceConnected(null /* iBinder */);
        mFirstServiceConnection.notifyServiceDisconnected();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testNotBoundToCaller() throws RemoteException {
        ChildProcessConnection connection =
                createTestConnection(false /* bindToCaller */, false /* bindAsExternalService */,
                        null /* serviceBundle */, false /* useFallback */);
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
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
                createTestConnection(true /* bindToCaller */, false /* bindAsExternalService */,
                        null /* serviceBundle */, false /* useFallback */);
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
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
                createTestConnection(true /* bindToCaller */, false /* bindAsExternalService */,
                        null /* serviceBundle */, false /* useFallback */);
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
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
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        connection.setupConnection(
                null /* connectionBundle */, null /* callback */, mConnectionCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        mConnectionParentProcess.sendPid(34);
        verify(mConnectionCallback, times(1)).onConnected(connection);
    }

    @Test
    public void testSendPidOnlyWorksOnce() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        connection.setupConnection(
                null /* connectionBundle */, null /* callback */, mConnectionCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);

        mConnectionParentProcess.sendPid(34);
        assertEquals(34, connection.getPid());
        mConnectionParentProcess.sendPid(543);
        assertEquals(34, connection.getPid());
    }

    @Test
    public void testSetupConnectionAfterServiceConnected() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.setupConnection(
                null /* connectionBundle */, null /* callback */, mConnectionCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        mConnectionParentProcess.sendPid(34);
        verify(mConnectionCallback, times(1)).onConnected(connection);
    }

    @Test
    public void testKill() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.setupConnection(
                null /* connectionBundle */, null /* callback */, mConnectionCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        mConnectionParentProcess.sendPid(34);
        verify(mConnectionCallback, times(1)).onConnected(connection);

        // Add strong binding so that connection is oom protected.
        connection.removeModerateBinding(false);
        assertEquals(ChildBindingState.WAIVED, connection.bindingStateCurrentOrWhenDied());
        connection.addModerateBinding(false);
        assertEquals(ChildBindingState.MODERATE, connection.bindingStateCurrentOrWhenDied());
        connection.addStrongBinding();
        assertEquals(ChildBindingState.STRONG, connection.bindingStateCurrentOrWhenDied());

        // Kill and verify state.
        connection.kill();
        verify(mIChildProcessService).forceKill();
        assertEquals(ChildBindingState.STRONG, connection.bindingStateCurrentOrWhenDied());
        Assert.assertTrue(connection.isKilledByUs());
    }

    @Test
    public void testBindingStateCounts() {
        ChildProcessConnection.resetBindingStateCountsForTesting();
        ChildProcessConnection connection0 = createDefaultTestConnection();
        ChildServiceConnectionMock connectionMock0 = mFirstServiceConnection;
        mFirstServiceConnection = null;
        ChildProcessConnection connection1 = createDefaultTestConnection();
        ChildServiceConnectionMock connectionMock1 = mFirstServiceConnection;
        mFirstServiceConnection = null;
        ChildProcessConnection connection2 = createDefaultTestConnection();
        ChildServiceConnectionMock connectionMock2 = mFirstServiceConnection;
        mFirstServiceConnection = null;

        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 0, 0});

        connection0.start(false /* useStrongBinding */, null /* serviceCallback */);
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 0, 0});

        connection1.start(true /* useStrongBinding */, null /* serviceCallback */);
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 0, 1});

        connection2.start(false /* useStrongBinding */, null /* serviceCallback */);
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 1, 1});

        Binder binder0 = new Binder();
        Binder binder1 = new Binder();
        Binder binder2 = new Binder();
        binder0.attachInterface(mIChildProcessService, IChildProcessService.class.getName());
        binder1.attachInterface(mIChildProcessService, IChildProcessService.class.getName());
        binder2.attachInterface(mIChildProcessService, IChildProcessService.class.getName());
        connectionMock0.notifyServiceConnected(binder0);
        connectionMock1.notifyServiceConnected(binder1);
        connectionMock2.notifyServiceConnected(binder2);
        ShadowLooper.runUiThreadTasks();

        // Add and remove moderate binding works as expected.
        connection2.removeModerateBinding(false);
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 1, 0, 1});
        connection2.addModerateBinding(false);
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 1, 1});

        // Add and remove strong binding works as expected.
        connection0.addStrongBinding();
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 1, 1});
        connection0.removeStrongBinding();
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 1, 1});

        // Stopped connection should no longe update.
        connection0.stop();
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 1, 1});
        assertArrayEquals(
                connection1.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 1, 0});

        connection2.removeModerateBinding(false);
        assertArrayEquals(
                connection0.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 0, 1, 1});
        assertArrayEquals(
                connection1.remainingBindingStateCountsCurrentOrWhenDied(), new int[] {0, 1, 0, 0});
    }

    @Test
    public void testUpdateGroupImportanceSmoke() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        when(mIChildProcessService.bindToCaller(any())).thenReturn(true);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.updateGroupImportance(1, 2);
        assertEquals(1, connection.getGroup());
        assertEquals(2, connection.getImportanceInGroup());
        assertEquals(4, mMockConnections.size());
        // Group should be set on the wavied (last) binding.
        ChildServiceConnectionMock mock = mMockConnections.get(mMockConnections.size() - 1);
        assertEquals(1, mock.getGroup());
        assertEquals(2, mock.getImportanceInGroup());
    }

    @Test
    public void testExceptionDuringInit() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.setupConnection(
                null /* connectionBundle */, null /* callback */, mConnectionCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionParentProcess);
        mConnectionParentProcess.sendPid(34);
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

        ChildProcessConnection connection = createTestConnection(false /* bindToCaller */,
                false /* bindAsExternalService */, serviceBundle, true /* useFallback */);
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);

        Assert.assertEquals(4, mMockConnections.size());
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
                null /* connectionBundle */, null /* callback */, mConnectionCallback);

        // Do not call onServiceConnected. Simulate timeout with ShadowLooper.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());

        Assert.assertEquals(8, mMockConnections.size());
        // First 4 should be unbound now.
        for (int i = 0; i < 4; ++i) {
            verify(mMockConnections.get(i), times(1)).retire();
            Assert.assertFalse(mMockConnections.get(i).isBound());
        }
        // New connection for fallback service should be bound.
        ChildServiceConnectionMock boundServiceConnection = null;
        for (int i = 4; i < 8; ++i) {
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
    public void testModerateWaiveCpuPriority() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        when(mIChildProcessService.bindToCaller(any())).thenReturn(true);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ChildServiceConnectionMock moderateConnection = mMockConnections.get(0);
        ChildServiceConnectionMock moderateWaiveCpuConnection = mMockConnections.get(1);

        Assert.assertTrue(connection.isModerateBindingBound());
        assertTrue(moderateConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.removeModerateBinding(false /* waiveCpuPriority */);
        Assert.assertFalse(connection.isModerateBindingBound());
        assertFalse(moderateConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.addModerateBinding(true /* waiveCpuPriority */);
        Assert.assertTrue(connection.isModerateBindingBound());
        assertFalse(moderateConnection.isBound());
        assertTrue(moderateWaiveCpuConnection.isBound());

        connection.removeModerateBinding(true /* waiveCpuPriority */);
        Assert.assertFalse(connection.isModerateBindingBound());
        assertFalse(moderateConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());
    }

    @Test
    public void testModerateWaiveCpuPriorityMixedWithModerate() throws RemoteException {
        // Test that the waive cpu connection should not be bound if the strong or moderate
        // bindings are bound.
        ChildProcessConnection connection = createDefaultTestConnection();
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        when(mIChildProcessService.bindToCaller(any())).thenReturn(true);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ChildServiceConnectionMock moderateConnection = mMockConnections.get(0);
        ChildServiceConnectionMock moderateWaiveCpuConnection = mMockConnections.get(1);

        Assert.assertTrue(connection.isModerateBindingBound());
        assertTrue(moderateConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.addModerateBinding(true /* waiveCpuPriority */);
        assertTrue(moderateConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.removeModerateBinding(false /* waiveCpuPriority */);
        assertFalse(moderateConnection.isBound());
        assertTrue(moderateWaiveCpuConnection.isBound());

        connection.addModerateBinding(false /* waiveCpuPriority */);
        assertTrue(moderateConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.removeModerateBinding(false /* waiveCpuPriority */);
        assertFalse(moderateConnection.isBound());
        assertTrue(moderateWaiveCpuConnection.isBound());

        connection.removeModerateBinding(true /* waiveCpuPriority */);
        assertFalse(moderateConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.addModerateBinding(true /* waiveCpuPriority */);
        assertFalse(moderateConnection.isBound());
        assertTrue(moderateWaiveCpuConnection.isBound());
    }

    @Test
    public void testModerateWaiveCpuPriorityMixedWithStrong() throws RemoteException {
        // Test that the waive cpu connection should not be bound if the strong or moderate
        // bindings are bound.
        ChildProcessConnection connection = createDefaultTestConnection();
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        when(mIChildProcessService.bindToCaller(any())).thenReturn(true);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ChildServiceConnectionMock moderateConnection = mMockConnections.get(0);
        ChildServiceConnectionMock moderateWaiveCpuConnection = mMockConnections.get(1);
        ChildServiceConnectionMock strongConnection = mMockConnections.get(2);

        connection.removeModerateBinding(false /* waiveCpuPriority */);
        assertFalse(moderateConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.addStrongBinding();
        assertTrue(strongConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.addModerateBinding(true /* waiveCpuPriority */);
        assertTrue(strongConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.removeStrongBinding();
        assertFalse(strongConnection.isBound());
        assertTrue(moderateWaiveCpuConnection.isBound());

        connection.addStrongBinding();
        assertTrue(strongConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.removeStrongBinding();
        assertFalse(strongConnection.isBound());
        assertTrue(moderateWaiveCpuConnection.isBound());

        connection.removeModerateBinding(true /* waiveCpuPriority */);
        assertFalse(strongConnection.isBound());
        assertFalse(moderateWaiveCpuConnection.isBound());

        connection.addModerateBinding(true /* waiveCpuPriority */);
        assertFalse(strongConnection.isBound());
        assertTrue(moderateWaiveCpuConnection.isBound());
    }
}
