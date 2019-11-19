// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.interstitials;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for the Lookalike URL interstitial (aka confusables). */
@RunWith(ChromeJUnit4ClassRunner.class)
@MediumTest
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
// clang-format off
@ParameterizedCommandLineFlags({
  @Switches(),
  @Switches("enable-features=" + ChromeFeatureList.LOOKALIKE_NAVIGATION_URL_SUGGESTIONS_UI),
})
// clang-format on
public class LookalikeInterstitialTest {
    private static final String INTERSTITIAL_TITLE_PREFIX = "Continue to ";

    private static final int INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS = 5;

    private EmbeddedTestServer mServer;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
        mServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mServer.stopAndDestroyServer();
    }

    @Test
    @Ignore("crbug/941488")
    public void testBasicInterstitialShown() throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.loadUrlOnUiThread(tab,
                mServer.getURLWithHostName("xn--googl-fsa.com", // googlé.com
                        "/chrome/test/data/android/navigate/simple.html"));

        // Wait for the interstitial page to commit and check the page title.
        new TabTitleObserver(tab, INTERSTITIAL_TITLE_PREFIX) {
            @Override
            protected boolean doesTitleMatch(String expectedTitle, String actualTitle) {
                return actualTitle.indexOf(expectedTitle) == 0;
            }
        }.waitForTitleUpdate(INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS);
        Assert.assertEquals(0, tab.getTitle().indexOf(INTERSTITIAL_TITLE_PREFIX));
    }
}
