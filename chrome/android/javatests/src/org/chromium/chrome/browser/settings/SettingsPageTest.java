// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/** Integration tests for {@link SettingsPage} inside a native tab. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class SettingsPageTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    public void testOpenSettingsAndClickPreference() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Verify the settings page loads by checking for a top-level preference item.
        onView(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

        // Click on a setting in the column on the left (e.g., Privacy and security).
        // Check the descendent because multi-column settings contains two recycler views.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.prefs_privacy_security)));
        onView(matcher).perform(scrollTo(hasDescendant(withText(R.string.prefs_privacy_security))));
        onView(withText(R.string.prefs_privacy_security)).perform(click());

        // Verify the detail page loads by checking an item in the detail preference screen.
        onView(withText(R.string.clear_browsing_data_title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSearchBoxMarginsOnContainerResized() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        onView(withId(R.id.search_box)).check(matches(isDisplayed()));

        // Measure initial container width before resizing. It may differ by emulator environment.
        final int originalWidth =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            View appBar =
                                    mActivityTestRule
                                            .getActivity()
                                            .findViewById(R.id.app_bar_layout);
                            assertNotNull(appBar);
                            return appBar.getWidth();
                        });

        // 1. Simulate opening the side panel (shrinking container width to narrow).
        final int narrowWidth = Math.min(800, originalWidth - 200);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    View settingsActivity = activity.findViewById(R.id.settings_activity);
                    assertNotNull(settingsActivity);
                    View parent = (View) settingsActivity.getParent();
                    var lp = parent.getLayoutParams();
                    lp.width = narrowWidth;
                    parent.setLayoutParams(lp);
                    ViewUtils.requestLayout(
                            parent, "SettingsPageTest.testSearchBoxMarginsOnContainerResized");
                });

        int minPadding =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);

        // Verify the search box stays on screen and matches container margins. Poll because
        // layout is asynchronous.
        CriteriaHelper.pollUiThread(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    View searchBox = activity.findViewById(R.id.search_box);
                    View appBar = activity.findViewById(R.id.app_bar_layout);
                    if (searchBox == null || appBar == null) return false;
                    if (appBar.getWidth() > narrowWidth) return false;
                    if (searchBox.getVisibility() != View.VISIBLE) return false;

                    int wideMinWidthPx =
                            ViewUtils.dpToPx(activity.getResources().getDisplayMetrics(), 600);
                    int expectedNarrowMargin =
                            Math.max(minPadding, (appBar.getWidth() - wideMinWidthPx) / 2);
                    boolean isOnWideScreen = expectedNarrowMargin > minPadding;
                    if (isOnWideScreen) {
                        int itemMargin =
                                activity.getResources()
                                        .getDimensionPixelSize(R.dimen.settings_item_margin);
                        expectedNarrowMargin += itemMargin;
                    }

                    // Search box should not be truncated on right or left and should
                    // have expected margins.
                    var lp = (ViewGroup.MarginLayoutParams) searchBox.getLayoutParams();
                    return searchBox.getRight() <= appBar.getWidth()
                            && searchBox.getLeft() >= 0
                            && lp.getMarginStart() == expectedNarrowMargin
                            && lp.getMarginEnd() == expectedNarrowMargin;
                });

        // 2. Simulate closing the side panel (restoring container width back to original).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    View settingsActivity = activity.findViewById(R.id.settings_activity);
                    assertNotNull(settingsActivity);
                    View parent = (View) settingsActivity.getParent();
                    var lp = parent.getLayoutParams();
                    lp.width = ViewGroup.LayoutParams.MATCH_PARENT;
                    parent.setLayoutParams(lp);
                    ViewUtils.requestLayout(
                            parent, "SettingsPageTest.testSearchBoxMarginsOnContainerResized");
                });

        // Verify the search box expands back to original/wide layout and is properly laid out.
        CriteriaHelper.pollUiThread(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    View searchBox = activity.findViewById(R.id.search_box);
                    View appBar = activity.findViewById(R.id.app_bar_layout);
                    if (searchBox == null || appBar == null) return false;
                    if (appBar.getWidth() <= narrowWidth + 100) return false;
                    if (searchBox.getVisibility() != View.VISIBLE) return false;

                    // Depending on emulator environment the expanded view may be multi-column
                    // or single column.
                    boolean isMultiColumn = searchBox.getParent() != appBar;
                    if (isMultiColumn) {
                        return searchBox.getWidth() > 0
                                && searchBox.getRight() <= appBar.getWidth()
                                && searchBox.getLeft() >= 0;
                    }

                    int wideMinWidthPx =
                            ViewUtils.dpToPx(activity.getResources().getDisplayMetrics(), 600);
                    int expectedExpandedMargin =
                            Math.max(minPadding, (appBar.getWidth() - wideMinWidthPx) / 2);
                    boolean isOnWideScreen = expectedExpandedMargin > minPadding;
                    if (isOnWideScreen) {
                        int itemMargin =
                                activity.getResources()
                                        .getDimensionPixelSize(R.dimen.settings_item_margin);
                        expectedExpandedMargin += itemMargin;
                    }

                    var lp = (ViewGroup.MarginLayoutParams) searchBox.getLayoutParams();
                    return searchBox.getRight() <= appBar.getWidth()
                            && searchBox.getLeft() >= 0
                            && lp.getMarginStart() == expectedExpandedMargin
                            && lp.getMarginEnd() == expectedExpandedMargin;
                });
    }
}
