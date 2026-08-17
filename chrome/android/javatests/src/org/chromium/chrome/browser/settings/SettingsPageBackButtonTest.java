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
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;

import static org.chromium.base.test.util.Batch.PER_CLASS;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;

/**
 * Integration tests focusing exclusively on back button navigation behavior in {@link
 * SettingsPage}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
@Restriction(DeviceFormFactor.DESKTOP)
public class SettingsPageBackButtonTest {
    private static final long TIMEOUT_MS = 20000L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private Fragment getDetailFragment() {
        var activity = mActivityTestRule.getActivity();
        Criteria.checkThat("Activity should not be null", activity, notNullValue());
        var hostFragment = SettingsHostFragment.get(activity);
        if (hostFragment != null) {
            return hostFragment.getMainFragment();
        }
        return findDetailFragmentInManager(activity.getSupportFragmentManager());
    }

    private Fragment findDetailFragmentInManager(FragmentManager fm) {
        Criteria.checkThat("FragmentManager should not be null", fm, notNullValue());
        Fragment detail = fm.findFragmentById(R.id.preferences_detail);
        if (detail != null) return detail;
        for (Fragment f : fm.getFragments()) {
            if (f.isAdded()) {
                Fragment childDetail = findDetailFragmentInManager(f.getChildFragmentManager());
                if (childDetail != null) return childDetail;
            }
        }
        return null;
    }

    private static List<View> findViewsWithId(View root, int id) {
        List<View> outViews = new ArrayList<>();
        if (root != null) {
            findViewsWithIdHelper(root, id, outViews);
        }
        return outViews;
    }

    private static void findViewsWithIdHelper(View root, int id, List<View> outViews) {
        if (root.getId() == id) {
            outViews.add(root);
        }
        if (root instanceof ViewGroup viewGroup) {
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                findViewsWithIdHelper(viewGroup.getChildAt(i), id, outViews);
            }
        }
    }

    /**
     * Performs back navigation using the detailed pane title back button and waits for the
     * specified previous fragment to be loaded and resumed in the detail pane.
     *
     * @param expectedPreviousFragmentClass The class of the fragment expected to be loaded after
     *     navigating back.
     */
    private void performBackNavigation(Class<?> expectedPreviousFragmentClass) {
        // Verify that there is exactly one back button with R.id.back_button in the settings title
        // container and that it is shown. Scoping to R.id.settings_title_in_detailed_pane avoids
        // ambiguity with other back buttons in the activity hierarchy (such as the main toolbar).
        // Once shown, invoke performClick() directly on the UI thread. Using performClick()
        // instead of Espresso touch click() prevents click events from being swallowed or
        // cancelled by parent HorizontalScrollView animations during breadcrumb title updates.
        CriteriaHelper.pollUiThread(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    Criteria.checkThat(activity, notNullValue());
                    View titleContainer =
                            activity.findViewById(R.id.settings_title_in_detailed_pane);
                    Criteria.checkThat(titleContainer, notNullValue());
                    List<View> backButtons = findViewsWithId(titleContainer, R.id.back_button);
                    Criteria.checkThat(
                            "Expected exactly one view with R.id.back_button in title container",
                            backButtons.size(),
                            is(1));
                    View backButton = backButtons.get(0);
                    Criteria.checkThat(backButton, instanceOf(ImageButton.class));
                    Criteria.checkThat(backButton.isShown(), is(true));
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    Criteria.checkThat("Activity should not be null", activity, notNullValue());
                    View titleContainer =
                            activity.findViewById(R.id.settings_title_in_detailed_pane);
                    Criteria.checkThat(
                            "Title container should not be null", titleContainer, notNullValue());
                    List<View> backButtons = findViewsWithId(titleContainer, R.id.back_button);
                    Criteria.checkThat(
                            "Expected exactly one view with R.id.back_button, but found "
                                    + backButtons.size(),
                            backButtons.size(),
                            is(1));
                    ImageButton backButton = (ImageButton) backButtons.get(0);
                    backButton.performClick();
                });

        // Wait for the expected previous fragment to be loaded and resumed in the detail pane.
        CriteriaHelper.pollUiThread(
                () -> {
                    Fragment fragment = getDetailFragment();
                    Criteria.checkThat(
                            expectedPreviousFragmentClass.getSimpleName()
                                    + " should be loaded in detail pane",
                            fragment,
                            instanceOf(expectedPreviousFragmentClass));
                    Criteria.checkThat(
                            expectedPreviousFragmentClass.getSimpleName() + " should be resumed",
                            fragment != null && fragment.isResumed(),
                            is(true));
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Verify whether back button navigation works by itself under default settings. */
    @Test
    @MediumTest
    public void testBackButton_defaultSettings() {
        mActivityTestRule.loadUrl(UrlConstants.SETTINGS_URL);

        // 1. Click "Privacy and security" in main settings left pane
        var mainSettingsListMatcher =
                allOf(withId(R.id.recycler_view), isDescendantOfA(withId(R.id.preferences_header)));
        var privacySecurityItemMatcher =
                allOf(
                        withText(R.string.prefs_privacy_security),
                        isDescendantOfA(mainSettingsListMatcher));
        onViewWaiting(mainSettingsListMatcher)
                .perform(scrollTo(hasDescendant(withText(R.string.prefs_privacy_security))));
        onViewWaiting(privacySecurityItemMatcher).perform(click());

        // 1a. Wait for PrivacySettings to be loaded in the detail pane
        String privacySettings = PrivacySettings.class.getSimpleName();
        CriteriaHelper.pollUiThread(
                () -> {
                    Fragment fragment = getDetailFragment();
                    Criteria.checkThat(
                            privacySettings + " should be loaded in detail pane",
                            fragment,
                            instanceOf(PrivacySettings.class));
                    Criteria.checkThat(
                            privacySettings + " should be resumed",
                            fragment != null && fragment.isResumed(),
                            is(true));
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        // 2. Click "Delete browsing data" pane to push a sub-fragment.
        var detailSettingsListMatcher =
                allOf(
                        withId(R.id.recycler_view),
                        isDescendantOfA(withId(R.id.preferences_detail_pane)));
        var clearBrowsingDataPreferenceMatcher =
                allOf(
                        withText(R.string.clear_browsing_data_title),
                        isDescendantOfA(withId(R.id.preferences_detail)));
        onViewWaiting(detailSettingsListMatcher)
                .perform(scrollTo(hasDescendant(withText(R.string.clear_browsing_data_title))));
        onViewWaiting(clearBrowsingDataPreferenceMatcher).perform(click());

        // 2a. Wait for ClearBrowsingDataFragment to be loaded in the detail pane
        String clearBrowsingData = ClearBrowsingDataFragment.class.getSimpleName();
        CriteriaHelper.pollUiThread(
                () -> {
                    Fragment fragment = getDetailFragment();
                    Criteria.checkThat(
                            clearBrowsingData + " should be loaded in detail pane",
                            fragment,
                            instanceOf(ClearBrowsingDataFragment.class));
                    Criteria.checkThat(
                            clearBrowsingData + " should be resumed",
                            fragment != null && fragment.isResumed(),
                            is(true));
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        // 3. Verify sub-fragment (Delete browsing data) has been navigated to by checking:
        // - The title "Delete browsing data" is displayed in the breadcrumb trail.
        // - The suboption "Cookies and site data" is displayed in the fragment list.
        // - The "Delete browsing data" preference button inside the fragment list is gone.
        var clearBrowsingDataBreadcrumbMatcher =
                allOf(
                        withText(R.string.clear_browsing_data_title),
                        isDescendantOfA(withId(R.id.settings_title_in_detailed_pane)));
        var cookiesAndSiteDataOptionMatcher = withText(R.string.clear_cookies_and_site_data_title);

        onViewWaiting(clearBrowsingDataBreadcrumbMatcher).check(matches(isDisplayed()));
        onViewWaiting(cookiesAndSiteDataOptionMatcher).check(matches(isDisplayed()));
        onView(clearBrowsingDataPreferenceMatcher).check(doesNotExist());

        // 4. Click back button in title container and 5. Verify back navigation returned to
        // "Privacy and security".
        performBackNavigation(PrivacySettings.class);

        // 5. Verify inverse state after navigating back to "Privacy and security":
        // - The "Delete browsing data" preference button inside the fragment list is present again.
        // - The "Delete browsing data" breadcrumb title is gone.
        // - The suboption "Cookies and site data" is gone.
        onViewWaiting(clearBrowsingDataPreferenceMatcher).check(matches(isDisplayed()));
        onView(clearBrowsingDataBreadcrumbMatcher).check(doesNotExist());
        onView(cookiesAndSiteDataOptionMatcher).check(doesNotExist());
    }
}
