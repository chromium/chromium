// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.SearchGeolocationDisclosureTabHelper;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.PermissionInfo;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Tests for the SearchGeolocationDisclosureInfobar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SearchGeolocationDisclosureInfoBarTest {
    private static final String SEARCH_PAGE = "/chrome/test/data/android/google.html";

    private EmbeddedTestServer mTestServer;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        // Simulate the DSE being granted location (the test server isn't set to be the DSE).
        PermissionInfo locationSettings = new PermissionInfo(
                PermissionInfo.Type.GEOLOCATION, mTestServer.getURL(SEARCH_PAGE), null, false);
        ThreadUtils.runOnUiThread(() -> locationSettings.setContentSetting(ContentSetting.ALLOW));
    }

    @After
    public void tearDown() throws Exception {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testInfoBarAppears() throws InterruptedException, TimeoutException {
        SearchGeolocationDisclosureTabHelper.setIgnoreUrlChecksForTesting();
        Assert.assertEquals(
                "Wrong starting infobar count", 0, mActivityTestRule.getInfoBars().size());

        // Infobar should appear when doing the first search.
        InfoBarContainer container = mActivityTestRule.getInfoBarContainer();
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        container.addAnimationListener(listener);
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        // Note: the number of infobars is checked immediately after the URL is loaded, unlike in
        // other infobar tests where it is checked after animations have completed. This is because
        // (a) in this case it should work, as these infobars are added as part of the URL loading
        // process, and
        // (b) if this doesn't work, it is important to catch it as otherwise the checks that
        // infobars haven't being shown are invalid.
        Assert.assertEquals(
                "Wrong infobar count after search", 1, mActivityTestRule.getInfoBars().size());
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        // Infobar should not appear again on the same day.
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        // There can be a delay from infobars being removed in the native InfobarManager and them
        // being removed in the Java container, so wait until the infobar has really gone.
        InfoBarUtil.waitUntilNoInfoBarsExist(mActivityTestRule.getInfoBars());
        Assert.assertEquals(
                "Wrong infobar count after search", 0, mActivityTestRule.getInfoBars().size());

        // Infobar should appear again the next day.
        SearchGeolocationDisclosureTabHelper.setDayOffsetForTesting(1);
        listener = new InfoBarTestAnimationListener();
        container.addAnimationListener(listener);
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        Assert.assertEquals(
                "Wrong infobar count after search", 1, mActivityTestRule.getInfoBars().size());
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        // Infobar should not appear again on the same day.
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        InfoBarUtil.waitUntilNoInfoBarsExist(mActivityTestRule.getInfoBars());
        Assert.assertEquals(
                "Wrong infobar count after search", 0, mActivityTestRule.getInfoBars().size());

        // Infobar should appear again the next day.
        SearchGeolocationDisclosureTabHelper.setDayOffsetForTesting(2);
        listener = new InfoBarTestAnimationListener();
        container.addAnimationListener(listener);
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        Assert.assertEquals(
                "Wrong infobar count after search", 1, mActivityTestRule.getInfoBars().size());
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        // Infobar should not appear again on the same day.
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        InfoBarUtil.waitUntilNoInfoBarsExist(mActivityTestRule.getInfoBars());
        Assert.assertEquals(
                "Wrong infobar count after search", 0, mActivityTestRule.getInfoBars().size());

        // Infobar has appeared three times now, it should not appear again.
        SearchGeolocationDisclosureTabHelper.setDayOffsetForTesting(3);
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        Assert.assertEquals(
                "Wrong infobar count after search", 0, mActivityTestRule.getInfoBars().size());

        // Check histograms have been recorded.
        Assert.assertEquals("Wrong pre-disclosure metric", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GeolocationDisclosure.PreDisclosureDSESetting", 1));
        Assert.assertEquals("Wrong post-disclosure metric", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GeolocationDisclosure.PostDisclosureDSESetting", 1));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testInfoBarDismiss() throws InterruptedException, TimeoutException {
        SearchGeolocationDisclosureTabHelper.setIgnoreUrlChecksForTesting();
        Assert.assertEquals(
                "Wrong starting infobar count", 0, mActivityTestRule.getInfoBars().size());

        // Infobar should appear when doing the first search.
        InfoBarContainer container = mActivityTestRule.getInfoBarContainer();
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        container.addAnimationListener(listener);
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        Assert.assertEquals(
                "Wrong infobar count after search", 1, mActivityTestRule.getInfoBars().size());
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        // Dismiss the infobar.
        Assert.assertTrue(InfoBarUtil.clickCloseButton(mActivityTestRule.getInfoBars().get(0)));

        // Infobar should not appear again on the same day.
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        InfoBarUtil.waitUntilNoInfoBarsExist(mActivityTestRule.getInfoBars());
        Assert.assertEquals(
                "Wrong infobar count after search", 0, mActivityTestRule.getInfoBars().size());

        // Infobar should not appear the next day either, as it has been dismissed.
        SearchGeolocationDisclosureTabHelper.setDayOffsetForTesting(1);
        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        Assert.assertEquals(
                "Wrong infobar count after search", 0, mActivityTestRule.getInfoBars().size());
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testNoInfoBarForRandomUrl() throws InterruptedException, TimeoutException {
        Assert.assertEquals(
                "Wrong starting infobar count", 0, mActivityTestRule.getInfoBars().size());

        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        Assert.assertEquals(
                "Wrong infobar count after search", 0, mActivityTestRule.getInfoBars().size());
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testNoInfoBarInIncognito() throws InterruptedException, TimeoutException {
        SearchGeolocationDisclosureTabHelper.setIgnoreUrlChecksForTesting();
        mActivityTestRule.newIncognitoTabFromMenu();
        Assert.assertEquals(
                "Wrong starting infobar count", 0, mActivityTestRule.getInfoBars().size());

        mActivityTestRule.loadUrl(mTestServer.getURL(SEARCH_PAGE));
        Assert.assertEquals(
                "Wrong infobar count after search", 0, mActivityTestRule.getInfoBars().size());
    }
}
