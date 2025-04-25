// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;


import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests loading the NTP and navigating between it and other pages. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NewTabPageNavigationTest {
    private static final String HISTOGRAM_NTP_MODULE_CLICK = "NewTabPage.Module.Click";
    private static final String HISTOGRAM_START_SURFACE_MODULE_CLICK = "StartSurface.Module.Click";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    private EmbeddedTestServer mTestServer;
    private RegularNewTabPageStation mNtp;

    @Before
    public void setUp() {
        mTestServer = mActivityTestRule.getTestServer();
        mHomepageTestRule.useChromeNtpForTest();
        mNtp = mActivityTestRule.startOnNtp();
    }

    /** Check that navigating away from the NTP does work. */
    @Test
    @LargeTest
    @Feature({"NewTabPage"})
    public void testNavigatingFromNtp() {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html");
        mNtp.loadWebPageProgrammatically(url);
    }

    /** Tests navigating back to the NTP after loading another page. */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testNavigateBackToNtpViaUrl() {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html");
        WebPageStation page = mNtp.loadWebPageProgrammatically(url);
        page.loadPageProgrammatically(UrlConstants.NTP_URL, RegularNewTabPageStation.newBuilder());
    }

    /** Tests navigating to the tab switcher from the NTP. */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testNavigateToTabSwitcherFromNtp() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.TAB_SWITCHER_BUTTON);

        RegularTabSwitcherStation tabSwitcher = mNtp.openRegularTabSwitcher();

        histogram.assertExpected();
        tabSwitcher.verifyTabSwitcherCardCount(1);
    }

    /** Tests navigating to the tab switcher from the Incognito NTP. */
    @Test
    @MediumTest
    public void testNavigateToTabSwitcherFromIncognitoNtp() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_NTP_MODULE_CLICK)
                        .expectNoRecords(HISTOGRAM_START_SURFACE_MODULE_CLICK)
                        .build();

        IncognitoTabSwitcherStation tabSwitcher =
                mNtp.openNewIncognitoTabFast().openIncognitoTabSwitcher();

        tabSwitcher.verifyTabSwitcherCardCount(1);
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 1, 1);
        histogram.assertExpected();
    }
}
