// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doCallRealMethod;

import android.app.Notification;
import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaSessionHelper;
import org.chromium.services.media_session.MediaMetadata;

/**
 * Test of media notifications to see whether the text updates when the tab title changes or the
 * MediaMetadata gets updated.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationTitleUpdatedTest extends MediaNotificationTestBase {
    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int THROTTLE_MILLIS =
            MediaNotificationController.Throttler.THROTTLE_MILLIS;
    private static final int HIDE_NOTIFICATION_DELAY_MILLIS =
            MediaSessionHelper.HIDE_NOTIFICATION_DELAY_MILLIS;

    private MediaNotificationTestTabHolder mTabHolder;

    @Before
    @Override
    public void setUp() {
        super.setUp();

        getController().mThrottler.mController = getController();
        doCallRealMethod().when(getController()).onServiceStarted(any(MockListenerService.class));
        doCallRealMethod()
                .when(mMockForegroundServiceUtils)
                .startForegroundService(any(Intent.class));
        mTabHolder = createMediaNotificationTestTabHolder(TAB_ID_1, "about:blank", "title1");
    }

    @Test
    public void testSessionStatePlaying() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals("title1", getDisplayedTitle());

        mTabHolder.simulateTitleUpdated("title2");
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title2", getDisplayedTitle());
    }

    @Test
    public void testSessionStateNewlyPaused() {
        mTabHolder.simulateMediaSessionStateChanged(true, true);
        assertNull(getController().mNotificationBuilder);

        mTabHolder.simulateTitleUpdated("title2");
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertNull(getController().mNotificationBuilder);
    }

    @Test
    public void testSessionStateUncontrollable() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals("title1", getDisplayedTitle());

        mTabHolder.simulateMediaSessionStateChanged(false, false);
        mTabHolder.simulateTitleUpdated("title2");
        advanceTimeByMillis(HIDE_NOTIFICATION_DELAY_MILLIS);
        assertNull(getController().mMediaNotificationInfo);
    }

    @Test
    public void testMediaMetadataSetsTitle() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateMediaSessionMetadataChanged(new MediaMetadata("title2", "", ""));
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title2", getDisplayedTitle());
    }

    @Test
    public void testMediaMetadataOverridesTitle() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateMediaSessionMetadataChanged(new MediaMetadata("title2", "", ""));
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title2", getDisplayedTitle());

        mTabHolder.simulateTitleUpdated("title2");
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title2", getDisplayedTitle());
    }

    /**
     * Test if a notification accepts the title update from another tab, using the following steps:
     *   1. set the title of mTab, start the media session, a notification should show up;
     *   2. stop the media session of mTab, the notification shall hide;
     *   3. create newTab, start the media session of newTab, a notification should show up,
     *      set the title of newTab, the notification title should match newTab;
     *   4. change the title of newTab and then mTab to different names,
     *      the notification should have the title of newTab.
     */
    @Test
    public void testMultipleTabs() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals("title1", getDisplayedTitle());
        mTabHolder.simulateMediaSessionStateChanged(false, false);

        MediaNotificationTestTabHolder newTabHolder =
                createMediaNotificationTestTabHolder(TAB_ID_2, "about:blank", "title2");

        newTabHolder.simulateMediaSessionStateChanged(true, false);
        newTabHolder.simulateTitleUpdated("title3");
        mTabHolder.simulateTitleUpdated("title2");
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title3", getDisplayedTitle());
    }

    /**
     * Test for the notification should not update for a paused tab if it mismatches the current tab
     * id:
     *   1. set the title of mTab, start the media session, a notification should show up;
     *   2. create newTab, start the media session of newTab, a notification should show up,
     *      set the title of newTab, the notification will match the title;
     *   3. stop the media session of mTab, and change its title, the notification should not
     *      change.
     */
    @Test
    public void testPreferLastActiveTab() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals("title1", getDisplayedTitle());

        MediaNotificationTestTabHolder newTabHolder =
                createMediaNotificationTestTabHolder(TAB_ID_2, "about:blank", "title2");

        newTabHolder.simulateMediaSessionStateChanged(true, false);
        newTabHolder.simulateTitleUpdated("title3");

        mTabHolder.simulateMediaSessionStateChanged(false, false);
        mTabHolder.simulateTitleUpdated("title2");

        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title3", getDisplayedTitle());
    }

    @Test
    public void testMediaMetadataResetsAfterNavigation() {
        mTabHolder.simulateNavigation("https://example.com/", false);
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateMediaSessionMetadataChanged(new MediaMetadata("title2", "", ""));
        mTabHolder.simulateMediaSessionActionsChanged(DEFAULT_ACTIONS);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title2", getDisplayedTitle());

        mTabHolder.simulateNavigation("https://example1.com/", false);
        mTabHolder.simulateMediaSessionActionsChanged(DEFAULT_ACTIONS);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title1", getDisplayedTitle());
    }

    @Test
    public void testMediaMetadataResetsAfterSameOriginNavigation() {
        mTabHolder.simulateNavigation("https://example.com/", false);
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateMediaSessionMetadataChanged(new MediaMetadata("title2", "", ""));
        mTabHolder.simulateMediaSessionActionsChanged(DEFAULT_ACTIONS);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title2", getDisplayedTitle());

        mTabHolder.simulateNavigation("https://example.com/foo.html", false);
        mTabHolder.simulateMediaSessionActionsChanged(DEFAULT_ACTIONS);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title1", getDisplayedTitle());
    }

    @Test
    public void testMediaMetadataPersistsAfterSameDocumentNavigation() {
        mTabHolder.simulateNavigation("https://example.com/", false);
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateMediaSessionMetadataChanged(new MediaMetadata("title2", "", ""));
        mTabHolder.simulateMediaSessionActionsChanged(DEFAULT_ACTIONS);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title2", getDisplayedTitle());

        mTabHolder.simulateNavigation("https://example.com/#1", true);
        mTabHolder.simulateMediaSessionActionsChanged(DEFAULT_ACTIONS);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals("title2", getDisplayedTitle());
    }

    private CharSequence getDisplayedTitle() {
        Notification notification = getController().mNotificationBuilder.build();
        ShadowNotification shadowNotification = Shadows.shadowOf(notification);

        return shadowNotification.getContentTitle();
    }

    @Override
    int getNotificationId() {
        return R.id.media_playback_notification;
    }
}
