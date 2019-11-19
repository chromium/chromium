// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.chrome.browser.TabsOpenedFromExternalAppTest.HTTP_REFERRER;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.network.mojom.ReferrerPolicy;

/**
 * Test the behavior of tabs when opening an HTTPS URL from an external app.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})

public class HTTPSTabsOpenedFromExternalAppTest {
    @Rule
    public ChromeActivityTestRule<? extends ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule(ChromeTabbedActivity.class);

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Tests that an http:// referrer is not stripped in case of https:// navigation with
     * default Policy.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpReferrerHttpsNavigationsPolicyDefault() {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getContext(), ServerCertificate.CERT_OK);
        try {
            String url = mTestServer.getURL("/chrome/test/data/android/about.html");
            TabsOpenedFromExternalAppTest.launchAndVerifyReferrerWithPolicy(
                    url, mActivityTestRule, ReferrerPolicy.DEFAULT, HTTP_REFERRER, HTTP_REFERRER);
        } finally {
            if (mTestServer != null) mTestServer.stopAndDestroyServer();
        }
    }
}
