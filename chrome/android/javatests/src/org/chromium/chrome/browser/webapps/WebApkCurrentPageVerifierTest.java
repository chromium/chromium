// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests the {@link CurrentPageVerifier} integration with WebAPK Activity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class WebApkCurrentPageVerifierTest {
    @Rule public final WebApkActivityTestRule mActivityTestRule = new WebApkActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = mActivityTestRule.getTestServer();
    }

    private WebappActivity launchWebApk(String url) {
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder("org.chromium.webapk.random", url).build();
        return mActivityTestRule.startWebApkActivity(intentDataProvider);
    }

    private @VerificationStatus int getCurrentPageVerifierStatus() {
        WebappActivity webappActivity = mActivityTestRule.getActivity();
        return webappActivity.getComponent().resolveCurrentPageVerifier().getState().status;
    }

    /**
     * Tests that {@link CurrentPageVerifier} verification succeeds if the page is within the WebAPK
     * scope.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testInScope() {
        String page = mTestServer.getURL("/chrome/test/data/android/customtabs/cct_header.html");
        String otherPageInScope =
                mTestServer.getURL("/chrome/test/data/android/customtabs/cct_header_frame.html");
        String expectedScope = mTestServer.getURL("/chrome/test/data/android/customtabs/");
        WebappActivity webappActivity = launchWebApk(page);
        assertEquals(expectedScope, getWebappExtras(webappActivity).scopeUrl);

        mActivityTestRule.loadUrl(otherPageInScope);
        assertEquals(VerificationStatus.SUCCESS, getCurrentPageVerifierStatus());
    }

    /**
     * Tests that {@link CurrentPageVerifier} verification fails if the WebAPK navigates to a page
     * which is outside the WebAPK scope.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testOutsideScopeSameOrigin() {
        String page = mTestServer.getURL("/chrome/test/data/android/customtabs/cct_header.html");
        String pageOutsideScopeSameOrigin =
                mTestServer.getURL("/chrome/test/data/android/simple.html");
        String expectedScope = mTestServer.getURL("/chrome/test/data/android/customtabs/");
        WebappActivity webappActivity = launchWebApk(page);
        assertEquals(expectedScope, getWebappExtras(webappActivity).scopeUrl);

        mActivityTestRule.loadUrl(pageOutsideScopeSameOrigin);
        assertEquals(VerificationStatus.FAILURE, getCurrentPageVerifierStatus());
    }

    private WebappExtras getWebappExtras(WebappActivity activity) {
        return activity.getIntentDataProvider().getWebappExtras();
    }
}
