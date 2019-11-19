// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doCallRealMethod;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.IconType;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.media.ui.MediaNotificationManager.ListenerService;

/**
 * Test of media notifications to ensure that the favicon is displayed on normal devices and
 * not displayed on Android Go devices.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        // Remove this after updating to a version of Robolectric that supports
        // notification channel creation. crbug.com/774315
        sdk = Build.VERSION_CODES.N_MR1,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationFaviconTest extends MediaNotificationManagerTestBase {
    private static final int TAB_ID_1 = 1;
    private static final String IS_LOW_END_DEVICE_SWITCH =
            "--" + BaseSwitches.ENABLE_LOW_END_DEVICE_MODE;

    private final Bitmap mFavicon = Bitmap.createBitmap(192, 192, Bitmap.Config.ARGB_8888);
    private MediaNotificationTestTabHolder mTabHolder;

    // Mock LargeIconBridge that runs callback using the given favicon.
    private class TestLargeIconBridge extends LargeIconBridge {
        private LargeIconCallback mCallback;
        private boolean mGetIconCalledAtLeastOnce;

        public boolean getIconCalledAtLeastOnce() {
            return mGetIconCalledAtLeastOnce;
        }

        @Override
        public boolean getLargeIconForUrl(
                final String pageUrl, int desiredSizePx, final LargeIconCallback callback) {
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

        getManager().mThrottler.mManager = getManager();
        doCallRealMethod().when(getManager()).onServiceStarted(any(ListenerService.class));
        doCallRealMethod()
                .when(mMockForegroundServiceUtils)
                .startForegroundService(any(Intent.class));
        mTabHolder = createMediaNotificationTestTabHolder(TAB_ID_1, "about:blank", "title1");
    }

    @Override
    @After
    public void tearDown() {
        CommandLine.reset();
    }

    @Test
    public void testSetNotificationIcon() {
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateFaviconUpdated(mFavicon);
        assertEquals(mFavicon, getDisplayedIcon());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testSetNotificationIcon_lowMem_preO() {
        // Run tests as a low-end device.
        CommandLine.init(new String[] {"testcommand", IS_LOW_END_DEVICE_SWITCH});

        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateFaviconUpdated(mFavicon);
        assertEquals(mFavicon, getDisplayedIcon());
    }

    // TODO(crbug.com/729029): Specify O-SDK.
    @Test
    public void testSetNotificationIcon_lowMem_O() {
        // Run tests as a low-end device.
        CommandLine.init(new String[] {"testcommand", IS_LOW_END_DEVICE_SWITCH});

        mTabHolder.simulateMediaSessionStateChanged(true, false);
        mTabHolder.simulateFaviconUpdated(mFavicon);
        assertEquals(Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ? null : mFavicon,
                getDisplayedIcon());
    }

    @Test
    public void testGetNullNotificationIcon() {
        mTabHolder.simulateFaviconUpdated(null);
        TestLargeIconBridge largeIconBridge = new TestLargeIconBridge();
        mTabHolder.mMediaSessionTabHelper.mLargeIconBridge = largeIconBridge;

        // Simulate and hide notification.
        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals(null, getDisplayedIcon());
        mTabHolder.simulateMediaSessionStateChanged(false, false);

        // Since the onFaviconUpdated was never called with valid favicon, the helper does not try
        // to fetch favicon.
        assertFalse(largeIconBridge.getIconCalledAtLeastOnce());
        mTabHolder.simulateFaviconUpdated(mFavicon);

        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals(null, getDisplayedIcon());

        assertTrue(largeIconBridge.getIconCalledAtLeastOnce());
        largeIconBridge.runCallback(null);
        assertEquals(null, getDisplayedIcon());
    }

    @Test
    public void testGetNotificationIcon() {
        mTabHolder.simulateFaviconUpdated(mFavicon);
        TestLargeIconBridge largeIconBridge = new TestLargeIconBridge();
        mTabHolder.mMediaSessionTabHelper.mLargeIconBridge = largeIconBridge;

        mTabHolder.simulateMediaSessionStateChanged(true, false);
        assertEquals(null, getDisplayedIcon());

        assertTrue(largeIconBridge.getIconCalledAtLeastOnce());
        largeIconBridge.runCallback(mFavicon);
        assertEquals(mFavicon, getDisplayedIcon());
    }

    private Bitmap getDisplayedIcon() {
        return mTabHolder.mMediaSessionTabHelper.mFavicon;
    }

    @Override
    int getNotificationId() {
        return R.id.media_playback_notification;
    }
}
