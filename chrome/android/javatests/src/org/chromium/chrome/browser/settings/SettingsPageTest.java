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
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isFocused;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.provider.Settings;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.settings.AndroidPaymentAppsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageAssassin;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.settings.search.SearchResultsPreferenceFragment;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ChromeTriggers;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Integration tests for {@link SettingsPage} inside a native tab. Most tests use a mix of onView()
 * and waitForView() depending on whether they need to wait for a fragment or view to load.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(PER_CLASS)
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class SettingsPageTest {
    private static Integer sInitialOrientation;
    private boolean mFinishActivity;

    /** The activity the test started with, to detect tests that recreate it. */
    private ChromeTabbedActivity mInitialActivity;

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Before
    public void setUp() {
        var activity = mActivityTestRule.getActivity();
        mInitialActivity = activity;
        if (sInitialOrientation == null) {
            sInitialOrientation = activity.getResources().getConfiguration().orientation;
        }

        // Skip the tests on Android 14 landscape devices. See below.
        boolean isLandscape = sInitialOrientation == Configuration.ORIENTATION_LANDSCAPE;
        Assume.assumeFalse(
                "Rotating to portrait letterboxes the activity on landscape-oriented devices,"
                        + " causing Android 14's letterbox education dialog to intercept clicks.",
                Build.VERSION.SDK_INT <= Build.VERSION_CODES.UPSIDE_DOWN_CAKE && isLandscape);

        // Batched tests spawn many renderer processes, which can trigger OS memory-pressure
        // signals (e.g. TRIM_MEMORY_RUNNING_LOW / CRITICAL on Android <= 13) while a Settings tab
        // is briefly hidden behind another tab. Prevent ChromeTabbedActivity.onTrimMemory() from
        // freezing hidden SettingsPage instances mid-test. See crbug.com/579988178.
        NativePageAssassin.setDisableFreezeAllHiddenPagesForTesting(true);
    }

    @After
    public void tearDown() {
        LocalizationUtils.setRtlForTesting(false);
        var activity = mActivityTestRule.getActivity();
        ActivityTestUtils.clearActivityOrientation(activity);
        // Do not carry an activity that was recreated during the test (theme change, tab
        // reparenting, etc.) over to the next test. The recreated activity may still be starting
        // up on the UI thread when the next test begins, and resetters it registers between
        // tests can deadlock against ResettersForTesting.beforeHooksWillExecute(), which runs
        // resetters under its lock while they block on the UI thread. That hangs the next test
        // before its body runs and times out the whole batch. See crbug.com/578114288.
        if (mFinishActivity || activity != mInitialActivity) {
            mActivityTestRule.finishActivity();
            return;
        }
        Espresso.closeSoftKeyboard();
        if (sInitialOrientation != null
                && activity.getResources().getConfiguration().orientation != sInitialOrientation) {
            ensureActivityOrientation(sInitialOrientation);
        }
    }

    /** Returns the index of the currently selected tab in the current tab model. */
    private int getCurrentTabIndex() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getCurrentTabModel().index());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testOpenSettingsAndClickPreference() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Verify the settings page loads by checking for a top-level preference item.
        waitForView(withText(R.string.search_engine_settings));

        // Click on a setting in the column on the left (e.g., Privacy and security).
        // Check the descendent because multi-column settings contains two recycler views.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.prefs_privacy_security)));
        waitForView(matcher)
                .performViewAction(
                        scrollTo(hasDescendant(withText(R.string.prefs_privacy_security))));
        waitForView(withText(R.string.prefs_privacy_security)).click();

        // Verify the detail page loads by checking an item in the detail preference screen.
        waitForView(withText(R.string.clear_browsing_data_title));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testSearchBoxMarginsOnContainerResized() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        waitForView(withId(R.id.search_box));

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
        final int narrowWidth =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                Math.min(
                                        ViewUtils.dpToPx(
                                                mActivityTestRule
                                                        .getActivity()
                                                        .getResources()
                                                        .getDisplayMetrics(),
                                                500),
                                        originalWidth - 200));
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
                    if (isOnWideScreen || SettingsInTab.shouldOpenSettingsInTab()) {
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
                    if (isOnWideScreen || SettingsInTab.shouldOpenSettingsInTab()) {
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
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testThemeSwitchRestoresSettingsPageAndDetailFragment() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Verify MainSettings header fragment is displayed by checking for Search engine
        // preference.
        waitForView(withText(R.string.search_engine_settings));

        // Click on "Search engine" in MainSettings header pane to open SearchEngineSettings detail
        // fragment.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.search_engine_settings)));
        waitForView(matcher)
                .performViewAction(
                        scrollTo(hasDescendant(withText(R.string.search_engine_settings))));
        waitForView(withText(R.string.search_engine_settings)).click();

        // Simulate theme switch / activity recreation.
        mActivityTestRule.recreateActivity();

        // 1. Verify Toolbar/Action Bar is restored and displayed.
        waitForView(withId(R.id.action_bar));

        // 2. Verify MainSettings header pane is restored. In single-column mode the detail pane
        // covers it.
        waitForHeaderPaneWithDetailOpen();

        // 3. Verify SearchEngineSettings detail pane fragment is restored and displayed.
        waitForView(withText("Microsoft Bing"));
    }

    /**
     * Waits for MainSettings to be restored in the header pane while a detail page is open.
     * Two-column mode shows the header pane beside the detail page. Single-column mode hides the
     * header pane under the detail page.
     */
    private void waitForHeaderPaneWithDetailOpen() {
        AtomicBoolean isTwoColumn = new AtomicBoolean();
        CriteriaHelper.pollUiThread(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    Criteria.checkThat(hostFragment, notNullValue());
                    var multiColumn = hostFragment.getMultiColumnSettings();
                    Criteria.checkThat(multiColumn, notNullValue());
                    View view = multiColumn.getView();
                    Criteria.checkThat(view, notNullValue());
                    Criteria.checkThat(view.isLaidOut(), is(true));
                    Criteria.checkThat(
                            multiColumn
                                    .getChildFragmentManager()
                                    .findFragmentById(R.id.preferences_header),
                            instanceOf(MainSettings.class));
                    View header = view.findViewById(R.id.preferences_header);
                    Criteria.checkThat(header, notNullValue());
                    isTwoColumn.set(multiColumn.isTwoColumn());
                    int expected = isTwoColumn.get() ? View.VISIBLE : View.INVISIBLE;
                    Criteria.checkThat(header.getVisibility(), is(expected));
                });
        if (isTwoColumn.get()) {
            waitForView(withText(R.string.prefs_privacy_security));
        }
    }

    /**
     * Recreates the activity the way a real theme change does. Unlike {@link
     * ChromeTabbedActivityTestRule#recreateActivity()}, which calls {@link
     * android.app.Activity#recreate()} directly, this goes through {@link
     * org.chromium.chrome.browser.app.ChromeActivity#onNightModeStateChanged()}, which detaches
     * tabs for reparenting before recreating the activity.
     */
    private void recreateActivityForThemeChange() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        activity::onNightModeStateChanged);
        mActivityTestRule.getActivityTestRule().setActivity(newActivity);
    }

    /** Regression test for crbug.com/566898692. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testThemeSwitchWithTabReparentingRestoresDetailFragment() {
        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withText(R.string.search_engine_settings));

        // Open the SearchEngineSettings detail fragment.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.search_engine_settings)));
        waitForView(matcher)
                .performViewAction(
                        scrollTo(hasDescendant(withText(R.string.search_engine_settings))));
        waitForView(withText(R.string.search_engine_settings)).click();
        waitForView(withText("Microsoft Bing"));

        // Recreate the activity through the theme change path, which detaches the tab.
        recreateActivityForThemeChange();

        // Verify the header pane and the SearchEngineSettings detail fragment are restored.
        waitForView(withId(R.id.action_bar));
        waitForHeaderPaneWithDetailOpen();
        waitForView(withText("Microsoft Bing"));
    }

    /**
     * Clicks the view with the given ID and waits for the activity it recreates, as a theme radio
     * button on the Theme page does. Unlike {@link #recreateActivityForThemeChange()}, the new
     * activity really has a new configuration.
     */
    private void clickAndWaitForRecreate(int viewId) {
        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        /* uiThreadTrigger= */ null,
                        /* backgroundThreadTrigger= */ () ->
                                onView(allOf(withId(viewId), isDisplayed())).perform(click()));
        mActivityTestRule.getActivityTestRule().setActivity(newActivity);
        waitForSettingsPageInTab("Settings page should reattach after the theme change.");
    }

    /**
     * Regression test for crbug.com/578622279: pages opened from settings search do not change the
     * tab URL, so a theme change must restore them from fragment state rather than from the URL.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testThemeChangeFromSearchKeepsThemePage_urlNav() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        int dark = org.chromium.chrome.browser.night_mode.R.id.dark;
        int light = org.chromium.chrome.browser.night_mode.R.id.light;
        try {
            mActivityTestRule.loadUrl("chrome-native://settings/");
            waitForView(withText(R.string.search_engine_settings));

            // Search for "Appearance", open it from the results, then open Theme.
            waitForView(withId(R.id.search_box)).click();
            var searchQuery = waitForView(withId(R.id.search_query));
            searchQuery.performViewAction(replaceText("Appearance"));
            searchQuery.performViewAction(closeSoftKeyboard());
            waitForView(
                            withId(android.R.id.title),
                            withText(R.string.appearance_settings),
                            isDescendantOfA(withId(R.id.preferences_detail)))
                    .click();
            waitForView(withText(R.string.theme_settings)).click();
            waitForView(withId(dark));

            // Switch to dark and back to light. The Theme page should survive both recreations.
            clickAndWaitForRecreate(dark);
            waitForView(withId(light));
            clickAndWaitForRecreate(light);
            waitForView(withId(dark));

            // Back should go to Appearance, i.e. the search's back stack was restored too.
            Espresso.pressBack();
            waitForView(withText(R.string.theme_settings));
        } finally {
            // Leave the batch in light mode even if the test failed while dark.
            if (ThreadUtils.runOnUiThreadBlocking(
                    () -> GlobalNightModeStateProviderHolder.getInstance().isInNightMode())) {
                mActivityTestRule
                        .getActivityTestRule()
                        .setActivity(
                                ApplicationTestUtils.waitForActivityWithClass(
                                        ChromeTabbedActivity.class,
                                        Stage.RESUMED,
                                        () ->
                                                ChromeNightModeTestUtils
                                                        .setUpNightModeForChromeActivity(false)));
            }
            ThreadUtils.runOnUiThreadBlocking(
                    ChromeNightModeTestUtils::tearDownNightModeAfterChromeActivityDestroyed);
        }
    }

    /**
     * Regression test for crbug.com/566132507: after a theme change, the toolbar back arrow on a
     * nested detail page should return to its parent page instead of navigating the tab away from
     * settings. Without SettingsInTabUrlNav this fails, because the TAB_HISTORY back press handler
     * outranks the settings page's NATIVE_PAGE handler, so nested-page back depends on fragment
     * callback registration order.
     */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testThemeSwitchInSingleColumnThenToolbarBackOnNestedPage() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);
        mActivityTestRule.loadUrl("chrome-native://settings/");
        ensureSingleColumnMode();

        // Open Privacy and security, then its nested Delete browsing data page.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.prefs_privacy_security)));
        waitForView(matcher)
                .performViewAction(
                        scrollTo(hasDescendant(withText(R.string.prefs_privacy_security))));
        waitForView(withText(R.string.prefs_privacy_security)).click();
        waitForView(withText(R.string.clear_browsing_data_title)).click();
        waitForView(withText(R.string.clear_cookies_and_site_data_title));

        recreateActivityForThemeChange();
        ensureSingleColumnMode();
        waitForView(withText(R.string.clear_cookies_and_site_data_title));

        // Tap the toolbar back arrow. Privacy and security should be shown in the same tab.
        // isDisplayed() disambiguates from the 0x0 back_arrow_icon with the same description.
        waitForView(
                        withContentDescription(R.string.back),
                        isDescendantOfA(withId(R.id.action_bar)),
                        isDisplayed())
                .click();
        CriteriaHelper.pollUiThread(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    Tab tab = activity.getActivityTab();
                    Criteria.checkThat(
                            "Tab should still show settings",
                            tab != null && tab.getNativePage() instanceof SettingsPage,
                            is(true));
                    var hostFragment = SettingsHostFragment.get(activity);
                    Criteria.checkThat(hostFragment, notNullValue());
                    Criteria.checkThat(
                            hostFragment.getMainFragment(), instanceOf(PrivacySettings.class));
                });
    }

    /** Regression test for https://crbug.com/535695748. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testTwoSettingsTabsThemeSwitchRestoresDetailFragment() {
        // Open settings in the current tab and navigate to the Search engine detail fragment.
        // Tests in this class are batched, so the current tab is not necessarily index 0. Remember
        // where settings was loaded so we can switch back to it below.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        int firstSettingsTab = getCurrentTabIndex();
        waitForView(withText(R.string.search_engine_settings));

        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.search_engine_settings)));
        waitForView(matcher)
                .performViewAction(
                        scrollTo(hasDescendant(withText(R.string.search_engine_settings))));
        waitForView(withText(R.string.search_engine_settings)).click();
        waitForView(withText("Microsoft Bing"));

        // Open a second settings tab at root MainSettings.
        mActivityTestRule.loadUrlInNewTab("chrome-native://settings/");
        waitForView(withText(R.string.prefs_privacy_security));

        // Simulate theme switch / activity recreation.
        mActivityTestRule.recreateActivity();

        // Verify the second tab (active tab): Action bar and MainSettings header pane are restored.
        waitForView(withId(R.id.action_bar));
        waitForView(withText(R.string.prefs_privacy_security));

        // Switch back to the first settings tab.
        ChromeTriggers.selectTabProgrammatically(mActivityTestRule.getActivity(), firstSettingsTab);

        // Verify the first settings tab: Action bar and SearchEngineSettings detail fragment are
        // restored.
        waitForView(withId(R.id.action_bar));
        waitForView(withText("Microsoft Bing"));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testAccessibilityPageZoomDoesNotShowPopup() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Click on "Accessibility" in MainSettings header pane.
        var matcher =
                allOf(
                        withId(R.id.recycler_view),
                        hasDescendant(withText(R.string.search_engine_settings)));
        waitForView(matcher)
                .performViewAction(scrollTo(hasDescendant(withText(R.string.prefs_accessibility))));
        waitForView(withText(R.string.prefs_accessibility)).click();

        // Verify the Accessibility preference screen is displayed.
        waitForView(withText(R.string.page_zoom_title));

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
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testAutofillAndPasswordsHighlighting() {
        assumeTwoColumnScreenWidth();

        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Shorten the IDs for better line wrapping.
        int searchEngineTitle = R.string.search_engine_settings;
        int autofillTitle = R.string.autofill_and_passwords_settings_title;

        // Verify the settings page loads by checking for a top-level preference item.
        waitForView(withText(searchEngineTitle));

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

    /** Regression test for https://crbug.com/565417618 under SettingsInTabUrlNav. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testExternalActivityPreferenceKeepsHighlight_urlNav() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        assumeTwoColumnScreenWidth();
        Assume.assumeTrue(
                "Test requires the notification settings row.",
                PackageManagerUtils.canResolveActivity(
                        new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS)));

        mActivityTestRule.loadUrl("chrome-native://settings/");

        int searchEngineTitle = R.string.search_engine_settings;
        int notificationsTitle = R.string.prefs_notifications;
        waitForView(withText(searchEngineTitle));

        var headerRecyclerViewMatcher =
                allOf(withId(R.id.recycler_view), isDescendantOfA(withId(R.id.preferences_header)));

        // Open Search engine in the detail pane.
        onView(headerRecyclerViewMatcher)
                .perform(scrollTo(hasDescendant(withText(searchEngineTitle))));
        var searchEngineInHeader =
                allOf(
                        isDescendantOfA(withId(R.id.preferences_header)),
                        withText(searchEngineTitle));
        onView(searchEngineInHeader).perform(click());
        onView(searchEngineInHeader).check(matches(isHighlighted()));

        Intents.init();
        try {
            // Stub the Android notification settings activity.
            intending(hasAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS))
                    .respondWith(new ActivityResult(Activity.RESULT_OK, null));

            onView(headerRecyclerViewMatcher)
                    .perform(scrollTo(hasDescendant(withText(notificationsTitle))));
            var notificationsInHeader =
                    allOf(
                            isDescendantOfA(withId(R.id.preferences_header)),
                            withText(notificationsTitle));
            onView(notificationsInHeader).perform(click());
            intended(hasAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS));

            // The detail pane didn't change, so neither should the highlight.
            onView(notificationsInHeader).check(matches(not(isHighlighted())));
            onView(headerRecyclerViewMatcher)
                    .perform(scrollTo(hasDescendant(withText(searchEngineTitle))));
            onView(searchEngineInHeader).check(matches(isHighlighted()));
        } finally {
            Intents.release();
        }
    }

    /** Regression test for https://crbug.com/565947215. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testPaymentAppsUrlHighlightsMainMenu_urlNav() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        assertUrlHighlightsAutofillAndPasswords("/paymentApps", AndroidPaymentAppsFragment.class);
    }

    /**
     * Regression test for https://crbug.com/565947215. These pages declare a main menu row that
     * Autofill and passwords replaces under HoT.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testFoldedRowPagesUrlHighlightMainMenu_urlNav() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        assertUrlHighlightsAutofillAndPasswords("/payments", AutofillPaymentMethodsFragment.class);
        assertUrlHighlightsAutofillAndPasswords("/addresses", AutofillProfilesFragment.class);
        assertUrlHighlightsAutofillAndPasswords(
                "/autofill/settings", AutofillOptionsFragment.class);
    }

    /**
     * Site settings and Safety check are nested in privacy and security, so their pages highlight
     * that row.
     */
    // TODO(crbug.com/581503633): Clean up after the SettingsIaReorganization experiment has run.
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION + ":arm/direction_1")
    public void testNestedInPrivacyPagesUrlHighlightPrivacy_urlNav() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        assertUrlHighlightsMainMenuRow(
                "/siteSettings", SiteSettings.class, R.string.prefs_privacy_security);
        assertUrlHighlightsMainMenuRow(
                "/safetyCheck", SafetyHubFragment.class, R.string.prefs_privacy_security);
    }

    /**
     * Loads {@code path} under chrome-native://settings, waits for {@code fragmentClass} to show in
     * the detail pane, and checks that Autofill and passwords is highlighted in the main menu.
     */
    private void assertUrlHighlightsAutofillAndPasswords(
            String path, Class<? extends Fragment> fragmentClass) {
        assertUrlHighlightsMainMenuRow(
                path, fragmentClass, R.string.autofill_and_passwords_settings_title);
    }

    /**
     * Loads {@code path} under chrome-native://settings, waits for {@code fragmentClass} to show in
     * the detail pane, and checks that the main menu row titled {@code rowTitle} is highlighted.
     */
    private void assertUrlHighlightsMainMenuRow(
            String path, Class<? extends Fragment> fragmentClass, @StringRes int rowTitle) {
        assumeTwoColumnScreenWidth();

        mActivityTestRule.loadUrl("chrome-native://settings" + path);

        CriteriaHelper.pollUiThread(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    Criteria.checkThat(hostFragment, notNullValue());
                    Criteria.checkThat(hostFragment.getMainFragment(), instanceOf(fragmentClass));
                });

        var headerRecyclerViewMatcher =
                allOf(withId(R.id.recycler_view), isDescendantOfA(withId(R.id.preferences_header)));
        onView(headerRecyclerViewMatcher).perform(scrollTo(hasDescendant(withText(rowTitle))));
        onView(allOf(isDescendantOfA(withId(R.id.preferences_header)), withText(rowTitle)))
                .check(matches(isHighlighted()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testSearchBoxAutoFocus() {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        waitForView(withId(R.id.search_box)).check(matches(isFocused()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testAutoFocusOnSettingsPageByTabSwitching() {
        // Load Settings. See getCurrentTabIndex() for why the index is captured.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        int settingsTab = getCurrentTabIndex();
        waitForView(withId(R.id.search_box)).check(matches(isFocused()));

        // Open a second tab (about:blank).
        mActivityTestRule.loadUrlInNewTab("about:blank");

        // Switch back to the Settings tab.
        ChromeTriggers.selectTabProgrammatically(mActivityTestRule.getActivity(), settingsTab);

        // Verify the search box is automatically focused on tab switch.
        waitForView(withId(R.id.search_box)).check(matches(isFocused()));
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
        waitForView(withId(R.id.search_box));

        // Rotate display to landscape.
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        ensureTwoColumnMode();

        waitForView(withId(R.id.search_box));

        // Tap on search box to enter search state.
        waitForView(withId(R.id.search_box)).click();
        waitForView(withId(R.id.search_query)).check(matches(isFocused()));

        // Tap on the back button in the search box to exit search state.
        clickSearchBackArrow();
        waitForView(withId(R.id.search_box));

        // Tap on search box a second time and verify search query is focused.
        waitForView(withId(R.id.search_box)).click();
        waitForView(withId(R.id.search_query)).check(matches(isFocused()));
    }

    /** Regression test for https://crbug.com/563047017. */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testRootSettingsShownAfterRotatingTwoColumnLandscapeToPortrait() {
        // Start in landscape, which uses two-column mode on a tablet.
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);

        mActivityTestRule.loadUrl("chrome-native://settings/");
        ensureTwoColumnMode();

        // Rotate to portrait, which uses single-column mode.
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);
        ensureSingleColumnMode();

        // Root settings should be showing, not the detail fragment that two-column mode
        // created to fill its detail pane.
        assertRootSettingsShown();
    }

    /** Regression test for https://crbug.com/563047017. */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testRootSettingsShownAfterRotatingPortraitToLandscapeAndBack() {
        // Start in portrait, which uses single-column mode at root settings.
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        mActivityTestRule.loadUrl("chrome-native://settings/");
        ensureSingleColumnMode();
        assertRootSettingsShown();

        // Rotate to landscape. Entering two-column mode creates an initial detail fragment to
        // fill the detail pane, which the user did not navigate to.
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        ensureTwoColumnMode();

        // Rotate back to portrait, which uses single-column mode.
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);
        ensureSingleColumnMode();

        // Root settings should be showing, not the auto-created detail fragment.
        assertRootSettingsShown();
    }

    /**
     * Asserts that root settings is showing, i.e. no detail page is covering it.
     *
     * <p>Espresso's isDisplayed() only tests whether a view occupies screen space, and the header
     * pane still does while an open detail pane is drawn on top of it. The pane state therefore has
     * to be checked directly.
     */
    private void assertRootSettingsShown() {
        waitForView(
                withText(R.string.search_engine_settings),
                isDescendantOfA(withId(R.id.preferences_header)));

        boolean rootShown =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var hostFragment =
                                    SettingsHostFragment.get(mActivityTestRule.getActivity());
                            assertNotNull(hostFragment);
                            var multiColumn = hostFragment.getMultiColumnSettings();
                            assertNotNull(multiColumn);
                            // Two-column mode always shows root settings in the header pane.
                            return multiColumn.isTwoColumn() || !multiColumn.isLayoutOpen();
                        });
        assertTrue("A detail page is covering root settings.", rootShown);
    }

    /** Regression test for https://crbug.com/549509308. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testTwoSettingsTabs_themeChange_searchBoxRemainsVisibleOnFirstTab() {
        // Open the first tab with Settings. See getCurrentTabIndex() for why the index is captured.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        int firstSettingsTab = getCurrentTabIndex();
        waitForView(withId(R.id.search_box));

        // Open a second tab with Settings.
        mActivityTestRule.loadUrlInNewTab("chrome-native://settings/");
        waitForView(withId(R.id.search_box));

        // Recreate activity (simulating theme change or OS configuration change).
        mActivityTestRule.recreateActivity();
        waitForView(withId(R.id.search_box));

        // Switch back to the first Settings tab.
        ChromeTriggers.selectTabProgrammatically(mActivityTestRule.getActivity(), firstSettingsTab);

        // Verify the search box is displayed on the first Settings tab.
        waitForView(withId(R.id.search_box));
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
        waitForView(withText(R.string.search_engine_settings));

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
        waitForView(withId(R.id.search_box)).click();
        waitForView(withId(R.id.search_query_container));

        // Rotate to landscape (two-column mode).
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        ensureTwoColumnMode();

        // Tap on back arrow in search query box to exit search.
        clickSearchBackArrow();

        // Verify that in two-column mode, the search box is visible and the detail pane is
        // not blank (shows the initial detail fragment, e.g. search engine settings in header
        // and default "Google services" detail fragment).
        waitForView(withId(R.id.search_box));
        waitForView(withText(R.string.search_engine_settings));
        waitForView(withText(R.string.allow_chrome_signin_title));
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
        waitForView(withText(R.string.search_engine_settings));

        // Ensure landscape (two-column mode).
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        ensureTwoColumnMode();

        // Wait for settings page in landscape (two-column mode with detail pane).
        waitForView(withText(R.string.allow_chrome_signin_title));

        // Click search box to enter search state in multi-column mode.
        waitForView(withId(R.id.search_box)).click();
        waitForView(withId(R.id.search_query_container));

        // Type search query "Theme".
        var searchQuery = waitForView(withId(R.id.search_query));
        searchQuery.performViewAction(replaceText("Theme"));
        searchQuery.performViewAction(closeSoftKeyboard());

        // Wait for search results to appear.
        waitForView(withId(android.R.id.title), withText(R.string.theme_settings));

        // Tap on back arrow in search query box to exit search.
        clickSearchBackArrow();

        // Verify that the search box is visible and Google Services detail pane is displayed.
        waitForView(withId(R.id.search_box));
        waitForView(withText(R.string.allow_chrome_signin_title));

        // Verify that the search result is not visible/shown behind the detail pane.
        onView(allOf(withId(android.R.id.title), withText(R.string.theme_settings)))
                .check(doesNotExist());
    }

    /** Regression test for https://crbug.com/573289137. */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchResultInMultiColumnStaysOpenAfterTabSwitch() {
        // Ensure landscape (two-column mode). See getCurrentTabIndex() for why the index is
        // captured.
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);
        mActivityTestRule.loadUrl("chrome-native://settings/");
        int settingsTab = getCurrentTabIndex();
        waitForView(withText(R.string.search_engine_settings));
        ensureTwoColumnMode();

        // Search for "Theme" and open the result.
        waitForView(withId(R.id.search_box)).click();
        var searchQuery = waitForView(withId(R.id.search_query));
        searchQuery.performViewAction(replaceText("Theme"));
        searchQuery.performViewAction(closeSoftKeyboard());
        waitForView(withId(android.R.id.title), withText(R.string.theme_settings)).click();
        CriteriaHelper.pollUiThread(
                () -> {
                    Fragment fragment = getSettingsMainFragment();
                    return fragment != null
                            && !(fragment instanceof SearchResultsPreferenceFragment);
                },
                "The search result page should be shown.");
        Class<?> resultPageClass = getSettingsMainFragment().getClass();
        Object settingsPage = getCurrentNativePage();

        // Open a second tab, then switch back to the Settings tab.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        ChromeTriggers.selectTabProgrammatically(mActivityTestRule.getActivity(), settingsTab);
        waitForView(withId(R.id.search_query_container));

        // A single tab switch keeps the settings page alive, rather than freezing and recreating
        // it, so its fragment state is preserved.
        assertSame(
                "The settings page should not be recreated.", settingsPage, getCurrentNativePage());

        // Let pending fragment transactions run, then verify the result page is still shown rather
        // than the search results it was opened from.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    assertNotNull(hostFragment);
                    hostFragment.executePendingNavigations();
                });
        assertThat(getSettingsMainFragment(), instanceOf(resultPageClass));
    }

    /**
     * Regression test for https://crbug.com/581444921: freezing a hidden settings tab removes its
     * host fragment, so that showing the tab again starts from a new one.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testFreezeHiddenSettingsTabRemovesHostFragment_urlNav() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        mActivityTestRule.loadUrl("chrome-native://settings/");
        int settingsTabIndex = getCurrentTabIndex();
        waitForView(withText(R.string.search_engine_settings));
        Tab settingsTab = mActivityTestRule.getActivityTab();
        SettingsHostFragment hostFragment =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> SettingsHostFragment.get(mActivityTestRule.getActivity()));
        assertNotNull(hostFragment);

        // Hide the settings tab, then freeze it as NativePageAssassin does.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    settingsTab.freezeNativePage();
                    assertTrue(settingsTab.getNativePage().isFrozen());
                    mActivityTestRule
                            .getActivity()
                            .getSupportFragmentManager()
                            .executePendingTransactions();
                    assertFalse(
                            "The frozen page's host fragment should be removed.",
                            hostFragment.isAdded());
                });

        // Showing the tab again unfreezes it into a new page.
        ChromeTriggers.selectTabProgrammatically(mActivityTestRule.getActivity(), settingsTabIndex);
        waitForView(withText(R.string.search_engine_settings));
    }

    /**
     * Regression test for https://crbug.com/549509308.
     *
     * <p>TODO(crbug.com/521895796): Remove once SettingsInTabUrlNav is enabled by default, in favor
     * of {@link #testSearchBoxAlignmentInPortrait_urlNav()}.
     */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchBoxAlignmentInPortrait() {
        FeatureOverrides.disable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        Rect searchBoxBounds = openThemeSettingsFromSearchInPortrait();

        // Simulate theme switch / activity recreation.
        mActivityTestRule.recreateActivity();

        // Navigate back from ThemeSettings to search results.
        Espresso.pressBack();

        // Exit search state back to MainSettings.
        Espresso.pressBack();

        tapSearchBoxAndAssertQueryAlignment(searchBoxBounds, "after navigating back");
    }

    /** Regression test for https://crbug.com/549509308, with URL navigation. */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchBoxAlignmentInPortrait_urlNav() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        Rect searchBoxBounds = openThemeSettingsFromSearchInPortrait();

        // Simulate theme switch / activity recreation.
        mActivityTestRule.recreateActivity();

        // Navigate back from ThemeSettings to search results.
        Espresso.pressBack();

        // Exit search state back to MainSettings.
        Espresso.pressBack();

        tapSearchBoxAndAssertQueryAlignment(searchBoxBounds, "after navigating back");
    }

    /**
     * Loads settings in portrait, enters search, and opens ThemeSettings from the search results.
     *
     * @return The screen bounds of the search box before entering search.
     */
    private Rect openThemeSettingsFromSearchInPortrait() {
        // Ensure portrait.
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        // Load settings.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withText(R.string.search_engine_settings));

        // Capture search_box screen bounds.
        Rect searchBoxBounds = getViewScreenBounds(R.id.search_box);

        tapSearchBoxAndAssertQueryAlignment(searchBoxBounds, "on entering search");

        // Type "Theme" in search query.
        var searchQuery = waitForView(withId(R.id.search_query));
        searchQuery.performViewAction(replaceText("Theme"));
        searchQuery.performViewAction(closeSoftKeyboard());

        // Wait for search results to appear and click "Theme".
        waitForView(withId(android.R.id.title), withText(R.string.theme_settings));
        onView(allOf(withId(android.R.id.title), withText(R.string.theme_settings)))
                .perform(click());

        return searchBoxBounds;
    }

    /**
     * Taps the search box to enter search state, and verifies that search_query_container matches
     * the given search_box horizontal screen bounds.
     */
    private void tapSearchBoxAndAssertQueryAlignment(Rect searchBoxBounds, String when) {
        waitForView(withId(R.id.search_box)).click();
        waitForView(withId(R.id.search_query_container));

        Rect queryBounds = getViewScreenBounds(R.id.search_query_container);
        assertEquals(
                "Search query container should align horizontally with search box " + when,
                searchBoxBounds.left,
                queryBounds.left);
        assertEquals(
                "Search query container should match search box width " + when,
                searchBoxBounds.width(),
                queryBounds.width());
    }

    /** Regression test for https://crbug.com/548848118. */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        // Automotive devices do not support display rotation.
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchBoxAlignmentInPortrait_Rtl() {
        mFinishActivity = true;
        // Start in portrait (single-column mode on tablet).
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        // Set RTL layout.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocalizationUtils.setRtlForTesting(true);
                    var activity = mActivityTestRule.getActivity();
                    Configuration config =
                            new Configuration(activity.getResources().getConfiguration());
                    config.setLayoutDirection(new Locale("ar"));
                    activity.getResources()
                            .updateConfiguration(
                                    config, activity.getResources().getDisplayMetrics());
                    activity.getWindow()
                            .getDecorView()
                            .setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
                });

        // Load settings in portrait.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withId(R.id.search_box));

        // Capture search_box and search_icon screen bounds.
        Rect searchBoxBounds = getViewScreenBounds(R.id.search_box);
        Rect searchIconBounds = getViewScreenBounds(R.id.search_icon);

        // Tap on search box to enter search state.
        waitForView(withId(R.id.search_box)).click();
        waitForView(withId(R.id.search_query_container));

        // Capture search_query_container and back_arrow_icon screen bounds.
        Rect queryBounds = getViewScreenBounds(R.id.search_query_container);
        Rect backArrowBounds = getViewScreenBounds(R.id.back_arrow_icon);

        assertEquals(
                "Search query container should match search box width in RTL",
                searchBoxBounds.width(),
                queryBounds.width());
        assertEquals(
                "Search query container should align horizontally with search box in RTL (left)",
                searchBoxBounds.left,
                queryBounds.left);
        assertEquals(
                "Search query container should align horizontally with search box in RTL (right)",
                searchBoxBounds.right,
                queryBounds.right);
        assertEquals(
                "Back arrow icon should horizontally align with search icon in RTL",
                searchIconBounds.left,
                backArrowBounds.left);

        // Tap on Search settings bar and change orientation to Landscape and then Portrait.
        ensureActivityOrientation(Configuration.ORIENTATION_LANDSCAPE);

        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);
        waitForView(withId(R.id.search_box));

        Rect searchBoxBoundsAfterRotate = getViewScreenBounds(R.id.search_box);
        Rect searchIconBoundsAfterRotate = getViewScreenBounds(R.id.search_icon);

        assertEquals(
                "Search box should match initial width after rotating back",
                searchBoxBounds.width(),
                searchBoxBoundsAfterRotate.width());
        assertEquals(
                "Search box should align horizontally after rotating back (left)",
                searchBoxBounds.left,
                searchBoxBoundsAfterRotate.left);
        assertEquals(
                "Search box should align horizontally after rotating back (right)",
                searchBoxBounds.right,
                searchBoxBoundsAfterRotate.right);
        assertEquals(
                "Search icon should align horizontally after rotating back",
                searchIconBounds.left,
                searchIconBoundsAfterRotate.left);
    }

    /**
     * Regression test for https://crbug.com/568207264. In RTL, the search query hint moved or
     * disappeared when the side UI (vertical tab strip / side panel) resized the settings container
     * while the search UI was open.
     */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchQueryHintAlignmentOnContainerResized_Rtl() {
        mFinishActivity = true;
        ensureActivityOrientation(Configuration.ORIENTATION_PORTRAIT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocalizationUtils.setRtlForTesting(true);
                    var activity = mActivityTestRule.getActivity();
                    Configuration config =
                            new Configuration(activity.getResources().getConfiguration());
                    config.setLayoutDirection(new Locale("ar"));
                    activity.getResources()
                            .updateConfiguration(
                                    config, activity.getResources().getDisplayMetrics());
                    activity.getWindow()
                            .getDecorView()
                            .setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
                });

        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withId(R.id.search_box));

        // Skip test if portrait mode happens to be wide enough for two-column mode. The bug only
        // affects the single-column search UI hosted in the toolbar.
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

        // The test device runs in English, so the hint resource is LTR text. Use an RTL hint
        // (Arabic "Search in settings") so the hint is laid out RTL as in the bug. Set it before
        // entering search so that focusing the query field lays it out fresh.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TextView queryEdit =
                            mActivityTestRule.getActivity().findViewById(R.id.search_query);
                    assertNotNull(queryEdit);
                    queryEdit.setHint("البحث في الإعدادات");
                });

        waitForView(withId(R.id.search_box)).click();
        waitForView(withId(R.id.search_query_container));
        waitForView(withId(R.id.search_query)).check(matches(isFocused()));

        final int originalWidth = getViewScreenBounds(R.id.search_query).width();
        assertQueryScrollMatchesWidth("initially");

        // Simulate opening the side panel (shrinking the container width).
        final int narrowWidth =
                Math.min(800, getViewScreenBounds(R.id.settings_activity).width() - 200);
        resizeSettingsContainerWithChangeBounds(narrowWidth);
        CriteriaHelper.pollUiThread(
                () -> {
                    View queryEdit =
                            mActivityTestRule.getActivity().findViewById(R.id.search_query);
                    return queryEdit != null
                            && queryEdit.getWidth() > 0
                            && queryEdit.getWidth() < originalWidth;
                },
                "Query field should shrink with the container.");
        assertQueryScrollMatchesWidth("after shrinking the container");

        // Simulate closing the side panel (restoring the container width).
        resizeSettingsContainerWithChangeBounds(ViewGroup.LayoutParams.MATCH_PARENT);
        CriteriaHelper.pollUiThread(
                () -> {
                    View queryEdit =
                            mActivityTestRule.getActivity().findViewById(R.id.search_query);
                    return queryEdit != null && queryEdit.getWidth() == originalWidth;
                },
                "Query field should grow back with the container.");
        assertQueryScrollMatchesWidth("after restoring the container");
    }

    /**
     * Resizes the settings container inside a {@link ChangeBounds} transition that targets the
     * settings view and all of its descendants, as the side UI does (see {@code
     * CompositorViewHolder#onPreSideUiSpecsChange}). A plain relayout does not reproduce the bug:
     * ChangeBounds resizes the views through {@code View#setLeftTopRightBottom} without a measure
     * pass, which is what leaves the TextView scroll offset stale. Blocks until the transition has
     * ended.
     */
    private void resizeSettingsContainerWithChangeBounds(int width) {
        AtomicBoolean transitionEnded = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View settingsActivity =
                            mActivityTestRule.getActivity().findViewById(R.id.settings_activity);
                    assertNotNull(settingsActivity);
                    ChangeBounds changeBounds = new ChangeBounds();
                    changeBounds.addTarget(settingsActivity);
                    List<View> descendants = new ArrayList<>();
                    ViewUtils.getAllDescendants(
                            settingsActivity, descendants, Collections.emptySet());
                    for (View view : descendants) changeBounds.addTarget(view);
                    Transition transition =
                            new TransitionSet()
                                    .setDuration(350L)
                                    .setOrdering(TransitionSet.ORDERING_TOGETHER)
                                    .addTransition(changeBounds)
                                    .addListener(
                                            new TransitionListenerAdapter() {
                                                @Override
                                                public void onTransitionEnd(Transition t) {
                                                    transitionEnded.set(true);
                                                }
                                            });
                    ViewGroup sceneRoot = (ViewGroup) settingsActivity.getRootView();
                    TransitionManager.beginDelayedTransition(sceneRoot, transition);
                });
        setSettingsContainerWidth(width);
        CriteriaHelper.pollUiThread(transitionEnded::get, "ChangeBounds transition should end.");
    }

    /**
     * Asserts that the query field's horizontal scroll offset is the one TextView computes for its
     * current width. A single-line TextView positions RTL text (and its hint) by scrolling a very
     * wide layout, so the offset depends on the width; a stale offset draws the hint shifted or
     * clipped. {@link TextView#bringPointIntoView} recomputes the offset and reports whether it had
     * to change it.
     */
    private void assertQueryScrollMatchesWidth(String when) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TextView queryEdit =
                            mActivityTestRule.getActivity().findViewById(R.id.search_query);
                    int scrollX = queryEdit.getScrollX();
                    queryEdit.bringPointIntoView(queryEdit.getSelectionEnd());
                    assertEquals(
                            "Query field scroll offset should match its width " + when,
                            queryEdit.getScrollX(),
                            scrollX);
                });
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

    /** Regression test for https://crbug.com/558516224. */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testThemeSwitchWithSelectLanguageFragment() {
        // Open Settings and wait for it to finish loading.
        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withText(R.string.search_engine_settings));

        // Navigate to Language settings in the header pane.
        var headerRecyclerViewMatcher =
                allOf(withId(R.id.recycler_view), isDescendantOfA(withId(R.id.preferences_header)));
        waitForView(headerRecyclerViewMatcher)
                .performViewAction(scrollTo(hasDescendant(withText(R.string.language_settings))));

        // Click on Language settings.
        var languageSettingMatcher =
                allOf(
                        isDescendantOfA(withId(R.id.preferences_header)),
                        withText(R.string.language_settings));
        waitForView(languageSettingMatcher).click();

        // Open SelectLanguageFragment ("Add language") and ensure the language list is displayed.
        waitForView(withId(org.chromium.chrome.browser.language.R.id.add_language)).click();
        waitForView(withId(org.chromium.chrome.browser.language.R.id.language_list));

        // Simulate theme switch / activity recreation.
        mActivityTestRule.recreateActivity();

        // Verify Toolbar/Action Bar is restored and displayed without crashing.
        waitForView(withId(R.id.action_bar));
    }

    /**
     * Regression test for https://crbug.com/559529471. Verifies that the search box and query
     * container align horizontally with preference items in single-column mode on narrow screens
     * when SettingsInTab is enabled.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testSearchBoxAlignmentInSingleColumn_narrowScreen() {
        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withText(R.string.search_engine_settings));

        // Resize container to narrow width (500px <= 632dp threshold) to force single-column mode.
        int narrowWidth = 500;
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
                            parent,
                            "SettingsPageTest.testSearchBoxAlignmentInSingleColumn_narrowScreen");
                });

        // Poll until single-column mode is established and search_box and preference items
        // are laid out inside the resized container.
        CriteriaHelper.pollUiThread(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    View appBar = activity.findViewById(R.id.app_bar_layout);
                    View searchBox = activity.findViewById(R.id.search_box);
                    RecyclerView rv = activity.findViewById(R.id.recycler_view);
                    return appBar != null
                            && appBar.getWidth() == narrowWidth
                            && searchBox != null
                            && searchBox.getParent() == appBar
                            && searchBox.getWidth() > 0
                            && searchBox.getWidth() < narrowWidth
                            && rv != null
                            && rv.getChildCount() > 0
                            && rv.getChildAt(0).getWidth() > 0;
                });

        // Get the horizontal screen bounds of the first preference item in the list.
        Rect firstItemBounds = new Rect();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    RecyclerView rv = activity.findViewById(R.id.recycler_view);
                    assertNotNull(rv);
                    assertTrue(rv.getChildCount() > 0);
                    View firstItem = rv.getChildAt(0);
                    int[] loc = new int[2];
                    firstItem.getLocationOnScreen(loc);
                    firstItemBounds.set(
                            loc[0],
                            loc[1],
                            loc[0] + firstItem.getWidth(),
                            loc[1] + firstItem.getHeight());
                });

        // Capture search_box screen bounds and verify they align with the preference items.
        Rect searchBoxBounds = getViewScreenBounds(R.id.search_box);
        assertEquals(
                "Search box left edge should align with preference items",
                firstItemBounds.left,
                searchBoxBounds.left);
        assertEquals(
                "Search box right edge should align with preference items",
                firstItemBounds.right,
                searchBoxBounds.right);

        // Tap search box to enter search state.
        waitForView(withId(R.id.search_box)).click();
        waitForView(withId(R.id.search_query_container));

        // Verify search_query_container matches search_box and preference items horizontal bounds.
        Rect queryBounds = getViewScreenBounds(R.id.search_query_container);
        assertEquals(
                "Search query container left edge should align with preference items",
                firstItemBounds.left,
                queryBounds.left);
        assertEquals(
                "Search query container right edge should align with preference items",
                firstItemBounds.right,
                queryBounds.right);

        // Tap on back arrow to exit search state.
        clickSearchBackArrow();
        waitForView(withId(R.id.search_box));

        // Verify search_box still aligns with preference items after exiting search.
        Rect searchBoxBoundsAfterExit = getViewScreenBounds(R.id.search_box);
        assertEquals(
                "Search box left edge should align after exiting search",
                firstItemBounds.left,
                searchBoxBoundsAfterExit.left);
        assertEquals(
                "Search box right edge should align after exiting search",
                firstItemBounds.right,
                searchBoxBoundsAfterExit.right);
    }

    /**
     * Regression test for https://crbug.com/563047017 under SettingsInTabUrlNav. Loading root
     * settings in two-column mode fills the detail pane with a default page. Leaving two-column
     * mode must then show root settings rather than that page, which the user never navigated to.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testRootSettingsShownAfterLeavingTwoColumn_urlNav() {
        FeatureOverrides.enable(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV);
        assumeTwoColumnScreenWidth();

        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withText(R.string.search_engine_settings));
        ensureTwoColumnMode();

        // Resize container to narrow width (500px <= 632dp threshold) to force single-column mode.
        setSettingsContainerWidth(500);
        try {
            ensureSingleColumnMode();
            CriteriaHelper.pollUiThread(
                    () -> {
                        var hostFragment =
                                SettingsHostFragment.get(mActivityTestRule.getActivity());
                        return hostFragment != null
                                && hostFragment.getMainFragment() instanceof MainSettings;
                    },
                    "Root settings should be shown after leaving two-column mode.");
        } finally {
            setSettingsContainerWidth(ViewGroup.LayoutParams.MATCH_PARENT);
        }
    }

    /** Sets the width of the view containing the settings page and requests a layout pass. */
    private void setSettingsContainerWidth(int width) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    View settingsActivity = activity.findViewById(R.id.settings_activity);
                    assertNotNull(settingsActivity);
                    View parent = (View) settingsActivity.getParent();
                    var lp = parent.getLayoutParams();
                    lp.width = width;
                    parent.setLayoutParams(lp);
                    ViewUtils.requestLayout(parent, "SettingsPageTest.setSettingsContainerWidth");
                });
    }

    /**
     * Regression test for crbug.com/562619494: when display density changes such that the scaled
     * width drops below 600dp (the tablet threshold), the tab should still host SettingsPage, the
     * search box should remain functional, and no orphaned fragments should remain.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testDensityChangeKeepsSettingsInTab() {
        mFinishActivity = true;
        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withText(R.string.search_engine_settings));

        Configuration originalConfig =
                new Configuration(
                        mActivityTestRule.getActivity().getResources().getConfiguration());
        try {
            // Change display density so scaled width drops below 600dp.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        var activity = mActivityTestRule.getActivity();
                        Configuration config =
                                new Configuration(activity.getResources().getConfiguration());
                        config.densityDpi *= 2;
                        config.smallestScreenWidthDp =
                                Math.min(config.smallestScreenWidthDp / 2, 400);
                        activity.getResources()
                                .updateConfiguration(
                                        config, activity.getResources().getDisplayMetrics());
                    });

            // Force activity recreation for the configuration change.
            mActivityTestRule.recreateActivity();

            // 1. Verify the tab still hosts SettingsPage, and 2. no orphaned fragments remain.
            SettingsHostFragment hostFragment =
                    waitForSettingsPageInTab(
                            "Tab should still host SettingsPage after density change.");

            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            assertTrue(
                                    "Settings should still be shown in tab",
                                    SettingsHostUtil.isShownInTab(hostFragment)));

            // 3. Verify the search box works after recreation.
            waitForView(withId(R.id.search_box)).click();
            waitForView(withId(R.id.search_query_container));
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        var activity = mActivityTestRule.getActivity();
                        if (activity != null) {
                            activity.getResources()
                                    .updateConfiguration(
                                            originalConfig,
                                            activity.getResources().getDisplayMetrics());
                        }
                    });
        }
    }

    /**
     * Regression test for crbug.com/562619494: when an unfolded foldable device (>= 600dp) opens
     * Settings in a tab and then folds (< 600dp), Settings remains functional in the tab.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testFoldKeepsSettingsInTab() {
        mFinishActivity = true;
        mActivityTestRule.loadUrl("chrome-native://settings/");
        waitForView(withText(R.string.search_engine_settings));

        Configuration originalConfig =
                new Configuration(
                        mActivityTestRule.getActivity().getResources().getConfiguration());
        try {
            // Simulate folding by updating configuration smallestScreenWidthDp to phone size (<
            // 600dp).
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        var activity = mActivityTestRule.getActivity();
                        Configuration config =
                                new Configuration(activity.getResources().getConfiguration());
                        config.smallestScreenWidthDp = 400;
                        activity.getResources()
                                .updateConfiguration(
                                        config, activity.getResources().getDisplayMetrics());
                    });

            mActivityTestRule.recreateActivity();

            // Verify the tab still hosts SettingsPage after folding.
            SettingsHostFragment hostFragment =
                    waitForSettingsPageInTab("Tab should still host SettingsPage after fold.");

            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            assertTrue(
                                    "Settings should still be shown in tab after fold",
                                    SettingsHostUtil.isShownInTab(hostFragment)));

            // Verify settings content and search box remain visible and interactive.
            waitForView(withId(R.id.search_box));
            waitForView(withText(R.string.search_engine_settings));
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        var activity = mActivityTestRule.getActivity();
                        if (activity != null) {
                            activity.getResources()
                                    .updateConfiguration(
                                            originalConfig,
                                            activity.getResources().getDisplayMetrics());
                        }
                    });
        }
    }

    /**
     * Regression test for crbug.com/562619494. On phone form factor, typing chrome://settings opens
     * a Settings tab rather than launching SettingsActivity.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testSettingsUrlOpensInTabOnPhone() {
        // Use the normalized URL with a trailing slash. The navigation commits
        // "chrome://settings/", and loadUrl() waits for that exact URL to load.
        mActivityTestRule.loadUrl("chrome://settings/");

        // Verify the settings page is loaded in the tab.
        waitForView(withText(R.string.search_engine_settings));

        SettingsHostFragment hostFragment =
                waitForSettingsPageInTab("Tab should host a SettingsPage on phone.");

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertTrue(
                                "Settings should be shown in tab",
                                SettingsHostUtil.isShownInTab(hostFragment)));
    }

    /**
     * Polls until the activity tab hosts a {@link SettingsPage} whose {@link SettingsHostFragment}
     * is attached, shown and has an active fragment, then returns that host fragment.
     *
     * <p>Callers must poll rather than read the host fragment directly, because {@link
     * ChromeTabbedActivityTestRule#recreateActivity()} only waits for the activity to reach {@code
     * Stage.RESUMED}. The native page and the host fragment's view are restored asynchronously
     * after that, and {@link SettingsHostFragment#get} returns null until the fragment's view is
     * attached and shown. Asserting immediately made these tests flaky. See
     * https://crbug.com/562619494.
     *
     * @param message Failure message describing the settings state being waited for.
     */
    private SettingsHostFragment waitForSettingsPageInTab(String message) {
        CriteriaHelper.pollUiThread(
                () -> {
                    var activity = mActivityTestRule.getActivity();
                    if (activity == null) return false;
                    Tab tab = activity.getActivityTab();
                    if (tab == null || !(tab.getNativePage() instanceof SettingsPage)) return false;
                    var hostFragment = SettingsHostFragment.get(activity);
                    return hostFragment != null && hostFragment.getActiveFragment() != null;
                },
                message);
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    assertNotNull(message, hostFragment);
                    return hostFragment;
                });
    }

    /**
     * Clicks the search back arrow to exit search state. Uses {@link View#performClick()} on the UI
     * thread after waiting for the view to be displayed because {@code R.id.back_arrow_icon} has a
     * {@code TooltipCompat} long-click listener: if the main thread stalls for >400ms between
     * {@code ACTION_DOWN} and {@code ACTION_UP} during a synthetic Espresso tap, the tooltip
     * long-press consumes the gesture and {@link View#onTouchEvent} skips {@code performClick()}.
     * https://crbug.com/562493964
     */
    private void clickSearchBackArrow() {
        waitForView(withId(R.id.back_arrow_icon));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View backArrow =
                            mActivityTestRule.getActivity().findViewById(R.id.back_arrow_icon);
                    assertNotNull(backArrow);
                    assertTrue(backArrow.performClick());
                });
    }

    private void assumeTwoColumnScreenWidth() {
        // The test requires an emulator wide enough to use two-column mode.
        Resources res = mActivityTestRule.getActivity().getResources();
        int minWidth = res.getDimensionPixelSize(R.dimen.settings_min_multi_column_screen_width);
        int screenWidth = res.getDisplayMetrics().widthPixels;
        Assume.assumeTrue("Test requires two-column mode.", screenWidth >= minWidth);
    }

    private void ensureTwoColumnMode() {
        CriteriaHelper.pollUiThread(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    return hostFragment != null && hostFragment.isTwoColumnSettingsVisible();
                },
                "Settings should be shown in two-column mode.");
    }

    private void ensureSingleColumnMode() {
        CriteriaHelper.pollUiThread(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    return hostFragment != null && !hostFragment.isTwoColumnSettingsVisible();
                },
                "Settings should be shown in single-column mode.");
    }

    /** Returns the current tab's native page, or null. */
    private Object getCurrentNativePage() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getActivityTab().getNativePage());
    }

    /** Returns the fragment shown as the settings main content, or null. */
    private Fragment getSettingsMainFragment() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var hostFragment = SettingsHostFragment.get(mActivityTestRule.getActivity());
                    return hostFragment != null ? hostFragment.getMainFragment() : null;
                });
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
