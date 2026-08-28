// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import android.content.res.Configuration;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;

/** Integration tests for sign-in / sign-out behavior in {@link SettingsPage}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests sign-in and sign-out which mutates global account state")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class SettingsPageSigninTest {
    private final SigninTestRule mSigninTestRule = new SigninTestRule();
    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.ONLY_TABLET)
    public void testSignOutInSingleColumnThenTransitionToMultiColumn() {
        // Sign in.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Ensure starting in portrait (usually single-column mode on tablet).
        var activity = mActivityTestRule.getActivity();
        if (activity.getResources().getConfiguration().orientation
                != Configuration.ORIENTATION_PORTRAIT) {
            ActivityTestUtils.rotateActivityToOrientation(
                    activity, Configuration.ORIENTATION_PORTRAIT);
        }

        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Wait for settings page to load.
        onView(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

        // Skip test if portrait mode happens to be wide enough for two-column mode.
        var isSingleColumn =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var hostFragment =
                                    SettingsHostFragment.get(mActivityTestRule.getActivity());
                            if (hostFragment == null) return false;
                            var activeFragment = hostFragment.getActiveFragment();
                            if (activeFragment instanceof MultiColumnSettings multiColumn) {
                                return !multiColumn.isTwoColumn();
                            }
                            return false;
                        });
        Assume.assumeTrue("Test requires single-column mode in portrait.", isSingleColumn);

        // Click on Account preference in MainSettings header pane to open ManageSyncSettings.
        var headerRecyclerViewMatcher =
                allOf(withId(R.id.recycler_view), isDescendantOfA(withId(R.id.preferences_header)));
        onView(headerRecyclerViewMatcher)
                .perform(scrollTo(hasDescendant(withText(TestAccounts.ACCOUNT1.getEmail()))));
        var headerAccountMatcher =
                allOf(
                        isDescendantOfA(withId(R.id.preferences_header)),
                        withText(TestAccounts.ACCOUNT1.getEmail()));
        onView(headerAccountMatcher).perform(click());

        // Verify Account settings (ManageSyncSettings) is displayed.
        onView(allOf(withText(R.string.account_settings_title), isDisplayed()))
                .check(matches(isDisplayed()));

        // 1. Sign out while in single-column mode.
        mSigninTestRule.signOut();

        // Verify that after signing out, the detail fragment is removed.
        CriteriaHelper.pollUiThread(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    if (hostFragment == null) return false;
                    var activeFragment = hostFragment.getActiveFragment();
                    if (!(activeFragment instanceof MultiColumnSettings multiColumn)) return false;
                    var childFragmentManager = multiColumn.getChildFragmentManager();
                    var detailFragment =
                            childFragmentManager.findFragmentById(R.id.preferences_detail);
                    return detailFragment == null;
                });

        // 2. Rotate display to landscape (transitioning to two-column mode).
        activity = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(
                activity, Configuration.ORIENTATION_LANDSCAPE);

        // In two-column mode, verify that the detail pane shows the default Google Services
        // settings and NOT the stale Account/ManageSyncSettings fragment.
        onView(withText(R.string.allow_chrome_signin_title)).check(matches(isDisplayed()));
        onView(withText(R.string.account_settings_title)).check(doesNotExist());
    }
}
