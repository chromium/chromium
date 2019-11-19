// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests starting the activity with URLs.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class MainActivityWithURLTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /**
     * Verify launch the activity with URL.
     */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testLaunchActivityWithURL() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        try {
            // Launch chrome
            mActivityTestRule.startMainActivityWithURL(
                    testServer.getURL("/chrome/test/data/android/simple.html"));
            String expectedTitle = "Activity test page";
            TabModel model = mActivityTestRule.getActivity().getCurrentTabModel();
            String title = model.getTabAt(model.index()).getTitle();
            Assert.assertEquals(expectedTitle, title);
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * Launch and verify URL is neither null nor empty.
     */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testLaunchActivity() {
        // Launch chrome
        mActivityTestRule.startMainActivityFromLauncher();
        String currentUrl = mActivityTestRule.getActivity().getActivityTab().getUrl();
        Assert.assertNotNull(currentUrl);
        Assert.assertEquals(false, currentUrl.isEmpty());
    }

    /**
     * Launch a NTP and make sure it loads correctly. This makes sure the
     * NTP loading complete notification is received.
     */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testNewTabPageLaunch() {
        // Launch chrome with NTP.
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        String currentUrl = mActivityTestRule.getActivity().getActivityTab().getUrl();
        Assert.assertNotNull(currentUrl);
        Assert.assertEquals(false, currentUrl.isEmpty());

        // Open NTP.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        currentUrl = mActivityTestRule.getActivity().getActivityTab().getUrl();
        Assert.assertNotNull(currentUrl);
        Assert.assertEquals(false, currentUrl.isEmpty());
    }
}
