// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.content.Intent;
import android.content.ServiceConnection;

import androidx.core.app.ServiceCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

/** Unit tests for {@link ActorForegroundServiceControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorForegroundServiceControllerImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorForegroundServiceImpl mServiceImpl;
    @Mock private ActorForegroundServiceImpl.LocalBinder mBinder;
    @Mock private Notification mNotification;

    private ActorForegroundServiceControllerImpl mController;
    private ShadowApplication mShadowApplication;

    @Before
    public void setUp() {
        mController = new ActorForegroundServiceControllerImpl();
        mShadowApplication = shadowOf(RuntimeEnvironment.getApplication());
        when(mBinder.getService()).thenReturn(mServiceImpl);
    }

    @Test
    public void testStartAndBindService() throws Exception {
        CallbackHelper connectedCallback = new CallbackHelper();
        mController.startAndBindService(connectedCallback::notifyCalled);

        // Verify service was started
        Intent startedIntent = mShadowApplication.getNextStartedService();
        assertEquals(
                "Service class name should match.",
                ActorForegroundService.class.getName(),
                startedIntent.getComponent().getClassName());

        // Simulate service connection
        ServiceConnection connection = mController.getServiceConnectionForTesting();
        connection.onServiceConnected(null, mBinder);
        connectedCallback.waitForOnly();
        assertTrue(
                "Controller should be connected after onServiceConnected.",
                mController.isConnected());
    }

    @Test
    public void testOnServiceDisconnected() throws Exception {
        mController.startAndBindService(() -> {});
        ServiceConnection connection = mController.getServiceConnectionForTesting();
        connection.onServiceConnected(null, mBinder);
        assertTrue("Controller should be connected.", mController.isConnected());

        connection.onServiceDisconnected(null);
        assertFalse("Controller should be disconnected.", mController.isConnected());
    }

    @Test
    public void testProxyMethods() {
        mController.startAndBindService(() -> {});
        mController.getServiceConnectionForTesting().onServiceConnected(null, mBinder);

        mController.startOrUpdateForegroundService(
                /* newNotificationId= */ 1,
                mNotification,
                /* oldNotificationId= */ 2,
                /* killOldNotification= */ true);
        verify(mServiceImpl)
                .startOrUpdateForegroundService(
                        /* newNotificationId= */ 1,
                        mNotification,
                        /* oldNotificationId= */ 2,
                        /* killOldNotification= */ true);

        mController.stopActorForegroundService(/* flags= */ ServiceCompat.STOP_FOREGROUND_REMOVE);
        verify(mServiceImpl)
                .stopActorForegroundService(/* flags= */ ServiceCompat.STOP_FOREGROUND_REMOVE);
    }

    @Test
    public void testUnbindService() {
        mController.startAndBindService(() -> {});
        mController.getServiceConnectionForTesting().onServiceConnected(null, mBinder);
        assertTrue("Controller should be connected.", mController.isConnected());

        mController.unbindService();
        assertFalse("Controller should be disconnected after unbind.", mController.isConnected());
    }
}
