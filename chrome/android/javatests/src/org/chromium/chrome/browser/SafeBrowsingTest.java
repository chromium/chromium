// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.Callable;

/**
 * Test integration with the SafeBrowsingApiHandler.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class SafeBrowsingTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    private EmbeddedTestServer mTestServer;

    /**
     * Wait for an interstitial (or lack thereof) to be shown.
     * Disclaimer: when |shouldBeShown| is false, it isn't clear that the interstitial would never
     * be shown at some point in the near future. There isn't currently a way to wait for some event
     * that would indicate this, unfortunately.
     */
    private void waitForInterstitial(final boolean shouldBeShown) {
        CriteriaHelper.pollUiThread(Criteria.equals(shouldBeShown, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                // TODO(carlosil): For now, we check the presence of an interstitial through the
                // title since isShowingInterstitialPage does not work with committed interstitials.
                // Once we fully migrate to committed interstitials, this should be changed to a
                // more robust check.
                return getWebContents().getTitle().equals("Security error");
            }
        }));
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getActivity().getCurrentWebContents();
    }

    /*
     * Loads a URL in the current tab without waiting for the load to finish.
     * This is necessary because pages with interstitials do not finish loading.
     */
    private void loadUrlNonBlocking(String url) {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(url, PageTransition.TYPED)));
    }

    @Before
    public void setUp() {
        // Create a new temporary instance to ensure the Class is loaded. Otherwise we will get a
        // ClassNotFoundException when trying to instantiate during startup.
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
        MockSafeBrowsingApiHandler.clearMockResponses();
    }

    @Test
    @MediumTest
    public void noInterstitialPage() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.loadUrl(url);
        waitForInterstitial(false);
    }

    @Test
    @MediumTest
    public void interstitialPage() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        MockSafeBrowsingApiHandler.addMockResponse(url, "{\"matches\":[{\"threat_type\":\"5\"}]}");
        mActivityTestRule.startMainActivityOnBlankPage();

        loadUrlNonBlocking(url);
        waitForInterstitial(true);
    }
}
