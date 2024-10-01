// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests loading the NTP and navigating between it and other pages. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NewTabPageNavigationTest {
    private static final String HISTOGRAM_NTP_MODULE_CLICK = "NewTabPage.Module.Click";
    private static final String HISTOGRAM_START_SURFACE_MODULE_CLICK = "StartSurface.Module.Click";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mHomepageTestRule.useChromeNtpForTest();
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
    }

    /** Sanity check that we do start on the NTP by default. */
    @Test
    @MediumTest
    @Feature({"NewTabPage", "Main"})
    public void testNtpIsDefault() {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertNotNull(tab);
        String url = ChromeTabUtils.getUrlStringOnUiThread(tab);
        Assert.assertTrue(
                "Unexpected url: " + url,
                url.startsWith("chrome-native://newtab/")
                        || url.startsWith("chrome-native://bookmarks/")
                        || url.startsWith("chrome-native://recent-tabs/"));
    }

    /** Check that navigating away from the NTP does work. */
    @Test
    @LargeTest
    @Feature({"NewTabPage"})
    public void testNavigatingFromNtp() {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html");
        mActivityTestRule.loadUrl(url);
        Assert.assertEquals(
                url,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }

    /** Tests navigating back to the NTP after loading another page. */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testNavigateBackToNtpViaUrl() {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html");
        mActivityTestRule.loadUrl(url);
        Assert.assertEquals(
                url,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertNotNull(tab);
        url = ChromeTabUtils.getUrlStringOnUiThread(tab);
        Assert.assertEquals(UrlConstants.NTP_URL, url);

        // Check that the NTP is actually displayed.
        Assert.assertTrue(tab.getNativePage() instanceof NewTabPage);
    }

    /** Tests navigating to the tab switcher from the NTP. */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testNavigateToTabSwitcherFromNtp() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Tab tab = cta.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            tab != null
                                    && !tab.isIncognito()
                                    && UrlUtilities.isNtpUrl(tab.getUrl()));
                });
        TabUiTestHelper.enterTabSwitcher(cta);
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 1);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.TAB_SWITCHER_BUTTON));
    }

    /** Tests navigating to the tab switcher from the Incognito NTP. */
    @Test
    @MediumTest
    public void testNavigateToTabSwitcherFromIncognitoNtp() {
        mActivityTestRule.newIncognitoTabFromMenu();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Tab tab = cta.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            tab != null
                                    && tab.isIncognito()
                                    && UrlUtilities.isNtpUrl(tab.getUrl()));
                });
        TabUiTestHelper.enterTabSwitcher(cta);
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 1);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 1);
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNtp.TAB_SWITCHER_BUTTON));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        ModuleTypeOnStartAndNtp.TAB_SWITCHER_BUTTON));
    }
}
