// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.NotificationManager;
import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceManager;

/** Unit tests for {@link GlicExperimentalOptInBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures("CacheNotificationsEnabled")
public class GlicExperimentalOptInBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorForegroundServiceManager mFgsManager;
    @Mock private GlicExperimentalOptInBridge.Natives mNativeMock;

    @Before
    public void setUp() {
        setNotificationsEnabled(true);
        ActorForegroundServiceManager.setInstanceForTesting(mFgsManager);
        GlicExperimentalOptInBridgeJni.setInstanceForTesting(mNativeMock);
        GlicExperimentalOptInBridge.resetForTesting();
    }

    @After
    public void tearDown() {
        GlicExperimentalOptInBridge.resetForTesting();
        ApplicationStatus.destroyForJUnitTests();
        setNotificationsEnabled(true);
        ActorForegroundServiceManager.resetInstanceForTesting();
        GlicExperimentalOptInBridgeJni.setInstanceForTesting(null);
    }

    private static void setNotificationsEnabled(boolean enabled) {
        NotificationManager notificationManager =
                (NotificationManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.NOTIFICATION_SERVICE);
        Shadows.shadowOf(notificationManager).setNotificationsEnabled(enabled);
    }

    @Test
    public void testShow_PinsOptInNotificationToForegroundService() {
        assertTrue(GlicExperimentalOptInBridge.show());
        assertTrue(GlicExperimentalOptInBridge.isShowingForTesting());

        verify(mFgsManager).pinOptInNotification(any());
    }

    @Test
    public void testShow_NotificationsDisabled_ReturnsFalse() {
        setNotificationsEnabled(false);

        assertFalse(GlicExperimentalOptInBridge.show());
        assertFalse(GlicExperimentalOptInBridge.isShowingForTesting());
        verify(mFgsManager, never()).pinOptInNotification(any());
    }

    @Test
    public void testShow_NullForegroundServiceManager_ReturnsFalse() {
        ActorForegroundServiceManager.resetInstanceForTesting();

        assertFalse(GlicExperimentalOptInBridge.show());
        assertFalse(GlicExperimentalOptInBridge.isShowingForTesting());
    }

    @Test
    public void testHide_UnpinsFromForegroundService() {
        assertTrue(GlicExperimentalOptInBridge.show());

        GlicExperimentalOptInBridge.hide();

        assertFalse(GlicExperimentalOptInBridge.isShowingForTesting());
        verify(mFgsManager).unpinOptInNotification();
    }

    @Test
    public void testShow_AlreadyShowing_IsIdempotent() {
        assertTrue(GlicExperimentalOptInBridge.show());
        assertTrue(GlicExperimentalOptInBridge.show());

        verify(mFgsManager, times(1)).pinOptInNotification(any());
    }

    @Test
    public void testOnOptInCancelled_WhenShowing_ClearsStateAndNotifiesNative() {
        assertTrue(GlicExperimentalOptInBridge.show());

        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        verify(mFgsManager).pinOptInNotification(captor.capture());

        captor.getValue().run();
        assertFalse(GlicExperimentalOptInBridge.isShowingForTesting());
        verify(mFgsManager, never()).unpinOptInNotification();
        verify(mNativeMock).onOptInCancelled();

        captor.getValue().run();
        verify(mNativeMock, times(1)).onOptInCancelled();
    }

    @Test
    public void testShow_SynchronousCancellationInPinOptInNotification_ReturnsFalse() {
        doAnswer(
                        invocation -> {
                            Runnable cancelCallback = invocation.getArgument(0);
                            cancelCallback.run();
                            return null;
                        })
                .when(mFgsManager)
                .pinOptInNotification(any());

        assertFalse(GlicExperimentalOptInBridge.show());
        assertFalse(GlicExperimentalOptInBridge.isShowingForTesting());
        verify(mNativeMock).onOptInCancelled();
    }

    @Test
    public void testActivityStateListener_NotifiesNativeOnlyWhileShowing() {
        Activity trackedActivity = Robolectric.buildActivity(Activity.class).create().get();
        assertTrue(GlicExperimentalOptInBridge.show());

        ApplicationStatus.onStateChangeForTesting(trackedActivity, ActivityState.STARTED);
        verify(mNativeMock, times(1)).onActivityStartedOrResumed();

        ApplicationStatus.onStateChangeForTesting(trackedActivity, ActivityState.RESUMED);
        verify(mNativeMock, times(2)).onActivityStartedOrResumed();

        ApplicationStatus.onStateChangeForTesting(trackedActivity, ActivityState.PAUSED);
        verify(mNativeMock, times(2)).onActivityStartedOrResumed();

        GlicExperimentalOptInBridge.hide();
        ApplicationStatus.onStateChangeForTesting(trackedActivity, ActivityState.RESUMED);
        verify(mNativeMock, times(2)).onActivityStartedOrResumed();

        assertTrue(GlicExperimentalOptInBridge.show());
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        verify(mFgsManager, times(2)).pinOptInNotification(captor.capture());
        captor.getValue().run();
        ApplicationStatus.onStateChangeForTesting(trackedActivity, ActivityState.STARTED);
        verify(mNativeMock, times(2)).onActivityStartedOrResumed();
    }
}
