// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.util.SparseArray;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;

import java.lang.reflect.Field;
import java.util.Map;

/** JUnit tests for {@link MediaNotificationManager} to verify multiple notifications support. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationManagerTest extends MediaNotificationTestBase {

    @Before
    @Override
    public void setUp() {
        super.setUp();
        // Clean up the controller set by MediaNotificationTestBase to start clean.
        MediaNotificationManager.hideForAllTabs(getNotificationId());
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
    public void testIsNotificationActive() throws Exception {
        // Show Tab 1 (playing)
        MediaNotificationInfo info1 =
                mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(false).build();
        ChromeMediaNotificationManager.show(info1);
        advanceTimeByMillis(500);

        // Get the unique ID for Tab 1
        Map<?, ?> uniqueIdMap = getUniqueIdMap();
        int id1 = (Integer) uniqueIdMap.get(android.util.Pair.create(1, getNotificationId()));

        // Verify Tab 1 is active
        assertTrue(MediaNotificationManager.isNotificationActive(id1));

        // Show Tab 2 (playing)
        MediaNotificationInfo info2 =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(false).build();
        ChromeMediaNotificationManager.show(info2);
        advanceTimeByMillis(500);

        int id2 = (Integer) uniqueIdMap.get(android.util.Pair.create(2, getNotificationId()));

        // Tab 2 should now be active because it was shown last and is playing.
        assertTrue(MediaNotificationManager.isNotificationActive(id2));
        assertFalse(MediaNotificationManager.isNotificationActive(id1));

        // Pause Tab 2. Tab 1 (which is still playing) should be promoted.
        MediaNotificationInfo info2Paused =
                mMediaNotificationInfoBuilder.setInstanceId(2).setPaused(true).build();
        ChromeMediaNotificationManager.show(info2Paused);

        assertTrue(MediaNotificationManager.isNotificationActive(id1));
        assertFalse(MediaNotificationManager.isNotificationActive(id2));
    }
}
