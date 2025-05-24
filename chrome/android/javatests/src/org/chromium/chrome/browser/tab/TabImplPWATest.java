// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for the {@link TabImpl} class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class TabImplPWATest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";
    private static final String TWA_PACKAGE_NAME = "com.foo.bar";
    private static final long DEFAULT_MAX_TIME_TO_WAIT_IN_MS = 3000;

    @ClassRule
    public static CustomTabActivityTestRule sCustomTabActivityTestRule =
            new CustomTabActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        sCustomTabActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestServer = sCustomTabActivityTestRule.getTestServer();
    }

    private void launchTwa(String twaPackageName, String url) throws Exception {
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        TrustedWebActivityTestUtil.spoofVerification(twaPackageName, url);
        TrustedWebActivityTestUtil.createSession(intent, twaPackageName);
        sCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabIsInPWA() throws Exception {
        launchTwa(TWA_PACKAGE_NAME, mTestServer.getURL(TEST_PATH));

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            sCustomTabActivityTestRule.getActivity().getActivityTab(),
                            Matchers.notNullValue());
                },
                DEFAULT_MAX_TIME_TO_WAIT_IN_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        assertTrue(sCustomTabActivityTestRule.getActivity().getActivityTab().isTabInPWA());
        assertFalse(sCustomTabActivityTestRule.getActivity().getActivityTab().isTabInBrowser());
    }
}
