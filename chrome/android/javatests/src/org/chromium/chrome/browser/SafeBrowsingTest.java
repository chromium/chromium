// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

/** Test integration with the SafeBrowsingApiHandler. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({ChromeFeatureList.SAFE_BROWSING_DELAYED_WARNINGS})
public final class SafeBrowsingTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    /**
     * Wait for an interstitial (or lack thereof) to be shown. Disclaimer: when |shouldBeShown| is
     * false, it isn't clear that the interstitial would never be shown at some point in the near
     * future. There isn't currently a way to wait for some event that would indicate this,
     * unfortunately.
     */
    private void waitForInterstitial(final boolean shouldBeShown) {
        CriteriaHelper.pollUiThread(
                () -> {
                    // TODO(carlosil): For now, we check the presence of an interstitial through the
                    // title since isShowingInterstitialPage does not work with committed
                    // interstitials.
                    // Once we fully migrate to committed interstitials, this should be changed to a
                    // more robust check.
                    String title = getWebContents().getTitle();
                    String errorTitle = "Security error";
                    if (shouldBeShown) {
                        Criteria.checkThat(title, Matchers.is(errorTitle));
                    } else {
                        Criteria.checkThat(title, Matchers.not(errorTitle));
                    }
                });
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
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(url, PageTransition.TYPED)));
    }

    @Before
    public void setUp() {
        SafeBrowsingApiBridge.setSafeBrowsingApiHandler(new MockSafeBrowsingApiHandler());
    }

    @After
    public void tearDown() {
        MockSafeBrowsingApiHandler.clearMockResponses();
        SafeBrowsingApiBridge.clearHandlerForTesting();
    }

    @Test
    @MediumTest
    public void noInterstitialPage() throws Exception {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mActivityTestRule.startMainActivityOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.loadUrl(url);
        waitForInterstitial(false);
    }

    @Test
    @MediumTest
    public void interstitialPage() throws Exception {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        MockSafeBrowsingApiHandler.addMockResponse(
                url, MockSafeBrowsingApiHandler.SOCIAL_ENGINEERING_CODE);
        mActivityTestRule.startMainActivityOnBlankPage();

        loadUrlNonBlocking(url);
        waitForInterstitial(true);
    }
}
