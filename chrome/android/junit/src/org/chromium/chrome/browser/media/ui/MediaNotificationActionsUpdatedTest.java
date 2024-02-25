// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doCallRealMethod;

import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.media_session.mojom.MediaSessionAction;

import java.util.HashSet;
import java.util.Set;

/**
 * Test of media notifications to see whether the control buttons update when MediaSessionActions
 * change or the tab navigates.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationActionsUpdatedTest extends MediaNotificationTestBase {
    private static final int TAB_ID_1 = 1;
    private static final int THROTTLE_MILLIS =
            MediaNotificationController.Throttler.THROTTLE_MILLIS;

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
    public void testActionsDefaultToNull() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals(DEFAULT_ACTIONS, getDisplayedActions());
    }

    @Test
    public void testActionPropagateProperly() {
        mTabHolder.simulateMediaSessionActionsChanged(buildActions());
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals(buildActions(), getDisplayedActions());
    }

    @Test
    public void testActionsResetAfterNavigation() {
        mTabHolder.simulateNavigation("https://example.com/", false);
        mTabHolder.simulateMediaSessionActionsChanged(buildActions());
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals(buildActions(), getDisplayedActions());

        mTabHolder.simulateNavigation("https://example1.com/", false);
        mTabHolder.simulateMediaSessionActionsChanged(DEFAULT_ACTIONS);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals(DEFAULT_ACTIONS, getDisplayedActions());
    }

    @Test
    public void testActionsPersistAfterSamePageNavigation() {
        mTabHolder.simulateNavigation("https://example.com/", false);
        mTabHolder.simulateMediaSessionActionsChanged(buildActions());
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals(buildActions(), getDisplayedActions());

        mTabHolder.simulateNavigation("https://example.com/foo.html", true);
        advanceTimeByMillis(THROTTLE_MILLIS);
        assertEquals(buildActions(), getDisplayedActions());
    }

    @Override
    int getNotificationId() {
        return R.id.media_playback_notification;
    }

    private Set<Integer> getDisplayedActions() {
        return getController().mMediaNotificationInfo.mediaSessionActions;
    }

    private Set<Integer> buildActions() {
        HashSet<Integer> actions = new HashSet<>();

        actions.add(MediaSessionAction.PLAY);
        actions.add(MediaSessionAction.PAUSE);
        return actions;
    }
}
