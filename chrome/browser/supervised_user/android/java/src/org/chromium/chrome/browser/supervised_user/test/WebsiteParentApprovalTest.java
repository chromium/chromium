// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.test;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests the local website approval flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(
        {ChromeFeatureList.LOCAL_WEB_APPROVALS, ChromeFeatureList.WEB_FILTER_INTERSTITIAL_REFRESH})
public class WebsiteParentApprovalTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private String mTestPage;

    @Before
    public void setUp() throws TimeoutException {
        mTestServer = mTabbedActivityTestRule.getEmbeddedTestServerRule().getServer();
        mTestPage = mTestServer.getURL(TEST_PAGE);

        mTabbedActivityTestRule.startMainActivityWithURL(mTestPage);

        // Set up website filtering configuration.
        configureAllowOnlyCertainSites();
    }

    @Test
    @MediumTest
    public void parentApprovesLocally() {
        // Navigate to a blocked website.
        loadUrl(mTestPage);

        // Verify the interstitial screen is shown and click ask in person.
        clickAskInPerson();

        // Mock successful parent auth.

        clickAllow();

        checkTestPageLoaded();
    }

    // TODO(crbug.com/1340913): add test cases to cover rejection, failures, etc.

    private void configureAllowOnlyCertainSites() {
        // Set behaviour to BLOCK.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SupervisedUserSettingsBridge.setFilteringBehavior(
                    Profile.getLastUsedRegularProfile(), FilteringBehavior.BLOCK);
        });
    }

    /**
     * Loads a URL and checks that it loaded successfully.
     *
     * This does not guarantee that the actual content was displayed, as opposed to the blocked
     * website interstitial.
     */
    private void loadUrl(String url) {
        mTabbedActivityTestRule.loadUrl(mTestPage);
        // TODO: check the load was successful.
    }

    private void clickAskInPerson() {
        // See eg.
        // https://source.chromium.org/chromium/chromium/src/+/main:content/public/test/android/javatests/src/org/chromium/content_public/browser/test/util/JavaScriptUtils.java;l=117;drc=cfd951a304bb7a1e6424c614c094f5bc581ca8b7
        //
        // C++ tests:
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/supervised_user/supervised_user_url_filter_browsertest.cc;l=80;drc=cfd951a304bb7a1e6424c614c094f5bc581ca8b7
    }

    private void clickAllow() {
        // TODO(crbug.com/1340913): implement
        // Parent clicks approve.
        // Use UiAutomator?
    }

    private void clickDontAllow() {
        // TODO(crbug.com/1340913): implement
        // Parent clicks don't allow.
    }

    private void checkTestPageLoaded() {
        // TODO(crbug.com/1340913): implement a simple check on the presence of expected text in the
        // page.
    }
}
