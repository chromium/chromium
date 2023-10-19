// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;

import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.common.ContentSwitches;

/** Tests the {@link CurrentPageVerifier} integration with PWAs added to the homescreen. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class AddToHomescreenCurrentPageVerifierTest {
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain().around(mActivityTestRule).around(mCertVerifierRule);

    @Before
    public void setUp() {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        Uri mapToUri =
                Uri.parse(mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
    }

    private void launchWebapp(String url) {
        Intent launchIntent = mActivityTestRule.createIntent();
        launchIntent.putExtra(WebappConstants.EXTRA_URL, url);
        mActivityTestRule.startWebappActivity(launchIntent);
    }

    private @VerificationStatus int getCurrentPageVerifierStatus() {
        WebappActivity webappActivity = mActivityTestRule.getActivity();
        return webappActivity.getComponent().resolveCurrentPageVerifier().getState().status;
    }

    /**
     * Tests that {@link CurrentPageVerifier} verification succeeds if the webapp navigates to a
     * page within the webapp origin.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testInScope() {
        String page = "https://foo.com/chrome/test/data/android/customtabs/cct_header.html";
        String otherPageInScope = "https://foo.com/chrome/test/data/android/simple.html";
        launchWebapp(page);

        mActivityTestRule.loadUrl(otherPageInScope);
        assertEquals(VerificationStatus.SUCCESS, getCurrentPageVerifierStatus());
    }

    /**
     * Tests that {@link CurrentPageVerifier} verification fails if the webapp navigates to a page
     * with a different origin than the webapp.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testDifferentOrigin() {
        String page = "https://foo.com/chrome/test/data/android/simple.html";
        String pageDifferentOrigin = "https://bar.com/chrome/test/data/android/simple.html";
        launchWebapp(page);

        mActivityTestRule.loadUrl(pageDifferentOrigin, /* secondsToWait= */ 10);
        assertEquals(VerificationStatus.FAILURE, getCurrentPageVerifierStatus());
    }
}
