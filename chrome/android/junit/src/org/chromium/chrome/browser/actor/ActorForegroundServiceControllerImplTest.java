// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.url.GURL;

import java.util.Collections;

/** Unit tests for {@link ActorForegroundServiceControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorForegroundServiceControllerImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorForegroundServiceImpl mServiceImpl;
    @Mock private ActorForegroundServiceImpl.LocalBinder mBinder;
    @Mock private Notification mNotification;
    @Mock private ActorTask mActorTask;
    @Mock private AsyncInitializationActivity mChromeActivity;
    @Mock private SettingsActivity mSettingsActivity;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;

    private ActorForegroundServiceControllerImpl mController;
    private ShadowApplication mShadowApplication;

    @Before
    public void setUp() {
        mController = new ActorForegroundServiceControllerImpl();
        mShadowApplication = shadowOf(RuntimeEnvironment.getApplication());
        when(mBinder.getService()).thenReturn(mServiceImpl);
        ApplicationStatus.destroyForJUnitTests();
        ApplicationStatus.initialize(RuntimeEnvironment.getApplication());
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
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

    @Test
    public void testCreateTrustedBringTabToFrontIntent() {
        int tabId = 123;
        int taskId = 456;
        when(mActorTask.getId()).thenReturn(taskId);
        when(mActorTask.getLastActedTabs()).thenReturn(Collections.singleton(tabId));

        Intent intent = mController.createTrustedBringTabToFrontIntent(mActorTask);
        assertNotNull("Intent should not be null.", intent);
        assertEquals(
                "Intent extra should contain the correct tabId.",
                tabId,
                intent.getIntExtra("BRING_TAB_TO_FRONT", Tab.INVALID_TAB_ID));
        assertTrue(
                "Intent should have EXTRA_SHOW_ACTOR_CONTROL.",
                intent.getBooleanExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, false));
        assertEquals(
                "Intent should have the correct taskId.",
                taskId,
                intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, -1));
    }

    @Test
    public void testCreateTrustedBringTabToFrontIntent_EmptyTabs() {
        int taskId = 456;
        when(mActorTask.getId()).thenReturn(taskId);
        when(mActorTask.getLastActedTabs()).thenReturn(Collections.emptySet());

        Intent intent = mController.createTrustedBringTabToFrontIntent(mActorTask);
        assertNotNull("Intent should not be null.", intent);
        assertEquals(
                "Intent extra should contain INVALID_TAB_ID for empty tabs.",
                Tab.INVALID_TAB_ID,
                intent.getIntExtra("BRING_TAB_TO_FRONT", Tab.INVALID_TAB_ID));
        assertTrue(
                "Intent should have EXTRA_SHOW_ACTOR_CONTROL.",
                intent.getBooleanExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, false));
        assertEquals(
                "Intent should have the correct taskId.",
                taskId,
                intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, -1));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoActivities() {
        assertFalse(mController.isActivityVisibleForTabs(Collections.emptySet()));
    }

    @Test
    public void testIsActivityVisibleForTabs_WithTabs_SilencesWhenTabInActivity() {
        int tabId = 123;
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.RESUMED);
        when(mTabModelSelector.getTabById(tabId)).thenReturn(mTab);

        assertTrue(mController.isActivityVisibleForTabs(Collections.singleton(tabId)));
    }

    @Test
    public void testIsActivityVisibleForTabs_WithTabs_NoSilenceWhenTabNotInActivity() {
        int tabId = 123;
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        when(mTabModelSelector.getTabById(tabId)).thenReturn(null);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(tabId)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceWhenInPiP() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        when(mChromeActivity.isInPictureInPictureMode()).thenReturn(true);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceWhenInIncognito() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_SettingsActivity_NotVisible() {
        ApplicationStatus.onStateChangeForTesting(mSettingsActivity, ActivityState.CREATED);
        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceWhenActivityFinishing() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.RESUMED);
        when(mChromeActivity.isFinishing()).thenReturn(true);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceWhenActivityDestroyed() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.RESUMED);
        when(mChromeActivity.isDestroyed()).thenReturn(true);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceOnNtp() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.RESUMED);

        GURL ntpUrl = new GURL("chrome-native://newtab/");
        when(mTab.getUrl()).thenReturn(ntpUrl);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    public ServiceConnection getServiceConnectionForTesting() {
        return mController.getServiceConnectionForTesting();
    }
}
