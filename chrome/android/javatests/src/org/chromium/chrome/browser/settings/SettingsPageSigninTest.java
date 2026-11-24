// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.base.test.transit.ViewFinder.waitForView;

import android.content.res.Configuration;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.Arrays;
import java.util.List;

/**
 * Integration tests for sign-in / sign-out behavior in {@link SettingsPage}.
 *
 * <p>TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2 launch.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@EnableFeatures({
    ChromeFeatureList.SETTINGS_IN_TAB,
    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
})
public class SettingsPageSigninTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParameters =
            Arrays.asList(
                    new ParameterSet().value(false).name("withAccountManagerFacadeSource"),
                    new ParameterSet().value(true).name("withIdentityManagerSource"));

    private final boolean mIsIdentityManagerSourceOfAccounts;

    private final SigninTestRule mSigninTestRule = SigninTestRule.createWithCleanups();
    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    public SettingsPageSigninTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2,
                mIsIdentityManagerSourceOfAccounts);
        mActivityTestRule.startMainActivityOnBlankPage();

        // Skip the tests on landscape devices. Rotating to portrait may letterbox the activity on
        // landscape-oriented devices, causing Android's letterbox education dialog to intercept
        // clicks, or rotation to fail (e.g. on automotive).
        var activity = mActivityTestRule.getActivity();
        boolean isLandscape =
                activity.getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE;
        Assume.assumeFalse(
                "Rotating to portrait letterboxes the activity on landscape-oriented devices,"
                        + " causing Android's letterbox education dialog to intercept clicks.",
                isLandscape);
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSignOutInSingleColumnThenTransitionToMultiColumn() {
        // Sign in.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Ensure starting in portrait (usually single-column mode on tablet).
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Wait for settings page to load.
        waitForView(withText(R.string.search_engine_settings));

        // Skip test if portrait mode happens to be wide enough for two-column mode.
        Assume.assumeTrue("Test requires single-column mode in portrait.", isSingleColumn());

        // Click on Account preference in MainSettings header pane to open ManageSyncSettings.
        var headerRecyclerViewMatcher =
                allOf(
                        withId(R.id.recycler_view),
                        isDescendantOfA(withId(R.id.preferences_header)),
                        hasDescendant(withText(TestAccounts.ACCOUNT1.getEmail())));
        waitForView(headerRecyclerViewMatcher)
                .performViewAction(
                        scrollTo(hasDescendant(withText(TestAccounts.ACCOUNT1.getEmail()))));
        var headerAccountMatcher =
                allOf(
                        isDescendantOfA(withId(R.id.preferences_header)),
                        withText(TestAccounts.ACCOUNT1.getEmail()));
        waitForView(headerAccountMatcher).click();

        // Verify Account settings (ManageSyncSettings) is displayed.
        waitForView(withText(R.string.account_settings_title));

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
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        ensureTwoColumnMode();

        // In two-column mode, verify that the detail pane shows the default Google Services
        // settings and NOT the stale Account/ManageSyncSettings fragment.
        waitForView(withText(R.string.allow_chrome_signin_title));
        onView(withText(R.string.account_settings_title)).check(doesNotExist());
    }

    /**
     * Tests that removing an account from the device while viewing the account subpage in
     * single-column mode on a tablet dismisses the subpage and navigates back to MainSettings.
     */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testRemoveAccountInSingleColumnRemovesDetailFragment() {
        // Sign in.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Ensure starting in portrait (usually single-column mode on tablet).
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Wait for settings page to load.
        waitForView(withText(R.string.search_engine_settings));

        // Skip test if portrait mode happens to be wide enough for two-column mode.
        Assume.assumeTrue("Test requires single-column mode in portrait.", isSingleColumn());

        // Click on Account preference in MainSettings header pane to open ManageSyncSettings.
        var headerRecyclerViewMatcher =
                allOf(
                        withId(R.id.recycler_view),
                        isDescendantOfA(withId(R.id.preferences_header)),
                        hasDescendant(withText(TestAccounts.ACCOUNT1.getEmail())));
        waitForView(headerRecyclerViewMatcher)
                .performViewAction(
                        scrollTo(hasDescendant(withText(TestAccounts.ACCOUNT1.getEmail()))));
        var headerAccountMatcher =
                allOf(
                        isDescendantOfA(withId(R.id.preferences_header)),
                        withText(TestAccounts.ACCOUNT1.getEmail()));
        waitForView(headerAccountMatcher).click();

        // Verify Account settings (ManageSyncSettings) is displayed.
        waitForView(withText(R.string.account_settings_title));

        // Remove the account from device.
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        // Verify that after removing the account, the detail fragment is removed and settings
        // returns to MainSettings.
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

        // Verify MainSettings header pane is visible, SignInPreference updated to the signed-out
        // state, and Account settings is not displayed.
        waitForView(withText(R.string.search_engine_settings));
        waitForView(
                        allOf(
                                withId(R.id.recycler_view),
                                isDescendantOfA(withId(R.id.preferences_header))))
                .performViewAction(
                        scrollTo(hasDescendant(withText(R.string.signin_settings_title))));
        onView(
                        allOf(
                                isDescendantOfA(withId(R.id.preferences_header)),
                                withText(R.string.signin_settings_title)))
                .check(matches(isDisplayed()));
        onView(withText(TestAccounts.ACCOUNT1.getEmail())).check(doesNotExist());
        onView(allOf(withText(R.string.account_settings_title), isDisplayed()))
                .check(doesNotExist());
    }

    private void ensureActivityOrientation(int orientation) {
        var activity = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(activity, orientation);
        CriteriaHelper.pollUiThread(
                () -> {
                    View decorView = activity.getWindow().getDecorView();
                    return orientation == Configuration.ORIENTATION_LANDSCAPE
                            ? decorView.getWidth() > decorView.getHeight()
                            : decorView.getHeight() > decorView.getWidth();
                },
                "Window should be laid out in the target orientation.");
    }

    private void ensureTwoColumnMode() {
        CriteriaHelper.pollUiThread(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    return hostFragment != null && hostFragment.isTwoColumnSettingsVisible();
                },
                "Settings should be shown in two-column mode.");
    }

    private boolean isSingleColumn() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    if (hostFragment == null) return false;
                    var activeFragment = hostFragment.getActiveFragment();
                    if (activeFragment instanceof MultiColumnSettings multiColumn) {
                        return !multiColumn.isTwoColumn();
                    }
                    return false;
                });
    }
}
