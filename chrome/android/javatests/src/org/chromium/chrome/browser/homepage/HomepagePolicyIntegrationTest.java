// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import android.content.Intent;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomepageLocationType;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
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

    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    public SettingsActivityTestRule<HomepageSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(HomepageSettings.class);

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Before
    public void setUp() {
        // Give some user pref setting, simulate user that have their customized preference.
        // Use shared preference manager, not to change the order object created in tests.
        mHomepageTestRule.useCustomizedHomepageForTest(GOOGLE_HTML);

        mActivityTestRule.startMainActivityFromLauncher();

        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
    }

    @Test
    @MediumTest
    @Feature({"Homepage"})
    public void testStartUpPage() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "HomepageLocation Policy should be enforced",
                                HomepagePolicyManager.isHomepageManagedByPolicy()));

        // The first time when the page starts, the homepage is fetched from shared preference
        // So the homepage policy is not enforced yet at this point.
        // Instead, we verify the shared preference to see if right policy URL were stored.
        String homepageGurlSerialized =
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, "");
        GURL homepageGurl = GURL.deserialize(homepageGurlSerialized);
        Assert.assertEquals(
                "URL stored in shared preference should be the same as policy setting",
                TEST_URL,
                homepageGurl.getSpec());

        // METRICS_HOMEPAGE_LOCATION_TYPE is recorded once in deferred start up tasks.
        Assert.assertEquals(
                "Settings.Homepage.LocationType should record POLICY_OTHER once.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_LOCATION_TYPE, HomepageLocationType.POLICY_OTHER));

        // Start the page again. This time, the homepage should be set to what policy is.
        destroyAndRestartActivity();

        Assert.assertEquals(
                "Start up homepage should be the same as the policy setting",
                TEST_URL,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }

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

        Assert.assertEquals(
                "After clicking HomeButton, URL should be back to Homepage",
                TEST_URL,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }

    private void destroyAndRestartActivity() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Create an intent to launch a new ChromeTabbedActivity.
        Intent intent = new Intent();
        intent.setClass(activity, ChromeTabbedActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Close all tabs so the new activity will create another initial tab with current homepage.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getTabModelSelector().closeAllTabs();
                });

        activity.finish();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getStateForActivity(activity),
                            Matchers.is(ActivityState.DESTROYED));
                });

        // Start a new ChromeActivity.
        mActivityTestRule.startActivityCompletely(intent);
        Assert.assertEquals(
                "Start up page is not homepage",
                getHomepageUrlOnUiThread(),
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }

    private String getHomepageUrlOnUiThread() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().getHomepageGurl().getSpec());
    }
}
