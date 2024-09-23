// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gcore;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.TimeUnit;

/** Tests for {@link GoogleApiClientHelper} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleApiClientHelperTest {
    private GoogleApiClient mMockClient;

    @Before
    public void setUp() {
        LifecycleHook.destroyInstanceForJUnitTests();
        mMockClient = mock(GoogleApiClient.class);
    }

    @After
    public void tearDown() {
        LifecycleHook.destroyInstanceForJUnitTests();
    }

    /** Tests that connection attempts are delayed. */
    @Test
    @Feature({"GCore"})
    // TODO(crbug.com/40182398): Change to use paused loop. See crbug for details.
    @LooperMode(LooperMode.Mode.LEGACY)
    public void connectionAttemptDelayTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);

        ShadowLooper.pauseMainLooper();
        helper.onConnectionFailed(new ConnectionResult(ConnectionResult.SERVICE_UPDATING));
        verify(mMockClient, times(0)).connect();
        ShadowLooper.unPauseMainLooper();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).connect();
    }

    /** Tests that the connection handler gives up after a number of connection attempts. */
    @Test
    @Feature({"GCore"})
    public void connectionFailureTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);

        helper.onConnectionFailed(new ConnectionResult(ConnectionResult.DEVELOPER_ERROR));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Should not retry on unrecoverable errors
        verify(mMockClient, never()).connect();

        // Connection attempts
        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT; i++) {
            helper.onConnectionFailed(new ConnectionResult(ConnectionResult.SERVICE_UPDATING));
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        }

        // Should have tried to connect every time.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT)).connect();

        // Try again
        helper.onConnectionFailed(new ConnectionResult(ConnectionResult.SERVICE_UPDATING));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // The connection handler should have given up, no new call.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT)).connect();
    }

    /** Tests that when a connection succeeds, the retry limit is reset. */
    @Test
    @Feature({"GCore"})
    public void connectionAttemptsResetTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);

        // Connection attempts
        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT - 1; i++) {
            helper.onConnectionFailed(new ConnectionResult(ConnectionResult.SERVICE_UPDATING));
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        }

        // Should have tried to connect every time.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT - 1)).connect();

        // Connection successful now
        helper.onConnected(null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT; i++) {
            helper.onConnectionFailed(new ConnectionResult(ConnectionResult.SERVICE_UPDATING));
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        }

        // A success should allow for more connection attempts.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT * 2 - 1)).connect();

        // This should not result in a connection attempt, the limit is still there.
        helper.onConnectionFailed(new ConnectionResult(ConnectionResult.SERVICE_UPDATING));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // The connection handler should have given up, no new call.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT * 2 - 1)).connect();
    }

    @Test
    @Feature({"GCore"})
    public void lifecycleManagementTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Activity mockActivity = mock(Activity.class);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);

        // The helper should have been registered to handle connectivity issues.
        verify(mMockClient).registerConnectionCallbacks(helper);
        verify(mMockClient).registerConnectionFailedListener(helper);

        // Client was not connected. Coming in the foreground should not change that.
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        verify(mMockClient, never()).connect();

        // We now say we are connected
        when(mMockClient.isConnected()).thenReturn(true);

        // Should be disconnected when we go in the background
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).disconnect();

        // Should be reconnected when we come in the foreground
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        verify(mMockClient).connect();

        helper.disable();

        // The helper should have been unregistered from handling connectivity issues.
        verify(mMockClient).unregisterConnectionCallbacks(helper);
        verify(mMockClient).unregisterConnectionFailedListener(helper);

        // Should not be interacted with anymore when we stop managing it.
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        verify(mMockClient).disconnect();
    }

    @Test
    @Feature({"GCore"})
    public void lifecycleManagementDelayTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Activity mockActivity = mock(Activity.class);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);
        helper.setDisconnectionDelay(5000);

        // We have a connected client
        when(mMockClient.isConnected()).thenReturn(true);

        // Should not be disconnected when we go in the background
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        ShadowLooper.runUiThreadTasks();
        verify(mMockClient, times(0)).disconnect();

        // Should be disconnected when we wait.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).disconnect();

        // Should be reconnected when we come in the foreground
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        verify(mMockClient).connect();

        // Should not disconnect when we became visible during the delay
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        ShadowLooper.runUiThreadTasks();
        verify(mMockClient, times(1)).disconnect();
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).disconnect();
    }

    @Test
    @Feature({"GCore"})
    public void disconnectionCancellingTest() {
        int disconnectionTimeout = 5000;
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Activity mockActivity = mock(Activity.class);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);
        helper.setDisconnectionDelay(disconnectionTimeout);

        // We have a connected client
        when(mMockClient.isConnected()).thenReturn(true);

        // We go in the background and come back before the end of the timeout.
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        ShadowLooper.idleMainLooper(disconnectionTimeout - 42, TimeUnit.MILLISECONDS);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // The client should not have been disconnected, which would drop requests otherwise.
        verify(mMockClient, never()).disconnect();
    }

    @Test
    @Feature({"GCore"})
    public void willUseConnectionBackgroundTest() {
        int disconnectionTimeout = 5000;
        int arbitraryNumberOfSeconds = 42;
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Activity mockActivity = mock(Activity.class);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);
        helper.setDisconnectionDelay(disconnectionTimeout);

        // We have a connected client
        when(mMockClient.isConnected()).thenReturn(true);

        // We go in the background and extend the delay
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        ShadowLooper.idleMainLooper(
                disconnectionTimeout - arbitraryNumberOfSeconds, TimeUnit.MILLISECONDS);
        helper.willUseConnection();

        // The client should not have been disconnected.
        ShadowLooper.idleMainLooper(
                disconnectionTimeout - arbitraryNumberOfSeconds, TimeUnit.MILLISECONDS);
        verify(mMockClient, never()).disconnect();

        // After the full timeout it should still disconnect though
        ShadowLooper.idleMainLooper(arbitraryNumberOfSeconds, TimeUnit.MILLISECONDS);
        verify(mMockClient).disconnect();

        // The client is now disconnected then
        when(mMockClient.isConnected()).thenReturn(false);

        // The call should reconnect a disconnected client
        helper.willUseConnection();
        verify(mMockClient).connect();
    }

    @Test
    @Feature({"GCore"})
    public void willUseConnectionForegroundTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Activity mockActivity = mock(Activity.class);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);
        helper.setDisconnectionDelay(5000);

        // We have a connected client
        when(mMockClient.isConnected()).thenReturn(true);

        // We are in the foreground
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Disconnections should not be scheduled when in the foreground.
        helper.willUseConnection();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, never()).disconnect();
    }
}
