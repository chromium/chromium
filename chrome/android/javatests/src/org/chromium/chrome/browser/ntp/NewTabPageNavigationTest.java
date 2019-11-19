// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests loading the NTP and navigating between it and other pages.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class NewTabPageNavigationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(null);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Sanity check that we do start on the NTP by default.
     */
    @Test
    @MediumTest
    @Feature({"NewTabPage", "Main"})
    public void testNTPIsDefault() {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertNotNull(tab);
        String url = tab.getUrl();
        Assert.assertTrue("Unexpected url: " + url,
                url.startsWith("chrome-native://newtab/")
                        || url.startsWith("chrome-native://bookmarks/")
                        || url.startsWith("chrome-native://recent-tabs/"));
    }

    /**
     * Check that navigating away from the NTP does work.
     */
    @Test
    @LargeTest
    @Feature({"NewTabPage"})
    public void testNavigatingFromNTP() {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html");
        mActivityTestRule.loadUrl(url);
        Assert.assertEquals(url, mActivityTestRule.getActivity().getActivityTab().getUrl());
    }

    /**
     * Tests navigating back to the NTP after loading another page.
     */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testNavigateBackToNTPViaUrl() {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html");
        mActivityTestRule.loadUrl(url);
        Assert.assertEquals(url, mActivityTestRule.getActivity().getActivityTab().getUrl());

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertNotNull(tab);
        url = tab.getUrl();
        Assert.assertEquals(UrlConstants.NTP_URL, url);

        // Check that the NTP is actually displayed.
        Assert.assertNotNull(tab.getNativePage() instanceof NewTabPage);
    }
}
