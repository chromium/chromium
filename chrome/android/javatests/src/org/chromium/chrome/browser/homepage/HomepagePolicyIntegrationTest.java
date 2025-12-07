// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import static org.junit.Assert.assertEquals;

import android.content.Intent;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomepageLocationType;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

/**
 * Integration test for {@link HomepagePolicyManager}. Checking if enabling HomepageLocation policy
 * will reflect the expected behaviors for {@link HomepageSettings} and home button.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Policies.Add({
    @Policies.Item(key = "HomepageLocation", string = HomepagePolicyIntegrationTest.TEST_URL)
})
public class HomepagePolicyIntegrationTest {
    public static final String TEST_URL = "http://127.0.0.1:8000/foo.html";
    public static final String GOOGLE_HTML = "/chrome/test/data/android/google.html";

    private static final String METRICS_HOMEPAGE_LOCATION_TYPE = "Settings.Homepage.LocationType";

    private EmbeddedTestServer mTestServer;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();
    private RegularNewTabPageStation mPage;

    @Before
    public void setUp() {
        // Give some user pref setting, simulate user that have their customized preference.
        // Use shared preference manager, not to change the order object created in tests.
        mHomepageTestRule.useCustomizedHomepageForTest(GOOGLE_HTML);

        mPage = mActivityTestRule.startFromLauncherAtNtp();

        mTestServer = mActivityTestRule.getTestServer();
    }

    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testStartUpPage() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "HomepageLocation Policy should be enforced",
                                HomepagePolicyManager.isHomepageLocationManaged()));

        // The first time when the page starts, the homepage is fetched from shared preference
        // So the homepage policy is not enforced yet at this point.
        // Instead, we verify the shared preference to see if right policy URL were stored.
        String homepageGurlSerialized =
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, "");
        GURL homepageGurl = GURL.deserialize(homepageGurlSerialized);
        assertEquals(
                "URL stored in shared preference should be the same as policy setting",
                TEST_URL,
                homepageGurl.getSpec());

        // METRICS_HOMEPAGE_LOCATION_TYPE is recorded once in deferred start up tasks.
        assertEquals(
                "Settings.Homepage.LocationType should record POLICY_OTHER once.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_LOCATION_TYPE, HomepageLocationType.POLICY_OTHER));

        // Start the page again. This time, the homepage should be set to what policy is.
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Create an intent to launch a new ChromeTabbedActivity.
        Intent intent = new Intent();
        intent.setClass(activity, ChromeTabbedActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Close all tabs so the new activity will create another initial tab with current homepage.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabClosureParams params =
                            TabClosureParams.closeAllTabs().uponExit(false).build();
                    TabModelSelector selector = activity.getTabModelSelector();
                    selector.getModel(false)
                            .getTabRemover()
                            .closeTabs(params, /* allowDialog= */ false);
                    selector.getModel(true)
                            .getTabRemover()
                            .closeTabs(params, /* allowDialog= */ false);
                });

        mActivityTestRule.finishActivity();

        // Start a new ChromeActivity.
        WebPageStation pageAfterRecreate =
                mActivityTestRule
                        .startWithIntentTo(intent)
                        .arriveAt(
                                WebPageStation.newBuilder()
                                        .withEntryPoint()
                                        .withExpectedUrlSubstring(TEST_URL)
                                        .build());

        String urlAfterRecreate = ChromeTabUtils.getUrlStringOnUiThread(pageAfterRecreate.getTab());
        assertEquals("Start up page is not homepage", getHomepageUrlOnUiThread(), urlAfterRecreate);
        assertEquals(
                "Start up homepage should be the same as the policy setting",
                TEST_URL,
                urlAfterRecreate);
    }

    @DisabledTest(message = "crbug.com/415374799")
    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testHomeButton() throws Exception {
        String anotherUrl = mTestServer.getURL(GOOGLE_HTML);
        new TabLoadObserver(mActivityTestRule.getActivity().getActivityTab())
                .fullyLoadUrl(anotherUrl);

        Assert.assertNotEquals(
                "Did not switch to a different URL",
                TEST_URL,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        CriteriaHelper.pollUiThread(
                () -> {
                    ToolbarManager toolbarManager =
                            mActivityTestRule.getActivity().getToolbarManager();
                    Criteria.checkThat(toolbarManager, Matchers.notNullValue());

                    View homeButton =
                            mActivityTestRule.getActivity().findViewById(R.id.home_button);
                    Criteria.checkThat(homeButton, Matchers.notNullValue());
                    Criteria.checkThat(
                            "Home Button should be visible",
                            homeButton.getVisibility(),
                            Matchers.is(View.VISIBLE));
                    homeButton.performLongClick();

                    Criteria.checkThat(
                            "Home button long click should not generate menu.",
                            toolbarManager.getHomeButtonCoordinatorForTesting().getMenuForTesting(),
                            Matchers.nullValue());
                });

        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(),
                TEST_URL,
                () -> {
                    View homeButton =
                            mActivityTestRule.getActivity().findViewById(R.id.home_button);
                    TouchCommon.singleClickView(homeButton);
                });

        assertEquals(
                "After clicking HomeButton, URL should be back to Homepage",
                TEST_URL,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }

    private String getHomepageUrlOnUiThread() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        HomepageManager.getInstance()
                                .getHomepageGurl(/* isIncognito= */ false)
                                .getSpec());
    }
}
