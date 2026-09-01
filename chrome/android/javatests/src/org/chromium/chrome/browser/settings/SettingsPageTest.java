// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.closeSoftKeyboard;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isFocused;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.util.Batch.PER_CLASS;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assume;
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
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.test.util.DeviceRestriction;

/**
 * Integration tests for {@link SettingsPage} inside a native tab. Most tests use a mix of onView()
 * and onViewWaiting() depending on whether they need to wait for a fragment or view to load.
 */
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

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    public void testOpenSettingsAndClickPreference() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Verify the settings page loads by checking for a top-level preference item.
        onViewWaiting(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

        // Click on a setting in the column on the left (e.g., Privacy and security).
        // Check the descendent because multi-column settings contains two recycler views.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.prefs_privacy_security)));
        onViewWaiting(matcher)
                .perform(scrollTo(hasDescendant(withText(R.string.prefs_privacy_security))));
        onViewWaiting(withText(R.string.prefs_privacy_security)).perform(click());

        // Verify the detail page loads by checking an item in the detail preference screen.
        onViewWaiting(withText(R.string.clear_browsing_data_title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSearchBoxMarginsOnContainerResized() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));

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

    @Test
    @MediumTest
    public void testThemeSwitchRestoresSettingsPageAndDetailFragment() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Verify MainSettings header fragment is displayed by checking for Search engine
        // preference.
        onViewWaiting(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

        // Click on "Search engine" in MainSettings header pane to open SearchEngineSettings detail
        // fragment.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.search_engine_settings)));
        onViewWaiting(matcher)
                .perform(scrollTo(hasDescendant(withText(R.string.search_engine_settings))));
        onViewWaiting(withText(R.string.search_engine_settings)).perform(click());

        // Simulate theme switch / activity recreation.
        mActivityTestRule.recreateActivity();

        // 1. Verify Toolbar/Action Bar is restored and displayed.
        onViewWaiting(withId(R.id.action_bar)).check(matches(isDisplayed()));

        // 2. Verify MainSettings header pane is restored (checking top-level preference item).
        onViewWaiting(withText(R.string.prefs_privacy_security)).check(matches(isDisplayed()));

        // 3. Verify SearchEngineSettings detail pane fragment is restored and displayed.
        onViewWaiting(withText("Microsoft Bing")).check(matches(isDisplayed()));
    }

    /** Regression test for https://crbug.com/535695748. */
    @Test
    @MediumTest
    public void testTwoSettingsTabsThemeSwitchRestoresDetailFragment() {
        // Tab 0: Open settings and navigate to Search engine detail fragment.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        onViewWaiting(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.search_engine_settings)));
        onViewWaiting(matcher)
                .perform(scrollTo(hasDescendant(withText(R.string.search_engine_settings))));
        onViewWaiting(withText(R.string.search_engine_settings)).perform(click());
        onViewWaiting(withText("Microsoft Bing")).check(matches(isDisplayed()));

        // Tab 1: Open a second settings tab at root MainSettings.
        mActivityTestRule.loadUrlInNewTab("chrome-native://settings/");
        onViewWaiting(allOf(withText(R.string.prefs_privacy_security), isDisplayed()))
                .check(matches(isDisplayed()));

        // Simulate theme switch / activity recreation.
        mActivityTestRule.recreateActivity();

        // Verify Tab 1 (active tab): Action bar and MainSettings header pane are restored.
        onViewWaiting(allOf(withId(R.id.action_bar), isDisplayed())).check(matches(isDisplayed()));
        onViewWaiting(allOf(withText(R.string.prefs_privacy_security), isDisplayed()))
                .check(matches(isDisplayed()));

        // Switch to Tab 0.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        // Verify Tab 0 (previously navigated tab): Action bar and SearchEngineSettings detail
        // fragment are restored.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        onView(allOf(withId(R.id.action_bar), isDisplayed()))
                                .check(matches(isDisplayed()));
                        onView(allOf(withText("Microsoft Bing"), isDisplayed()))
                                .check(matches(isDisplayed()));
                        return true;
                    } catch (AssertionError | Exception e) {
                        return false;
                    }
                });
    }

    @Test
    @MediumTest
    public void testAccessibilityPageZoomDoesNotShowPopup() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Click on "Accessibility" in MainSettings header pane.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.search_engine_settings)));
        onViewWaiting(matcher)
                .perform(scrollTo(hasDescendant(withText(R.string.prefs_accessibility))));
        onViewWaiting(withText(R.string.prefs_accessibility)).perform(click());

        // Verify the Accessibility preference screen is displayed.
        onViewWaiting(withText(R.string.page_zoom_title)).check(matches(isDisplayed()));

        // Verify the page zoom popup window is not permitted to show on the settings native page.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var rootUiCoordinator =
                            mActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
                    assertFalse(
                            rootUiCoordinator
                                    .getPageZoomManager()
                                    .canShowPopupWindow(UrlConstants.SETTINGS_HOST));
                });
    }

    /** Regression test for https://crbug.com/546419920. */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testAutofillAndPasswordsHighlighting() {
        // The test requires an emulator wide enough to use two-column mode.
        Resources res = mActivityTestRule.getActivity().getResources();
        int minWidth = res.getDimensionPixelSize(R.dimen.settings_min_multi_column_screen_width);
        int screenWidth = res.getDisplayMetrics().widthPixels;
        Assume.assumeTrue("Test requires two-column mode.", screenWidth >= minWidth);

        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Shorten the IDs for better line wrapping.
        int searchEngineTitle = R.string.search_engine_settings;
        int autofillTitle = R.string.autofill_and_passwords_settings_title;

        // Verify the settings page loads by checking for a top-level preference item.
        onViewWaiting(withText(searchEngineTitle)).check(matches(isDisplayed()));

        // Multi-column settings has multiple RecyclerViews. Disambiguate the header
        // RecyclerView by its parent layout rather than using hasDescendant(...), because
        // target preferences may be scrolled off-screen and not yet attached to the hierarchy
        // on shorter viewports (e.g., landscape foldable/tablet screens).
        var headerRecyclerViewMatcher =
                allOf(withId(R.id.recycler_view), isDescendantOfA(withId(R.id.preferences_header)));

        // Click on Search engine in the left column.
        onView(headerRecyclerViewMatcher)
                .perform(scrollTo(hasDescendant(withText(searchEngineTitle))));
        var searchEngineInHeader =
                allOf(
                        isDescendantOfA(withId(R.id.preferences_header)),
                        withText(searchEngineTitle));
        onView(searchEngineInHeader).perform(click());

        // Verify Search engine is highlighted.
        onView(searchEngineInHeader).check(matches(isHighlighted()));

        // Click on Autofill and passwords in the left column.
        onView(headerRecyclerViewMatcher).perform(scrollTo(hasDescendant(withText(autofillTitle))));
        var autofillInHeader =
                allOf(isDescendantOfA(withId(R.id.preferences_header)), withText(autofillTitle));
        onView(autofillInHeader).perform(click());

        // Verify Autofill and passwords is highlighted.
        onView(autofillInHeader).check(matches(isHighlighted()));
    }

    @Test
    @MediumTest
    public void testSearchBoxAutoFocus() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.search_box)).check(matches(isFocused()));
    }

    @Test
    @MediumTest
    public void testAutoFocusOnSettingsPageByTabSwitching() {
        // Load Settings in Tab 0.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.search_box)).check(matches(isFocused()));

        // Open a second tab (about:blank).
        mActivityTestRule.loadUrlInNewTab("about:blank");

        // Switch back to Tab 0 (Settings).
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        // Verify the search box is automatically focused on tab switch.
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.search_box)).check(matches(isFocused()));
    }

    /** Regression test for https://crbug.com/551620206. */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchBoxFocusAfterExitingSearch() {
        // Ensure starting in portrait (single-column mode on tablet).
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        mActivityTestRule.loadUrl("chrome-native://settings/");
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));

        // Rotate display to landscape.
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        ensureTwoColumnMode();

        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));

        // Tap on search box to enter search state.
        onViewWaiting(withId(R.id.search_box)).perform(click());
        onViewWaiting(withId(R.id.search_query)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.search_query)).check(matches(isFocused()));

        // Tap on the back button in the search box to exit search state.
        onViewWaiting(withId(R.id.back_arrow_icon)).perform(click());
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));

        // Tap on search box a second time and verify search query is focused.
        onViewWaiting(withId(R.id.search_box)).perform(click());
        onViewWaiting(withId(R.id.search_query)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.search_query)).check(matches(isFocused()));
    }

    /** Regression test for https://crbug.com/549509308. */
    @Test
    @MediumTest
    public void testTwoSettingsTabs_themeChange_searchBoxRemainsVisibleOnFirstTab() {
        // Open Tab 0 with Settings.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));

        // Open Tab 1 with Settings.
        mActivityTestRule.loadUrlInNewTab("chrome-native://settings/");
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));

        // Recreate activity (simulating theme change or OS configuration change).
        mActivityTestRule.recreateActivity();

        // Switch back to Tab 0.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        // Verify the search box is displayed on Tab 0.
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchInSingleColumnThenRotateToLandscapeAndExitSearch() {
        // Ensure starting in portrait (usually single-column mode on tablet).
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Wait for settings page to load.
        onViewWaiting(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

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

        // Click search box to enter search state in single-column mode.
        onViewWaiting(withId(R.id.search_box)).perform(click());
        onViewWaiting(withId(R.id.search_query_container)).check(matches(isDisplayed()));

        // Rotate to landscape (two-column mode).
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        ensureTwoColumnMode();

        // Tap on back arrow in search query box to exit search.
        onViewWaiting(withId(R.id.back_arrow_icon)).perform(click());

        // Verify that in two-column mode, the search box is visible and the detail pane is
        // not blank (shows the initial detail fragment, e.g. search engine settings in header
        // and default "Google services" detail fragment).
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));
        onViewWaiting(allOf(withText(R.string.search_engine_settings), isDisplayed()))
                .check(matches(isDisplayed()));
        onViewWaiting(withText(R.string.allow_chrome_signin_title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchQueryInMultiColumnThenExitSearchDoesNotShowSearchResultsBehindDetail() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Wait for settings page to load.
        onViewWaiting(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

        // Ensure landscape (two-column mode).
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        ensureTwoColumnMode();

        // Wait for settings page in landscape (two-column mode with detail pane).
        onViewWaiting(withText(R.string.allow_chrome_signin_title)).check(matches(isDisplayed()));

        // Click search box to enter search state in multi-column mode.
        onViewWaiting(withId(R.id.search_box)).perform(click());
        onViewWaiting(withId(R.id.search_query_container)).check(matches(isDisplayed()));

        // Type search query "Theme".
        onViewWaiting(withId(R.id.search_query)).perform(replaceText("Theme"), closeSoftKeyboard());

        // Wait for search results to appear.
        onViewWaiting(allOf(withId(android.R.id.title), withText(R.string.theme_settings)))
                .check(matches(isDisplayed()));

        // Tap on back arrow in search query box to exit search.
        onViewWaiting(withId(R.id.back_arrow_icon)).perform(click());

        // Verify that the search box is visible and Google Services detail pane is displayed.
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));
        onViewWaiting(withText(R.string.allow_chrome_signin_title)).check(matches(isDisplayed()));

        // Verify that the search result is not visible/shown behind the detail pane.
        onView(allOf(withId(android.R.id.title), withText(R.string.theme_settings)))
                .check(doesNotExist());
    }

    /** Regression test for https://crbug.com/549509308. */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchBoxAlignmentInPortrait() {
        // Ensure portrait.
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        // Load settings.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        onViewWaiting(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

        // Capture search_box screen bounds.
        Rect searchBoxBounds = getViewScreenBounds(R.id.search_box);

        // Tap on search box to enter search state.
        onViewWaiting(withId(R.id.search_box)).perform(click());
        onViewWaiting(withId(R.id.search_query_container)).check(matches(isDisplayed()));

        // Verify search_query_container matches search_box horizontal screen bounds.
        Rect queryBounds = getViewScreenBounds(R.id.search_query_container);
        assertEquals(
                "Search query container should align horizontally with search box",
                searchBoxBounds.left,
                queryBounds.left);
        assertEquals(
                "Search query container should match search box width",
                searchBoxBounds.width(),
                queryBounds.width());

        // Type "Theme" in search query.
        onViewWaiting(withId(R.id.search_query)).perform(replaceText("Theme"), closeSoftKeyboard());

        // Wait for search results to appear and click "Theme".
        onViewWaiting(allOf(withId(android.R.id.title), withText(R.string.theme_settings)))
                .check(matches(isDisplayed()));
        onView(allOf(withId(android.R.id.title), withText(R.string.theme_settings)))
                .perform(click());

        // Simulate theme switch / activity recreation.
        mActivityTestRule.recreateActivity();

        // Navigate back from ThemeSettings to search results.
        Espresso.pressBack();

        // Exit search state back to MainSettings.
        Espresso.pressBack();

        // Tap on search box again.
        onViewWaiting(withId(R.id.search_box)).perform(click());
        onViewWaiting(withId(R.id.search_query_container)).check(matches(isDisplayed()));

        // Verify search_query_container still matches search_box horizontal screen bounds.
        Rect queryBoundsAfterBack = getViewScreenBounds(R.id.search_query_container);
        assertEquals(
                "Search query container should align horizontally with search box after navigating"
                        + " back",
                searchBoxBounds.left,
                queryBoundsAfterBack.left);
        assertEquals(
                "Search query container should match search box width after navigating back",
                searchBoxBounds.width(),
                queryBoundsAfterBack.width());
    }

    /** Returns the on-screen bounds of the view with the given id. */
    private Rect getViewScreenBounds(int viewId) {
        Rect bounds = new Rect();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    View view = activity.findViewById(viewId);
                    int[] location = new int[2];
                    view.getLocationOnScreen(location);
                    bounds.set(
                            location[0],
                            location[1],
                            location[0] + view.getWidth(),
                            location[1] + view.getHeight());
                });
        return bounds;
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

    /**
     * Matches whether a TextView (such as a preference title in the settings main menu) has the
     * selected text color applied by {@link SelectionDecoration}.
     */
    private static Matcher<View> isHighlighted() {
        return new BoundedMatcher<View, TextView>(TextView.class) {
            @Override
            public void describeTo(Description description) {
                description.appendText("is highlighted (selected text color)");
            }

            @Override
            protected boolean matchesSafely(TextView textView) {
                int expectedColor =
                        SemanticColorUtils.getColorOnSecondaryContainer(textView.getContext());
                return textView.getCurrentTextColor() == expectedColor;
            }
        };
    }
}
