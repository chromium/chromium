// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.prerender.PrerenderTestHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.TimeoutException;

/**
 * Prerender tests.
 *
 * Tests are disabled on low-end devices. These only support one renderer for performance reasons.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrerenderTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * We are using Autocomplete Action Predictor to decide whether or not to prerender.
     * Without any training data the default action should be no-prerender.
     */
    @Test
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    @DisableIf.Build(sdk_is_greater_than = 25, message = "https://crbug.com/1014213")
    public void testNoPrerender() throws InterruptedException {
        String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/prerender/google.html");
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Mimic user behavior: touch to focus then type some URL.
        // Since this is a URL, it should be prerendered.
        // Type one character at a time to properly simulate input
        // to the action predictor.
        mActivityTestRule.typeInOmnibox(testUrl, true);

        Assert.assertFalse("URL should not have been prerendered.",
                PrerenderTestHelper.waitForPrerenderUrl(tab, testUrl, true));
        // Navigate should not use the prerendered version.
        Assert.assertEquals(TabLoadStatus.DEFAULT_PAGE_LOAD,
                mActivityTestRule.loadUrlInTab(
                        testUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab));
    }

    /*
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    */
    @Test
    @FlakyTest(message = "crbug.com/339668")
    public void testPrerenderNotDead() throws TimeoutException {
        String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/prerender/google.html");
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        PrerenderTestHelper.prerenderUrl(testUrl, tab);
        // Navigate should use the prerendered version.
        Assert.assertEquals(
                TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD, mActivityTestRule.loadUrl(testUrl));

        // Prerender again with new text; make sure we get something different.
        String newTitle = "Welcome to the YouTube";
        testUrl = mTestServer.getURL("/chrome/test/data/android/prerender/youtube.html");
        PrerenderTestHelper.prerenderUrl(testUrl, tab);

        // Make sure the current tab title is NOT from the prerendered page.
        Assert.assertNotEquals(newTitle, tab.getTitle());

        TabTitleObserver observer = new TabTitleObserver(tab, newTitle);

        // Now commit and see the new title.
        mActivityTestRule.loadUrl(testUrl);

        observer.waitForTitleUpdate(5);
        Assert.assertEquals(newTitle, tab.getTitle());
    }

    /**
     * Tests that we don't crash when dismissing a prerendered page with infobars and unload
     * handler (See bug 5757331).
     * Note that this bug happened with the instant code. Now that we use Wicked Fast, we don't
     * deal with infobars ourselves.
     */
    /*
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    */
    @Test
    @DisabledTest(message = "Prerenderer disables infobars. crbug.com/588808")
    public void testInfoBarDismissed() {
        final String url = mTestServer.getURL(
                "/chrome/test/data/geolocation/geolocation_on_load.html");
        final ExternalPrerenderHandler handler = PrerenderTestHelper.prerenderUrl(
                url, mActivityTestRule.getActivity().getActivityTab());

        // Cancel the prerender. This will discard the prerendered WebContents and close the
        // infobars.
        TestThreadUtils.runOnUiThreadBlocking(() -> handler.cancelCurrentPrerender());
    }
}
