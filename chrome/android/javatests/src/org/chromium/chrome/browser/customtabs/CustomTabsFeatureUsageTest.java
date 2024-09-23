// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.customtabs.CustomTabsFeatureUsage.CustomTabsFeature;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Instrumentation tests for app menu, context menu, and toolbar of a {@link CustomTabActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "Some tests are Testing CCT start up behavior. "
                        + "Unit test conversion tracked in crbug.com/1217031")
public class CustomTabsFeatureUsageTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public IncognitoCustomTabActivityTestRule mIncognitoCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        Context appContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
    }

    private Activity startBlankUiTestActivity() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent emptyIntent = new Intent(context, BlankUiTestActivity.class);
        emptyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return InstrumentationRegistry.getInstrumentation().startActivitySync(emptyIntent);
    }

    private void assertHistogramEnumRecorded(
            @CustomTabsFeature int featureEnum, boolean umaRecorded) {
        Assert.assertEquals(
                String.format("CustomTabs Feature enum <%s> not recorded correctly:", featureEnum),
                umaRecorded ? 1 : 0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "CustomTabs.FeatureUsage", featureEnum));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.CCT_FEATURE_USAGE})
    public void testNormalFeatureUsage() throws Exception {
        Activity emptyActivity = startBlankUiTestActivity();
        Intent intent =
                CustomTabsIntentTestUtils.createCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mTestPage,
                        false,
                        builder -> {});
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        intent.setData(Uri.parse(mTestPage));

        CustomTabActivity cctActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.CREATED,
                        () -> emptyActivity.startActivity(intent));
        mCustomTabActivityTestRule.setActivity(cctActivity);
        mCustomTabActivityTestRule.waitForActivityCompletelyLoaded();

        assertHistogramEnumRecorded(CustomTabsFeature.CTF_SESSIONS, true);
        assertHistogramEnumRecorded(CustomTabsFeature.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
        assertHistogramEnumRecorded(CustomTabsFeature.CTF_PACKAGE_NAME, true);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.CCT_FEATURE_USAGE})
    public void testNormalFeatureUsageIncognito() throws Exception {
        startBlankUiTestActivity();
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mTestPage);
        mIncognitoCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        intent.setData(Uri.parse(mTestPage));

        /*
        TODO(donnd): why is this section needed in the test above but not here?
        CustomTabActivity cctActivity = ApplicationTestUtils.waitForActivityWithClass(
            CustomTabActivity.class, Stage.CREATED, () -> emptyActivity.startActivity(intent));
        mCustomTabActivityTestRule.setActivity(cctActivity);
        mCustomTabActivityTestRule.waitForActivityCompletelyLoaded();
        */

        assertHistogramEnumRecorded(CustomTabsFeature.CTF_SESSIONS, true);
        assertHistogramEnumRecorded(CustomTabsFeature.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        assertHistogramEnumRecorded(CustomTabsFeature.CTF_PACKAGE_NAME, true);
    }
}
