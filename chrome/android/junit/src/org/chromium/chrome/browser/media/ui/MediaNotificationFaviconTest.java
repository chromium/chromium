// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doCallRealMethod;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Test of media notifications to ensure that the favicon is displayed on normal devices and
 * not displayed on Android Go devices.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationFaviconTest extends MediaNotificationTestBase {
    private static final int TAB_ID_1 = 1;

    private final Bitmap mFavicon = Bitmap.createBitmap(192, 192, Bitmap.Config.ARGB_8888);
    private GURL mFaviconUrl;
    private MediaNotificationTestTabHolder mTabHolder;

    // Mock LargeIconBridge that runs callback using the given favicon.
    private static class TestLargeIconBridge extends LargeIconBridge {
        private LargeIconCallback mCallback;
        private boolean mGetIconCalledAtLeastOnce;

        public boolean getIconCalledAtLeastOnce() {
            return mGetIconCalledAtLeastOnce;
        }

        @Override
        public boolean getLargeIconForUrl(
                final GURL pageUrl, int desiredSizePx, final LargeIconCallback callback) {
            mGetIconCalledAtLeastOnce = true;
            mCallback = callback;
            return true;
        }

        public void runCallback(Bitmap favicon) {
            mCallback.onLargeIconAvailable(favicon, Color.BLACK, false, IconType.INVALID);
        }
    }

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
        mFaviconUrl = JUnitTestGURLs.EXAMPLE_URL;
    }

    @Test
    public void testSetNotificationIcon() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateFaviconUpdated(mFavicon, mFaviconUrl);
        assertEquals(mFavicon, getDisplayedIcon());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.O)
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testSetNotificationIcon_lowMem_O() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateFaviconUpdated(mFavicon, mFaviconUrl);
        assertEquals(null, getDisplayedIcon());
    }

    @Test
    public void testGetNullNotificationIcon() {
        mTabHolder.simulateFaviconUpdated(null, null);
        TestLargeIconBridge largeIconBridge = new TestLargeIconBridge();
        mTabHolder.mMediaSessionTabHelper.mMediaSessionHelper.mLargeIconBridge = largeIconBridge;

        // Simulate and hide notification.
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals(null, getDisplayedIcon());
        mTabHolder.simulateMediaSessionStateChanged(false, false);

        // Since the onFaviconUpdated was never called with valid favicon, the helper does not try
        // to fetch favicon.
        assertFalse(largeIconBridge.getIconCalledAtLeastOnce());
        mTabHolder.simulateFaviconUpdated(mFavicon, mFaviconUrl);

        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals(null, getDisplayedIcon());

        assertTrue(largeIconBridge.getIconCalledAtLeastOnce());
        largeIconBridge.runCallback(null);
        assertEquals(null, getDisplayedIcon());
    }

    @Test
    public void testGetNotificationIcon() {
        mTabHolder.simulateFaviconUpdated(mFavicon, mFaviconUrl);
        TestLargeIconBridge largeIconBridge = new TestLargeIconBridge();
        mTabHolder.mMediaSessionTabHelper.mMediaSessionHelper.mLargeIconBridge = largeIconBridge;

        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals(null, getDisplayedIcon());

        assertTrue(largeIconBridge.getIconCalledAtLeastOnce());
        largeIconBridge.runCallback(mFavicon);
        assertEquals(mFavicon, getDisplayedIcon());
    }

    @Test
    public void testWillReturnLargeIcon() {
        mTabHolder.simulateFaviconUpdated(mFavicon, mFaviconUrl);
        mTabHolder.mMediaSessionTabHelper.mMediaSessionHelper.mLargeIconBridge =
                new TestLargeIconBridge();

        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals(0, getCurrentNotificationInfo().defaultNotificationLargeIcon);
    }

    @Test
    public void testNoLargeIcon() {
        mTabHolder.simulateFaviconUpdated(null, null);
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertNotEquals(0, getCurrentNotificationInfo().defaultNotificationLargeIcon);
    }

    private Bitmap getDisplayedIcon() {
        return mTabHolder.mMediaSessionTabHelper.mMediaSessionHelper.mFavicon;
    }

    private MediaNotificationInfo getCurrentNotificationInfo() {
        return mTabHolder.mMediaSessionTabHelper.mMediaSessionHelper.mNotificationInfoBuilder
                .build();
    }

    @Override
    int getNotificationId() {
        return R.id.media_playback_notification;
    }
}
