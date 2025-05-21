// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Intent;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.external_intents.ExternalIntentsFeatures;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Tests for InterceptNavigationDelegateCustomTabTest */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This test interacts with activities startup")
public class InterceptNavigationDelegateCustomTabTest {
    @ClassRule
    public static CustomTabActivityTestRule sCustomTabActivityTestRule =
            new CustomTabActivityTestRule();

    private static final String BASE_PAGE = "/chrome/test/data/navigation_interception/";
    private static final String NAVIGATION_TO_AUXILIARY_TAB =
            BASE_PAGE + "navigation_to_auxiliary_tab.html";
    private static final String NAVIGATION_TO_TOP_LEVEL_TAB =
            BASE_PAGE + "navigation_to_top_level_tab.html";
    private static final String TWA_PACKAGE_NAME = "com.foo.bar";
    private static final long DEFAULT_MAX_TIME_TO_WAIT_IN_MS = 3000;

    private EmbeddedTestServer mTestServer;

    private void launchTwa(String twaPackageName, String url) throws TimeoutException {
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        TrustedWebActivityTestUtil.spoofVerification(twaPackageName, url);
        TrustedWebActivityTestUtil.createSession(intent, twaPackageName);
        sCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Before
    public void setUp() throws Exception {
        InterceptNavigationDelegateClientImpl.setIsDesktopWindowingModeForTesting(true);
        sCustomTabActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);

        mTestServer = sCustomTabActivityTestRule.getTestServer();
    }

    private ChromeActivity launchTwaAndClick(String url) throws TimeoutException {
        launchTwa(TWA_PACKAGE_NAME, url);
        CustomTabActivity activity = sCustomTabActivityTestRule.getActivity();

        Assert.assertTrue(activity.getActivityTab().isTabInPWA());
        Assert.assertFalse(activity.getActivityTab().getWebContents().hasOpener());

        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.STARTED,
                        () -> TouchCommon.singleClickView(activity.getActivityTab().getView()));

        ApplicationTestUtils.waitForActivityState(newActivity, Stage.RESUMED);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(newActivity.getActivityTab(), Matchers.notNullValue());
                },
                DEFAULT_MAX_TIME_TO_WAIT_IN_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return newActivity;
    }

    @Test
    @EnableFeatures(ExternalIntentsFeatures.NAVIGATION_CAPTURE_REFACTOR_ANDROID_NAME)
    @MediumTest
    public void testAuxiliaryNavigationWasReparented() throws Exception {
        ChromeActivity newActivity =
                launchTwaAndClick(mTestServer.getURL(NAVIGATION_TO_AUXILIARY_TAB));
        // The tab was opened in the browser
        Assert.assertFalse(newActivity.getActivityTab().isTabInPWA());
        Assert.assertTrue(newActivity.getActivityTab().getWebContents().hasOpener());
    }

    @Test
    @EnableFeatures(ExternalIntentsFeatures.REPARENT_TOP_LEVEL_NAVIGATION_FROM_PWA_NAME)
    @MediumTest
    public void testTopLevelNavigationWasReparented() throws Exception {
        ChromeActivity newActivity =
                launchTwaAndClick(mTestServer.getURL(NAVIGATION_TO_TOP_LEVEL_TAB));
        // The tab was opened in the browser
        Assert.assertFalse(newActivity.getActivityTab().isTabInPWA());
        Assert.assertFalse(newActivity.getActivityTab().getWebContents().hasOpener());
    }
}
