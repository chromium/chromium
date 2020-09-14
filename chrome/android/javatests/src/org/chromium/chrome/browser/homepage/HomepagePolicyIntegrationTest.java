// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.preference.Preference;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomeButtonPreferenceState;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomepageLocationType;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Integration test for {@link HomepagePolicyManager}.
 * Checking if enabling HomepageLocation policy will reflect the expected behaviors for
 * {@link HomepageSettings} and {@link org.chromium.chrome.browser.toolbar.HomeButton}
 */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY)
@Policies.Add({
    @Policies.Item(key = "HomepageLocation", string = HomepagePolicyIntegrationTest.TEST_URL)
})
public class HomepagePolicyIntegrationTest {
    // clang-format on
    public static final String TEST_URL = "http://127.0.0.1:8000/foo.html";
    public static final String GOOGLE_HTML = "/chrome/test/data/android/google.html";

    private static final String METRICS_HOME_BUTTON_STATE_ENUM =
            "Settings.ShowHomeButtonPreferenceStateManaged";
    private static final String METRICS_HOMEPAGE_IS_CUSTOMIZED = "Settings.HomePageIsCustomized";
    private static final String METRICS_HOMEPAGE_LOCATION_TYPE = "Settings.Homepage.LocationType";

    private EmbeddedTestServer mTestServer;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public SettingsActivityTestRule<HomepageSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(HomepageSettings.class);

    @Rule
    public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Before
    public void setUp() {

        // Give some user pref setting, simulate user that have their customized preference.
        // Use shared preference manager, not to change the order object created in tests.
        mHomepageTestRule.useCustomizedHomepageForTest(GOOGLE_HTML);

        mActivityTestRule.startMainActivityFromLauncher();

        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    @Feature({"Homepage"})
    @FlakyTest(message = "crbug.com/1075804")
    public void testStartUpPage() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue("HomepageLocation Policy should be enforced",
                                HomepagePolicyManager.isHomepageManagedByPolicy()));

        // The first time when the page starts, the homepage is fetched from shared preference
        // So the homepage policy is not enforced yet at this point.
        // Instead, we verify the shared preference to see if right policy URL were stored.
        Assert.assertEquals("URL stored in shared preference should be the same as policy setting",
                TEST_URL,
                SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY, ""));

        // METRICS_HOMEPAGE_IS_CUSTOMIZED Should be collected twice it is called in:
        // 1. ProcessInitializationHandler#handleDeferredStartupTasksInitialization;
        // 2. HomepageManager#onHomepagePolicyUpdate, which will be called when native initialized.
        Assert.assertEquals(
                "Settings.HomepageIsCustomized should be recorded twice when policy enabled", 2,
                RecordHistogram.getHistogramTotalCountForTesting(METRICS_HOMEPAGE_IS_CUSTOMIZED));

        // METRICS_HOME_BUTTON_STATE_ENUM should be collected once in deferred start up tasks.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOME_BUTTON_STATE_ENUM, HomeButtonPreferenceState.MANAGED_ENABLED));

        // METRICS_HOMEPAGE_LOCATION_TYPE is recorded once in deferred start up tasks.
        Assert.assertEquals("Settings.Homepage.LocationType should record POLICY_OTHER once.", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_LOCATION_TYPE, HomepageLocationType.POLICY_OTHER));

        // Start the page again. This time, the homepage should be set to what policy is.
        destroyAndRestartActivity();

        Assert.assertEquals("Start up homepage should be the same as the policy setting", TEST_URL,
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

        Assert.assertNotEquals("Did not switch to a different URL", TEST_URL,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), TEST_URL, () -> {
                    ToolbarManager toolbarManager =
                            mActivityTestRule.getActivity().getToolbarManager();
                    if (toolbarManager != null) {
                        HomeButton homeButton = toolbarManager.getHomeButtonForTesting();
                        if (homeButton != null) {
                            Assert.assertEquals("Home Button should be visible", View.VISIBLE,
                                    homeButton.getVisibility());

                            // Context menu is disabled by checking long clickable
                            Assert.assertFalse("Long press for home button should be disabled",
                                    homeButton.isLongClickable());
                            TouchCommon.singleClickView(homeButton);
                        }
                    }
                });

        Assert.assertEquals("After clicking HomeButton, URL should be back to Homepage", TEST_URL,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }

    @Test
    @MediumTest
    @Feature({"Homepage"})
    @DisableFeatures(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION)
    public void testHomepagePreference() {
        // Launch homepage preference page
        mSettingsActivityTestRule.startSettingsActivity();
        HomepageSettings fragment = mSettingsActivityTestRule.getFragment();
        Assert.assertNotNull(fragment);

        ChromeSwitchPreference homepageSwitch = (ChromeSwitchPreference) fragment.findPreference(
                HomepageSettings.PREF_HOMEPAGE_SWITCH);

        Preference homepageEdit = fragment.findPreference(HomepageSettings.PREF_HOMEPAGE_EDIT);

        Assert.assertNotNull(homepageSwitch);
        Assert.assertNotNull(homepageEdit);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Switch should be disabled", homepageSwitch.isEnabled());
            Assert.assertTrue("Switch should be checked", homepageSwitch.isChecked());

            Assert.assertFalse(
                    "Homepage Edit should be disabled under policy", homepageEdit.isEnabled());
            Assert.assertEquals("Homepage Url should be set as policy setting",
                    homepageEdit.getSummary(), TEST_URL);
        });
    }

    private void destroyAndRestartActivity() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Create an intent to launch a new ChromeTabbedActivity.
        Intent intent = new Intent();
        intent.setClass(activity, ChromeTabbedActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Close all tabs so the new activity will create another initial tab with current homepage.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getTabModelSelector().closeAllTabs(); });

        activity.finish();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(ApplicationStatus.getStateForActivity(activity),
                    Matchers.is(ActivityState.DESTROYED));
        });

        // Start a new ChromeActivity.
        mActivityTestRule.startActivityCompletely(intent);
        Assert.assertEquals("Start up page is not homepage", HomepageManager.getHomepageUri(),
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }
}
