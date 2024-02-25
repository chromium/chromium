// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.Mockito.doReturn;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link HomeSurfaceTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeSurfaceTrackerUnitTest {
    @Mock private Tab mNtpTab;
    @Mock private Tab mLastActiveTab;

    private HomeSurfaceTracker mHomeSurfaceTracker;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHomeSurfaceTracker = new HomeSurfaceTracker();

        doReturn(JUnitTestGURLs.URL_1).when(mLastActiveTab).getUrl();
        doReturn(true).when(mNtpTab).isNativePage();
    }

    @Test
    public void testUpdateHomeSurfaceAndTrackingTabs() {
        mHomeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(mNtpTab, mLastActiveTab);
        Assert.assertEquals(mNtpTab, mHomeSurfaceTracker.getHomeSurfaceTabForTesting());
        Assert.assertEquals(
                mLastActiveTab, mHomeSurfaceTracker.getLastActiveTabToTrackForTesting());

        mHomeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(mNtpTab, null);
        Assert.assertEquals(mNtpTab, mHomeSurfaceTracker.getHomeSurfaceTabForTesting());
        Assert.assertNull(mHomeSurfaceTracker.getLastActiveTabToTrackForTesting());
    }

    @Test
    public void testGetLastActiveTabToTrack() {
        mHomeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(mNtpTab, mLastActiveTab);
        Assert.assertEquals(mLastActiveTab, mHomeSurfaceTracker.getLastActiveTabToTrack());

        doReturn(true).when(mLastActiveTab).isClosing();
        Assert.assertNull(mHomeSurfaceTracker.getLastActiveTabToTrack());

        doReturn(false).when(mLastActiveTab).isClosing();
        doReturn(true).when(mLastActiveTab).isDestroyed();
        Assert.assertNull(mHomeSurfaceTracker.getLastActiveTabToTrack());
    }

    @Test
    public void testIsHomeSurfaceTab() {
        Assert.assertFalse(mHomeSurfaceTracker.isHomeSurfaceTab(mNtpTab));
        Assert.assertFalse(mHomeSurfaceTracker.isHomeSurfaceTab(null));

        mHomeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(mNtpTab, mLastActiveTab);
        Assert.assertFalse(mHomeSurfaceTracker.isHomeSurfaceTab(null));
        Assert.assertFalse(mHomeSurfaceTracker.isHomeSurfaceTab(mLastActiveTab));
        Assert.assertTrue(mHomeSurfaceTracker.isHomeSurfaceTab(mNtpTab));
    }

    @Test
    public void testCanShowHomeSurface() {
        mHomeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(mNtpTab, mLastActiveTab);

        Assert.assertFalse(mHomeSurfaceTracker.canShowHomeSurface(null));
        Assert.assertFalse(mHomeSurfaceTracker.canShowHomeSurface(mLastActiveTab));
        Assert.assertTrue(mHomeSurfaceTracker.canShowHomeSurface(mNtpTab));

        doReturn(true).when(mLastActiveTab).isClosing();
        Assert.assertFalse(mHomeSurfaceTracker.canShowHomeSurface(mNtpTab));

        doReturn(false).when(mLastActiveTab).isClosing();
        doReturn(true).when(mLastActiveTab).isDestroyed();
        Assert.assertFalse(mHomeSurfaceTracker.canShowHomeSurface(mNtpTab));
    }
}
