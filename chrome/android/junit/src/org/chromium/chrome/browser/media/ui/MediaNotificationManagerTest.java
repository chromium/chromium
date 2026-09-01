// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.util.SparseArray;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

import java.lang.reflect.Field;
import java.util.Map;

/** JUnit tests for {@link MediaNotificationManager} to verify multiple notifications support. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationManagerTest extends MediaNotificationTestBase {

    @Before
    @Override
    public void setUp() {
        super.setUp();
        // Clean up the controller set by MediaNotificationTestBase to start clean.
        MediaNotificationManager.hideForAllTabs(getNotificationId());
        NotificationProxyUtils.setNotificationEnabledForTest(true);
    }

    @After
    @Override
    public void tearDown() {
        super.tearDown();
        // Clear again to avoid leaking state.
        MediaNotificationManager.hideForAllTabs(getNotificationId());
    }

    @SuppressWarnings("unchecked")
    private SparseArray<MediaNotificationController> getControllers() throws Exception {
        Field field = MediaNotificationManager.class.getDeclaredField("sControllers");
        field.setAccessible(true);
        return (SparseArray<MediaNotificationController>) field.get(null);
    }

    private Map<?, ?> getUniqueIdMap() throws Exception {
        Field field = MediaNotificationManager.class.getDeclaredField("sUniqueIdMap");
        field.setAccessible(true);
        return (Map<?, ?>) field.get(null);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testShowMultipleNotifications_Enabled() throws Exception {
        // Tab 1
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.setInstanceId(1).build();
        ChromeMediaNotificationManager.show(info1);

        // Tab 2
        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.setInstanceId(2).build();
        ChromeMediaNotificationManager.show(info2);

        SparseArray<MediaNotificationController> controllers = getControllers();
        Map<?, ?> uniqueIdMap = getUniqueIdMap();

        // Verify two controllers are registered.
        assertEquals(2, controllers.size());
        assertEquals(2, uniqueIdMap.size());

        // Verify they are different controllers.
        assertNotEquals(controllers.valueAt(0), controllers.valueAt(1));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testShowMultipleNotifications_Disabled() throws Exception {
        // Tab 1
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.setInstanceId(1).build();
        ChromeMediaNotificationManager.show(info1);

        // Tab 2
        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.setInstanceId(2).build();
        ChromeMediaNotificationManager.show(info2);

        SparseArray<MediaNotificationController> controllers = getControllers();
        Map<?, ?> uniqueIdMap = getUniqueIdMap();

        // Verify only one controller is registered (it was overwritten or reused).
        assertEquals(1, controllers.size());
        // Unique ID map should be empty because the feature is disabled.
        assertEquals(0, uniqueIdMap.size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testHideMultipleNotifications() throws Exception {
        // Show Tab 1 and Tab 2
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.setInstanceId(1).build();
        ChromeMediaNotificationManager.show(info1);

        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.setInstanceId(2).build();
        ChromeMediaNotificationManager.show(info2);

        SparseArray<MediaNotificationController> controllers = getControllers();
        assertEquals(2, controllers.size());

        // Hide Tab 1
        MediaNotificationManager.hide(1, getNotificationId());

        // Verify only one controller remains, and it is Tab 2's controller.
        assertEquals(1, controllers.size());

        // We can verify which one remains by checking the uniqueIdMap
        Map<?, ?> uniqueIdMap = getUniqueIdMap();
        assertEquals(1, uniqueIdMap.size());
        // The remaining key should have tabId = 2
        Object key = uniqueIdMap.keySet().iterator().next();
        android.util.Pair<?, ?> pair = (android.util.Pair<?, ?>) key;
        assertEquals(2, pair.first);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testClearMultipleNotifications() throws Exception {
        // Show Tab 1 and Tab 2
        MediaNotificationInfo info1 = mMediaNotificationInfoBuilder.setInstanceId(1).build();
        ChromeMediaNotificationManager.show(info1);

        MediaNotificationInfo info2 = mMediaNotificationInfoBuilder.setInstanceId(2).build();
        ChromeMediaNotificationManager.show(info2);

        SparseArray<MediaNotificationController> controllers = getControllers();
        assertEquals(2, controllers.size());

        // Clear all of this media type
        MediaNotificationManager.hideForAllTabs(getNotificationId());

        // Verify all are cleared
        assertEquals(0, controllers.size());
        assertEquals(0, getUniqueIdMap().size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testFGSPromotionAndDemotion_SingleTab() throws Exception {
        startAndRegisterService();

        // 1. Show notification for Tab 1 (playing)
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);

        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);

        // Manually initialize PendingIntent to bypass Robolectric timing issues
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);

        // Trigger throttler task to run immediately
        advanceTimeByMillis(500);

        // Verify Tab 1 is promoted to FGS
        assertTrue(controller1.isForeground());
        verify(mMockForegroundServiceUtils, times(1))
                .startForeground(any(), eq(uniqueId1), any(), anyInt());

        // 2. Pause Tab 1 (paused, supports swipe away)
        MediaNotificationInfo info1Paused =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(true).build();
        ChromeMediaNotificationManager.show(info1Paused);

        // Trigger throttler task
        advanceTimeByMillis(500);

        // Verify Tab 1 is demoted from FGS
        assertFalse(controller1.isForeground());
        verify(mMockForegroundServiceUtils, times(1))
                .stopForeground(any(), eq(android.app.Service.STOP_FOREGROUND_DETACH));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testFGSPromotionAndDemotion_MultiTab() throws Exception {
        startAndRegisterService();

        // 1. Show notification for Tab 1 (playing)
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);

        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Verify Tab 1 is FGS
        assertTrue(controller1.isForeground());

        // 2. Show notification for Tab 2 (playing)
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(false).build();
        ChromeMediaNotificationManager.show(info2);

        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Verify Tab 2 is now the active FGS, and Tab 1 is demoted
        assertTrue(controller2.isForeground());
        assertFalse(controller1.isForeground());
        verify(mMockForegroundServiceUtils, times(1))
                .startForeground(any(), eq(uniqueId2), any(), anyInt());

        // 3. Pause Tab 2 (active tab paused)
        MediaNotificationInfo info2Paused =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(true).build();
        ChromeMediaNotificationManager.show(info2Paused);
        advanceTimeByMillis(500);

        // Verify Tab 2 is demoted. Tab 1 is still playing, so the manager should automatically
        // fall back and promote it to FGS to protect its background playback.
        assertFalse(controller2.isForeground());
        assertTrue(controller1.isForeground());

        // 4. Play Tab 1 again (making it active)
        ChromeMediaNotificationManager.show(info1);
        advanceTimeByMillis(500);

        // Verify Tab 1 is promoted to FGS again
        assertTrue(controller1.isForeground());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testBackgroundTabProtection() throws Exception {
        startAndRegisterService();

        // 1. Show notification for Tab 1 (playing) -> Active
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);

        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Verify Tab 1 is FGS
        assertTrue(controller1.isForeground());

        // 2. Show notification for Tab 2 (paused) -> Background
        MediaNotificationInfo info2Paused =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(true).build();
        ChromeMediaNotificationManager.show(info2Paused);

        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Verify Tab 2 is still NOT FGS, and Tab 1 remains FGS
        assertFalse(controller2.isForeground());
        assertTrue(controller1.isForeground());

        // Verify startForeground was NOT called for Tab 2
        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), eq(uniqueId2), any(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testFallbackPromotionNotificationContent() throws Exception {
        startAndRegisterService();
        mMediaNotificationInfoBuilder.setPrivate(false);

        // 1. Show notification for Tab 1 (playing) with title "Tab 1 Title"
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder
                        .setInstanceId(1)
                        .setPaused(false)
                        .setMetadata(
                                new org.chromium.services.media_session.MediaMetadata(
                                        "Tab 1 Title", "artist", "album"))
                        .build();
        ChromeMediaNotificationManager.show(info1);

        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // 2. Show notification for Tab 2 (playing)
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(false).build();
        ChromeMediaNotificationManager.show(info2);

        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Verify Tab 2 takes over FGS
        assertTrue(controller2.isForeground());
        assertFalse(controller1.isForeground());

        // 3. Pause Tab 2. Tab 1 should be promoted to FGS.
        MediaNotificationInfo info2Paused =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(true).build();
        ChromeMediaNotificationManager.show(info2Paused);
        advanceTimeByMillis(500);

        assertTrue(controller1.isForeground());

        // Capture notifications passed to startForeground for Tab 1
        org.mockito.ArgumentCaptor<android.app.Notification> notificationCaptor =
                org.mockito.ArgumentCaptor.forClass(android.app.Notification.class);
        // startForeground should have been called for uniqueId1 when starting (step 1)
        // and when promoted (step 3).
        verify(mMockForegroundServiceUtils, times(2))
                .startForeground(any(), eq(uniqueId1), notificationCaptor.capture(), anyInt());

        // The second captured notification is the fallback promotion one.
        android.app.Notification fallbackNotification = notificationCaptor.getAllValues().get(1);
        assertNotNull(fallbackNotification);

        // Verify the fallback notification has the correct metadata, not empty.
        CharSequence title =
                fallbackNotification.extras.getCharSequence(android.app.Notification.EXTRA_TITLE);
        assertEquals("Tab 1 Title", title.toString());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testVideoSwitchWithMultipleTabs() throws Exception {
        startAndRegisterService();
        mMediaNotificationInfoBuilder.setPrivate(false);

        // 1. Tab 1 starts playing (FGS)
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);
        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);
        assertTrue(controller1.isForeground());

        // 2. Tab 2 starts playing (FGS transitions to Tab 2)
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(false).build();
        ChromeMediaNotificationManager.show(info2);
        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);
        assertTrue(controller2.isForeground());
        assertFalse(controller1.isForeground());

        // 3. Tab 3 starts playing (FGS transitions to Tab 3)
        MediaNotificationInfo info3 =
                mMediaNotificationInfoBuilder.setInstanceId(3).setPaused(false).build();
        ChromeMediaNotificationManager.show(info3);
        int uniqueId3 = MediaNotificationManager.getUniqueId(3, getNotificationId());
        MediaNotificationController controller3 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId3);
        assertNotNull(controller3);
        controller3.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);
        assertTrue(controller3.isForeground());
        assertFalse(controller2.isForeground());
        assertFalse(controller1.isForeground());

        // Mock subsequent startForeground calls for Tab 1 to fail twice then succeed.
        // 1st call for Tab 1 (step 1) already succeeded.
        doThrow(new RuntimeException("FGS failed")) // 2nd call (step 4 fallback)
                .doThrow(new RuntimeException("FGS failed")) // 3rd call (step 6 fallback)
                .doNothing() // 4th call (step 7 hide fallback)
                .when(mMockForegroundServiceUtils)
                .startForeground(any(), eq(uniqueId1), any(), anyInt());

        // 4. Tab 3 pauses (FGS fallback to Tab 1 because uniqueId1 < uniqueId2)
        MediaNotificationInfo info3Paused =
                mMediaNotificationInfoBuilder.setInstanceId(3).setPaused(true).build();
        ChromeMediaNotificationManager.show(info3Paused);
        advanceTimeByMillis(500);
        assertFalse(controller3.isForeground());
        assertFalse(controller1.isForeground()); // FAILED to promote
        assertFalse(controller2.isForeground());

        // 5. Tab 3 resumes playing (FGS transitions back to Tab 3)
        ChromeMediaNotificationManager.show(info3);
        advanceTimeByMillis(500);
        assertTrue(controller3.isForeground());
        assertFalse(controller1.isForeground());
        assertFalse(controller2.isForeground());

        // 6. Tab 3 transitions to next video: first uncontrollable and paused
        ChromeMediaNotificationManager.show(info3Paused);
        advanceTimeByMillis(500);
        // FGS fallback to Tab 1 again
        assertFalse(controller3.isForeground());
        assertFalse(controller1.isForeground()); // FAILED to promote again
        assertFalse(controller2.isForeground());

        // 7. Tab 3 is hidden (delayed hide)
        MediaNotificationManager.hide(3, getNotificationId());
        advanceTimeByMillis(500);
        assertNull(MediaNotificationManager.getControllerByNotificationId(uniqueId3));
        // Tab 1 should now successfully promote to FGS on hide fallback
        assertTrue(controller1.isForeground());

        // 8. Tab 3 plays next video (gets new unique ID)
        MediaNotificationInfo info3New =
                mMediaNotificationInfoBuilder.setInstanceId(3).setPaused(false).build();
        ChromeMediaNotificationManager.show(info3New);
        int uniqueId3New = MediaNotificationManager.getUniqueId(3, getNotificationId());
        assertNotEquals(uniqueId3, uniqueId3New);
        MediaNotificationController controller3New =
                MediaNotificationManager.getControllerByNotificationId(uniqueId3New);
        assertNotNull(controller3New);
        controller3New.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // FGS should transition to Tab 3 new
        assertTrue(controller3New.isForeground());
        assertFalse(controller1.isForeground());
        assertFalse(controller2.isForeground());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testMultipleNotificationsPublished() throws Exception {
        startAndRegisterService();

        // 1. Show Tab 1 (playing)
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);
        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // 2. Show Tab 2 (playing)
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(false).build();
        ChromeMediaNotificationManager.show(info2);
        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Get ShadowNotificationManager
        android.app.NotificationManager nm =
                (android.app.NotificationManager)
                        RuntimeEnvironment.getApplication()
                                .getSystemService(Context.NOTIFICATION_SERVICE);
        ShadowNotificationManager shadowNM = Shadows.shadowOf(nm);

        assertNotNull(shadowNM.getNotification(uniqueId1));
        assertNotNull(shadowNM.getNotification(uniqueId2));

        assertNotNull(controller1.mMediaSession);
        assertTrue(controller1.mMediaSession.isActive());
        assertNotNull(controller2.mMediaSession);
        assertTrue(controller2.mMediaSession.isActive());
        assertNotEquals(
                controller1.mMediaSession.getSessionToken(),
                controller2.mMediaSession.getSessionToken());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testHideNotification_LeavesOtherTabsNotifications() throws Exception {
        startAndRegisterService();

        // 1. Show Tab 1 (playing)
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);
        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);
        assertTrue(controller1.isForeground());

        // 2. Show Tab 2 (paused)
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(true).build();
        ChromeMediaNotificationManager.show(info2);
        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Verify both controllers exist
        assertEquals(2, getControllers().size());

        // 3. Close Tab 2 (e.g. closing window with Tab 2)
        MediaNotificationManager.hide(2, getNotificationId());
        advanceTimeByMillis(500);

        // Verify Tab 2 is removed, but Tab 1 remains active and in foreground
        assertNull(MediaNotificationManager.getControllerByNotificationId(uniqueId2));
        assertNotNull(MediaNotificationManager.getControllerByNotificationId(uniqueId1));
        assertTrue(controller1.isForeground());
        assertEquals(1, getControllers().size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testHidePlayingNotification_LeavesOtherPausedTabsNotifications() throws Exception {
        startAndRegisterService();

        // 1. Show Tab 1 (paused)
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(true).build();
        ChromeMediaNotificationManager.show(info1);
        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);
        assertFalse(controller1.isForeground());

        // 2. Show Tab 2 (playing)
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(false).build();
        ChromeMediaNotificationManager.show(info2);
        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);
        assertTrue(controller2.isForeground());

        // 3. Close Tab 2 (closing playing window)
        MediaNotificationManager.hide(2, getNotificationId());
        advanceTimeByMillis(500);

        // Verify Tab 2 is removed, but Tab 1 (paused) remains intact and didn't get promoted
        assertNull(MediaNotificationManager.getControllerByNotificationId(uniqueId2));
        assertNotNull(MediaNotificationManager.getControllerByNotificationId(uniqueId1));
        assertEquals(1, getControllers().size());
        assertFalse(controller1.isForeground());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testHideNonActiveTab_PreservesActiveFgsOwnershipAndControllerState()
            throws Exception {
        startAndRegisterService();

        // 1. Tab 1 plays (initial active FGS).
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);
        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        assertTrue(controller1.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId1));

        // 2. Tab 2 plays and becomes the new active FGS owner (Tab 1 moves to background).
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(false).build();
        ChromeMediaNotificationManager.show(info2);
        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        assertTrue(controller2.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId2));
        assertFalse(controller1.isForeground());

        // 3. Tab 3 is shown and paused (e.g. video transition).
        MediaNotificationInfo info3 =
                mMediaNotificationInfoBuilder.setInstanceId(3).setPaused(true).build();
        ChromeMediaNotificationManager.show(info3);
        int uniqueId3 = MediaNotificationManager.getUniqueId(3, getNotificationId());
        MediaNotificationController controller3 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId3);
        assertNotNull(controller3);
        controller3.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Tab 2 should still be the active FGS owner.
        assertTrue(controller2.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId2));
        assertEquals(3, getControllers().size());

        // 4. Tab 3 delayed hide timer expires -> hide(3)
        MediaNotificationManager.hide(3, getNotificationId());
        advanceTimeByMillis(500);

        // Verify Tab 3 is removed, but Tab 2 remains active FGS owner without churn.
        assertNull(MediaNotificationManager.getControllerByNotificationId(uniqueId3));
        assertNotNull(MediaNotificationManager.getControllerByNotificationId(uniqueId1));
        assertNotNull(MediaNotificationManager.getControllerByNotificationId(uniqueId2));
        assertTrue(controller2.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId2));
        assertFalse(controller1.isForeground());
        assertEquals(2, getControllers().size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testHideActiveTab_PromotesFallbackPlayingTab() throws Exception {
        startAndRegisterService();

        // 1. Tab 1 plays in background.
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);
        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // 2. Tab 2 plays and becomes the active FGS owner.
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(false).build();
        ChromeMediaNotificationManager.show(info2);
        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // Verify Tab 2 is active FGS.
        assertTrue(controller2.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId2));

        // 3. Tab 2 is hidden.
        MediaNotificationManager.hide(2, getNotificationId());
        advanceTimeByMillis(500);

        // Verify Tab 2 is removed, and Tab 1 is promoted to active FGS.
        assertNull(MediaNotificationManager.getControllerByNotificationId(uniqueId2));
        assertNotNull(MediaNotificationManager.getControllerByNotificationId(uniqueId1));
        assertTrue(controller1.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId1));
        assertEquals(1, getControllers().size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
    public void testHideWhenActiveTabIsNonSwipeablePaused_PreservesNonSwipeableFgsOwnership()
            throws Exception {
        startAndRegisterService();

        // 1. Tab 1 is non-swipeable (e.g. Cast) and active.
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder
                        .setInstanceId(1)
                        .setPaused(false)
                        .setActions(
                                MediaNotificationInfo.ACTION_STOP
                                        | MediaNotificationInfo.ACTION_PLAY_PAUSE)
                        .build();
        ChromeMediaNotificationManager.show(info1);
        int uniqueId1 = MediaNotificationManager.getUniqueId(1, getNotificationId());
        MediaNotificationController controller1 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId1);
        assertNotNull(controller1);
        controller1.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        assertTrue(controller1.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId1));

        // Pause Tab 1 (non-swipeable should remain in FGS when paused).
        MediaNotificationInfo info1Paused =
                mMediaNotificationInfoBuilder
                        .setInstanceId(1)
                        .setPaused(true)
                        .setActions(
                                MediaNotificationInfo.ACTION_STOP
                                        | MediaNotificationInfo.ACTION_PLAY_PAUSE)
                        .build();
        ChromeMediaNotificationManager.show(info1Paused);
        advanceTimeByMillis(500);

        assertTrue(controller1.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId1));

        // 2. Tab 2 is non-swipeable and paused.
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder
                        .setInstanceId(2)
                        .setPaused(true)
                        .setActions(
                                MediaNotificationInfo.ACTION_STOP
                                        | MediaNotificationInfo.ACTION_PLAY_PAUSE)
                        .build();
        ChromeMediaNotificationManager.show(info2);
        int uniqueId2 = MediaNotificationManager.getUniqueId(2, getNotificationId());
        MediaNotificationController controller2 =
                MediaNotificationManager.getControllerByNotificationId(uniqueId2);
        assertNotNull(controller2);
        controller2.mPendingIntentActionSwipe = mock(PendingIntentProvider.class);
        advanceTimeByMillis(500);

        // 3. Tab 2 is hidden.
        MediaNotificationManager.hide(2, getNotificationId());
        advanceTimeByMillis(500);

        // Verify Tab 1 (paused non-swipeable) retains FGS ownership.
        assertNull(MediaNotificationManager.getControllerByNotificationId(uniqueId2));
        assertNotNull(MediaNotificationManager.getControllerByNotificationId(uniqueId1));
        assertTrue(controller1.isForeground());
        assertTrue(MediaNotificationManager.isNotificationActive(uniqueId1));
    }

    private void startAndRegisterService() {
        ensureService();
        MediaNotificationManager.setService(getNotificationId(), mService);
    }
}
