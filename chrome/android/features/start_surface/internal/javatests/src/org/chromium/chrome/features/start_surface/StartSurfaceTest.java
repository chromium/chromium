// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.os.Build.VERSION_CODES.M;
import static android.os.Build.VERSION_CODES.N;
import static android.os.Build.VERSION_CODES.P;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import static org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS;
import static org.chromium.chrome.features.start_surface.InstantStartTest.createTabStateFile;
import static org.chromium.chrome.features.start_surface.InstantStartTest.createThumbnailBitmapAndWriteToFile;
import static org.chromium.chrome.features.start_surface.StartSurfaceMediator.FEED_VISIBILITY_CONSISTENCY;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.support.test.uiautomator.UiDevice;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import com.google.android.material.appbar.AppBarLayout;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.FeedSurfaceMediator;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.SingleTabSwitcherMediator;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorTestingRobot;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.start_surface.R;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests of the {@link StartSurface} for cases with tabs. See {@link
 * StartSurfaceNoTabsTest} for test that have no tabs.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class StartSurfaceTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false, false).name("NoInstant_NoReturn"),
                    new ParameterSet().value(true, false).name("Instant_NoReturn"),
                    new ParameterSet().value(false, true).name("NoInstant_Return"),
                    new ParameterSet().value(true, true).name("Instant_Return"));

    private static final String BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:start_surface_variation";

    private static final long MAX_TIMEOUT_MS = 30000L;

    /** Somehow {@link ViewActions#swipeUp} couldn't be performed */
    private static final ViewAction SWIPE_UP_FROM_CENTER = new GeneralSwipeAction(
            Swipe.FAST, GeneralLocation.CENTER, GeneralLocation.TOP_CENTER, Press.FINGER);

    /** {@link ViewActions#swipeDown} can wrongly touch the omnibox. */
    private static final ViewAction SWIPE_DOWN_FROM_CENTER = new GeneralSwipeAction(
            Swipe.FAST, GeneralLocation.CENTER, GeneralLocation.BOTTOM_CENTER, Press.FINGER);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private final boolean mUseInstantStart;
    private final boolean mImmediateReturn;

    public StartSurfaceTest(boolean useInstantStart, boolean immediateReturn) {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.INSTANT_START, useInstantStart);

        mUseInstantStart = useInstantStart;
        mImmediateReturn = immediateReturn;
    }

    /**
     * Only launch Chrome without waiting for a current tab.
     * This test could not use {@link ChromeActivityTestRule#startMainActivityFromLauncher()}
     * because of its {@link org.chromium.chrome.browser.tab.Tab} dependency.
     */
    private void startMainActivityFromLauncher() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.launchActivity(intent);
    }

    private boolean isInstantReturn() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START) && mImmediateReturn;
    }

    private void pressHome() {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.pressHome();
        ChromeApplicationTestUtils.waitUntilChromeInBackground();
    }

    private void pressBack() {
        // ChromeTabbedActivity expects the native libraries to be loaded when back is pressed.
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        Espresso.pressBack();
    }

    @Before
    public void setUp() throws IOException {
        // Scrolling tests need more tabs.
        String scrollMode = StartSurfaceConfiguration.START_SURFACE_OMNIBOX_SCROLL_MODE.getValue();
        int expectedTabs = scrollMode.isEmpty() ? 1 : 16;
        int additionalTabs = expectedTabs - (mImmediateReturn ? 0 : 1);
        if (additionalTabs > 0) {
            int[] tabIDs = new int[additionalTabs];
            for (int i = 0; i < additionalTabs; i++) {
                tabIDs[i] = i;
                createThumbnailBitmapAndWriteToFile(i);
            }
            InstantStartTest.createTabStateFile(tabIDs);
        }
        if (mImmediateReturn) {
            TAB_SWITCHER_ON_RETURN_MS.setForTesting(0);
            assertEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
            assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

            // Need to start main activity from launcher for immediate return to be effective.
            // However, need at least one tab for carousel to show, which starting main activity
            // from launcher doesn't provide.
            // Creating tabs and restart the activity would do the trick, but we cannot do that for
            // Instant start because we cannot unload native library.
            // Create fake TabState files to emulate having one tab in previous session.
            TabAttributeCache.setTitleForTesting(0, "tab title");
            startMainActivityFromLauncher();
        } else {
            assertFalse(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));
            // Cannot use startMainActivityFromLauncher().
            // Otherwise tab switcher could be shown immediately if single-pane is enabled.
            mActivityTestRule.startMainActivityOnBlankPage();
            onViewWaiting(withId(R.id.home_button));
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableIf.Build(sdk_is_less_than = P, message = "crbug.com/1084176")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1084176")
    @CommandLineFlags.Add({BASE_PARAMS + "/tasksonly"})
    public void testShow_TasksOnly() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) {
            TabUiTestHelper.enterTabSwitcher(cta);
        }
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.trendy_terms_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getLayoutManager().hideOverview(false));
        assertFalse(cta.getLayoutManager().overviewVisible());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableIf.Build(hardware_is = "bullhead", message = "crbug.com/1081657")
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/omniboxonly" +
            "/hide_switch_when_no_incognito_tabs/true"})
    public void testShow_OmniboxOnly() {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) {
            TabUiTestHelper.enterTabSwitcher(cta);
        }
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.trendy_terms_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        TabUiTestHelper.createTabs(cta, true, 1);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 1);
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        TabUiTestHelper.enterTabSwitcher(cta);
        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(isDisplayed()));
        }
        TestThreadUtils.runOnUiThreadBlocking(
                () -> cta.getTabModelSelector().getModel(true).closeAllTabs());
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));
        assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/trendyterms" +
            "/hide_switch_when_no_incognito_tabs/true"})
    public void testShow_TrendyTerms() {
        // TODO(https://crbug.com/1102288) Reenable this test.
        if (!mUseInstantStart && mImmediateReturn) {
          return;
        }

        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) {
            TabUiTestHelper.enterTabSwitcher(cta);
        }
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.trendy_terms_recycler_view))
                .check(matches(withEffectiveVisibility(VISIBLE)));

        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        TabUiTestHelper.createTabs(cta, true, 1);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 1);
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        TabUiTestHelper.enterTabSwitcher(cta);
        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(isDisplayed()));
        }
        TestThreadUtils.runOnUiThreadBlocking(
                () -> cta.getTabModelSelector().getModel(true).closeAllTabs());
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));
        assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableIf.Build(sdk_is_less_than = P, message = "crbug.com/1081822")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1081822")
    @CommandLineFlags.Add({BASE_PARAMS + "/twopanes"})
    public void testShow_TwoPanes() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) {
            TabUiTestHelper.enterTabSwitcher(cta);
        }
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(withId(R.id.ss_bottom_bar)).check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.trendy_terms_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        onView(withId(R.id.ss_explore_tab)).perform(click());
        onViewWaiting(withId(R.id.start_surface_explore_view));

        pressBack();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));
        assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single/home_button_on_grid_tab_switcher/false"})
    @FlakyTest(message = "https://crbug.com/1207306")
    public void testShow_SingleAsHomepage() {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(withId(R.id.search_box_text)).check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_title))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.trendy_terms_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO(crbug.com/1186752): Investigate whether this would be a problem for real users.
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        waitForView(allOf(withParent(withId(R.id.secondary_tasks_surface_view)),
                withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)));
        assertEquals(mActivityTestRule.getActivity()
                             .findViewById(R.id.home_button_on_tab_switcher)
                             .getVisibility(),
                View.GONE);

        pressBack();
        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(
                allOf(withParent(withId(
                              org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                        withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        hideWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_NoIncognitoSwitch() {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        waitForOverviewVisible();

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(withId(R.id.search_box_text));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_title))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.trendy_terms_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
        onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                .check(matches(withEffectiveVisibility(GONE)));

        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO(crbug.com/1186752): Investigate whether this would be a problem for real users.
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                .check(matches(withEffectiveVisibility(VISIBLE)));

        pressBack();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        if (isInstantReturn()
                && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                        && Build.VERSION.SDK_INT < Build.VERSION_CODES.O)) {
            // TODO(crbug.com/1139515): Fix incognito_toggle_tabs visibility AssertionFailedError
            // issue.
            // TODO(crbug.com/1092642): Fix androidx.test.espresso.PerformException issue when
            // performing a single click on position: 0. See code below.
            return;
        }

        onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                .check(matches(withEffectiveVisibility(GONE)));

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onView(allOf(withParent(withId(
                             org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                       withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        hideWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single" +
        "/exclude_mv_tiles/true/hide_switch_when_no_incognito_tabs/true"})
    public void testShow_SingleAsHomepage_NoMVTiles() {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(withId(R.id.search_box_text));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_title))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.trendy_terms_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }

        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO(crbug.com/1186752): Investigate whether this would be a problem for real users.
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));

        pressBack();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        if (isInstantReturn()
                && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                        && Build.VERSION.SDK_INT < Build.VERSION_CODES.O)) {
            // TODO(crbug.com/1092642): Fix androidx.test.espresso.PerformException issue when
            // performing a single click on position: 0. See code below.
            return;
        }

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onView(allOf(withParent(withId(
                             org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                       withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        hideWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/exclude_mv_tiles/true" +
        "/hide_switch_when_no_incognito_tabs/true/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepage_SingleTabNoMVTiles() {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(withId(R.id.search_box_text));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_title))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.single_tab_view))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.trendy_terms_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }
        onViewWaiting(allOf(
                withId(org.chromium.chrome.tab_ui.R.id.tab_title_view), withText(not(is("")))));

        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO(crbug.com/1186752): Investigate whether this would be a problem for real users.
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        pressBack();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.single_tab_view)).perform(click());
        hideWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsTabSwitcher() {
        if (mImmediateReturn) {
            waitForOverviewVisible();
            waitForTabModel();
            if (isInstantReturn()) {
                // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
                // omnibox.
                return;
            }
            // Single surface is shown as homepage. Clicks "more_tabs" button to get into tab
            // switcher.
            try {
                TestThreadUtils.runOnUiThreadBlocking(
                        ()
                                -> mActivityTestRule.getActivity()
                                           .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                           .performClick());
            } catch (ExecutionException e) {
                fail("Failed to tap 'more tabs' " + e.toString());
            }
        } else {
            TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        }

        onViewWaiting(withId(R.id.secondary_tasks_surface_view));

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(allOf(withParent(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body)),
                              withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        hideWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.P, message = "Flaky, see crbug.com/1169673")
    public void testShow_SingleAsHomepage_FromResumeShowStart() throws Exception {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager() != null && cta.getLayoutManager().overviewVisible());
        waitForTabModel();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getTabModelSelector().getModel(false).closeAllTabs(); });
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertTrue(cta.getLayoutManager().overviewVisible());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> cta.getTabCreator(true /*incognito*/).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 1);

        // Simulates pressing the Android's home button and bringing Chrome to the background.
        pressHome();

        // Simulates pressing Chrome's icon and launching Chrome from warm start.
        mActivityTestRule.resumeMainActivityFromLauncher();

        waitForTabModel();
        assertTrue(cta.getTabModelSelector().getCurrentModel().isIncognito());
        if (mImmediateReturn) {
            waitForOverviewVisible();
            onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        } else {
            onViewWaiting(withId(R.id.new_tab_incognito_container));
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single/omnibox_focused_on_new_tab/false"})
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.O, message = "Flaky, see crbug.com/1170673")
    public void testSearchInSingleSurface() {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();
        waitForTabModel();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(1));

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        onViewWaiting(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(2));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabCreator(false).launchNTP());
        waitForOverviewVisible();
        onViewWaiting(withId(R.id.search_box_text));
        TextView urlBar = mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertFalse(urlBar.isFocused());
        onView(withId(R.id.search_box_text)).perform(click());
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    @DisabledTest(message = "http://crbug/1120698 - NoInstant_Return version is flaky on bots.")
    public void testSearchInIncognitoSingleSurface() {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();
        waitForTabModel();
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): hide toolbar to make incognito switch visible.
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
            });

            // TODO(crbug.com/1097001): remove after fixing the default focus issue, which might
            // relate to crbug.com/1076274 above since it doesn't exist for the other combinations.
            assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        } else {
            onViewWaiting(withId(R.id.incognito_toggle_tabs)).perform(click());
        }
        assertTrue(mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(1));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testTapMVTilesInSingleSurface() {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();
        waitForTabModel();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(1));

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout))
                .perform(new ViewAction() {
                    @Override
                    public Matcher<View> getConstraints() {
                        return isDisplayed();
                    }

                    @Override
                    public String getDescription() {
                        return "Click the first child in MV tiles.";
                    }

                    @Override
                    public void perform(UiController uiController, View view) {
                        ViewGroup mvTilesContainer = (ViewGroup) view;
                        mvTilesContainer.getChildAt(0).performClick();
                    }
                });
        hideWatcher.waitForBehavior();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(2));
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        // Press back button should close the tab opened from the Start surface.
        OverviewModeBehaviorWatcher showWatcher =
                TabUiTestHelper.createOverviewShowWatcher(mActivityTestRule.getActivity());
        pressBack();
        showWatcher.waitForBehavior();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(1));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/open_ntp_instead_of_start/true"})
    public void testCreateNewTab_OpenNTPInsteadOfStart() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Create a new tab from menu should create NTP instead of showing start.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, false);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        CriteriaHelper.pollUiThread(() -> !cta.getOverviewModeBehavior().overviewVisible());
        TabUiTestHelper.enterTabSwitcher(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        // Click plus button from top toolbar should create NTP instead of showing start surface.
        onViewWaiting(withId(R.id.new_tab_button)).perform(click());
        TabUiTestHelper.verifyTabModelTabCount(cta, 3, 0);
        assertFalse(cta.getOverviewModeBehavior().overviewVisible());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @DisabledTest(message = "https://crbug.com/1119322")
    @CommandLineFlags.Add({BASE_PARAMS + "/single/open_ntp_instead_of_start/true"})
    public void testHomeButton_OpenNTPInsteadOfStart() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, false);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        mActivityTestRule.loadUrl("about:blank");
        CriteriaHelper.pollUiThread(()
                                            -> cta.getTabModelSelector()
                                                       .getCurrentTab()
                                                       .getOriginalUrl()
                                                       .getSpec()
                                                       .equals("about:blank"));

        // Click the home button should navigate to NTP instead of showing start surface.
        pressHomePageButton();
        CriteriaHelper.pollUiThread(
                () -> UrlUtilities.isNTPUrl(cta.getTabModelSelector().getCurrentTab().getUrl()));
        assertFalse(cta.getOverviewModeBehavior().overviewVisible());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @DisabledTest(message = "crbug.com/1118181")
    @DisableIf.Build(hardware_is = "bullhead", message = "crbug.com/1081657")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1108459")
    @CommandLineFlags.Add({BASE_PARAMS + "/omniboxonly" +
        "/hide_switch_when_no_incognito_tabs/true/omnibox_scroll_mode/top"})
    public void testScroll_Top() {
        // clang-format on
        // TODO(crbug.com/1082664): Make it work with NoReturn.
        assumeTrue(mImmediateReturn);

        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));

        onView(withId(org.chromium.chrome.tab_ui.R.id.scroll_component_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .perform(SWIPE_UP_FROM_CENTER, SWIPE_UP_FROM_CENTER, SWIPE_UP_FROM_CENTER);
        onView(withId(org.chromium.chrome.tab_ui.R.id.scroll_component_container))
                .check(matches(not(isDisplayed())));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .perform(SWIPE_DOWN_FROM_CENTER);
        onView(withId(org.chromium.chrome.tab_ui.R.id.scroll_component_container))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @DisabledTest(message = "crbug.com/1118181")
    @DisableIf.Build(sdk_is_less_than = P, message = "crbug.com/1083174")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1108459")
    @CommandLineFlags.Add({BASE_PARAMS + "/omniboxonly" +
        "/hide_switch_when_no_incognito_tabs/true/omnibox_scroll_mode/quick"})
    public void testScroll_Quick() {
        // clang-format on
        // TODO(crbug.com/1082664): Make it work with NoReturn.
        assumeTrue(mImmediateReturn);

        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));

        onView(withId(org.chromium.chrome.tab_ui.R.id.scroll_component_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .perform(SWIPE_UP_FROM_CENTER, SWIPE_UP_FROM_CENTER, SWIPE_UP_FROM_CENTER);
        onView(withId(org.chromium.chrome.tab_ui.R.id.scroll_component_container))
                .check(matches(not(isDisplayed())));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .perform(SWIPE_DOWN_FROM_CENTER);
        onView(withId(org.chromium.chrome.tab_ui.R.id.scroll_component_container))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @DisabledTest(message = "crbug.com/1118181")
    @DisableIf.Build(sdk_is_less_than = P, message = "crbug.com/1083459")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1083459")
    @CommandLineFlags.Add({BASE_PARAMS + "/omniboxonly" +
        "/hide_switch_when_no_incognito_tabs/true/omnibox_scroll_mode/pinned"})
    public void testScroll_Pinned() {
        // clang-format on
        // TODO(crbug.com/1082664): Make it work with NoReturn.
        assumeTrue(mImmediateReturn);

        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));

        onView(withId(org.chromium.chrome.tab_ui.R.id.scroll_component_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .perform(SWIPE_UP_FROM_CENTER, SWIPE_UP_FROM_CENTER, SWIPE_UP_FROM_CENTER);
        onView(withId(org.chromium.chrome.tab_ui.R.id.scroll_component_container))
                .check(matches(isDisplayed()));
    }

    private void waitForTabModel() {
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized,
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Tests that histograms are recorded only if the StartSurface is shown when Chrome is launched
     * from cold start.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true"})
    public void startSurfaceRecordHistogramsTest() {
        // clang-format on
        if (!mImmediateReturn) {
            assertNotEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
            pressHomePageButton();
        } else {
            assertEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        }

        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue());
        waitForOverviewVisible();
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        // Waits for the current Tab to complete loading. The deferred startup will be triggered
        // after the loading.
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        if (tab != null && tab.isLoading()) {
            CriteriaHelper.pollUiThread(()
                                                -> !tab.isLoading(),
                    MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        }
        assertTrue("Deferred startup never completed", mActivityTestRule.waitForDeferredStartup());

        boolean isInstantStart = TabUiFeatureUtilities.supportInstantStart(false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        StartSurfaceConfiguration.getHistogramName(
                                AsyncInitializationActivity.FIRST_DRAW_COMPLETED_TIME_MS_UMA,
                                isInstantStart)));
        int expectedRecordCount = mImmediateReturn ? 1 : 0;
        // Histograms should be only recorded when StartSurface is shown immediately after
        // launch.
        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        StartSurfaceConfiguration.getHistogramName(
                                SingleTabSwitcherMediator.SINGLE_TAB_TITLE_AVAILABLE_TIME_UMA,
                                isInstantStart)));

        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        StartSurfaceConfiguration.getHistogramName(
                                FeedSurfaceMediator.FEED_CONTENT_FIRST_LOADED_TIME_MS_UMA,
                                isInstantStart)));

        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        StartSurfaceConfiguration.getHistogramName(
                                FeedSurfaceCoordinator.FEED_STREAM_CREATED_TIME_MS_UMA,
                                isInstantStart)));

        Assert.assertEquals(isInstantReturn() ? 1 : 0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        StartSurfaceConfiguration.getHistogramName(
                                FeedLoadingCoordinator.FEEDS_LOADING_PLACEHOLDER_SHOWN_TIME_UMA,
                                true)));
        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramTotalCountForTesting(FEED_VISIBILITY_CONSISTENCY));
        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramValueCountForTesting(FEED_VISIBILITY_CONSISTENCY, 1));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_CloseAllTabsShouldHideTabSwitcher() {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForOverviewVisible();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        assertEquals(cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_title)
                             .getVisibility(),
                View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getTabModelSelector().getModel(false).closeAllTabs(); });
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertEquals(cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_title)
                             .getVisibility(),
                View.GONE);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface", "TabGroup"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @DisabledTest(message = "crbug.com/1148365")
    public void testCreateTabWithinTabGroup() throws Exception {
        // Create tab state files for a group with two tabs.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        createTabStateFile(new int[] {0, 1});

        // Restart and open tab grid dialog.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                             .getTabModelFilterProvider()
                                             .getTabModelFilter(false);
        if (mImmediateReturn) {
            onViewWaiting(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                                  withParent(withId(org.chromium.chrome.tab_ui.R.id
                                                            .carousel_tab_switcher_container))))
                    .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        } else {
            onViewWaiting(allOf(withId(org.chromium.chrome.tab_ui.R.id.toolbar_left_button),
                                  isDescendantOfA(withId(R.id.bottom_controls))))
                    .perform(click());
        }
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                        withParent(withId(org.chromium.chrome.tab_ui.R.id.dialog_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(2));

        // Show start surface through tab grid dialog toolbar plus button and create a new tab by
        // clicking on MV tiles.
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.toolbar_right_button),
                       isDescendantOfA(
                               withId(org.chromium.chrome.tab_ui.R.id.dialog_container_view))))
                .perform(click());
        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout))
                .perform(new ViewAction() {
                    @Override
                    public Matcher<View> getConstraints() {
                        return isDisplayed();
                    }

                    @Override
                    public String getDescription() {
                        return "Click the first child in MV tiles.";
                    }

                    @Override
                    public void perform(UiController uiController, View view) {
                        ViewGroup mvTilesContainer = (ViewGroup) view;
                        mvTilesContainer.getChildAt(0).performClick();
                    }
                });
        hideWatcher.waitForBehavior();

        // Verify a tab is created within the group by checking the tab strip and tab model.
        onView(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))
                .check(waitForView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                        isCompletelyDisplayed())));
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                       withParent(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(3));
        assertEquals(3, cta.getTabModelSelector().getCurrentModel().getCount());
        assertEquals(1, filter.getTabGroupCount());

        // Show start surface through tab strip plus button and create a new tab by perform a query
        // search in fake box.
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.toolbar_right_button),
                       isDescendantOfA(withId(org.chromium.chrome.tab_ui.R.id.bottom_controls))))
                .perform(click());
        onViewWaiting(withId(R.id.search_box_text))
                .check(matches(isCompletelyDisplayed()))
                .perform(replaceText("wfh tips"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Verify a tab is created within the group by checking the tab strip and tab model.
        onView(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))
                .check(waitForView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                        isCompletelyDisplayed())));
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                       withParent(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(4));
        assertEquals(4, cta.getTabModelSelector().getCurrentModel().getCount());
        assertEquals(1, filter.getTabGroupCount());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_VoiceSearchButtonShown() {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        waitForOverviewVisible();
        waitForTabModel();

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(withId(R.id.search_box_text)).check(matches(isDisplayed()));
        onView(withId(R.id.voice_search_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BottomSheet() {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        BottomSheetTestSupport bottomSheetTestSupport = new BottomSheetTestSupport(
                cta.getRootUiCoordinatorForTesting().getBottomSheetController());
        waitForOverviewVisible();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }

        /** Verifies the case of start surface -> a tab -> tab switcher -> start surface. */
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                        withParent(withId(
                                org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        TabUiTestHelper.enterTabSwitcher(cta);
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        assertTrue(bottomSheetTestSupport.hasSuppressionTokens());

        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        /** Verifies the case of navigating to a tab -> start surface -> tab switcher. */
        onViewWaiting(
                allOf(withParent(withId(
                              org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                        withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        pressHomePageButton();
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        assertTrue(bottomSheetTestSupport.hasSuppressionTokens());
    }

    private void pressHomePageButton() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getToolbarManager()
                    .getToolbarTabControllerForTesting()
                    .openHomepage();
        });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    @DisableIf.Build(sdk_is_less_than = N, message = "crbug.com/1185009")
    public void testShow_SingleAsHomepage_ResetScrollPosition() {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForOverviewVisible();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Scroll the toolbar.
        scrollToolbar();
        AppBarLayout taskSurfaceHeader =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.task_surface_header);
        assertNotEquals(taskSurfaceHeader.getBottom(), taskSurfaceHeader.getHeight());

        // Verifies the case of scrolling Start surface ->  tab switcher -> tap "+1" button ->
        // Start surface. The Start surface should reset its scroll position.
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }

        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container), isDisplayed()));

        assertEquals(taskSurfaceHeader.getBottom(), taskSurfaceHeader.getHeight());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisabledTest(message = "https://crbug.com/1176084")
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BackButton() throws ExecutionException {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForOverviewVisible();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Case 1:
        // Launches the first site in mv tiles, and press back button.
        LinearLayout tilesLayout =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout);
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container), isDisplayed()));
        TestThreadUtils.runOnUiThreadBlocking(() -> tilesLayout.getChildAt(0).performClick());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        // Verifies a new Tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        pressBack();

        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager().overviewVisible());
        // Verifies the new Tab is deleted.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Case 2:
        // Launches the first site in mv tiles, and press home button to return to the Start
        // surface.
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container), isDisplayed()));
        TestThreadUtils.runOnUiThreadBlocking(() -> tilesLayout.getChildAt(0).performClick());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        pressHomePageButton();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view), isDisplayed()));
        // Verifies a new Tab is created, and can be seen in the Start surface.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        // Launches the new tab from the carousel tab switcher, and press back button.
        onViewWaiting(
                allOf(withParent(withId(
                              org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                        withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(1, click()));
        Assert.assertEquals(TabLaunchType.FROM_START_SURFACE,
                cta.getTabModelSelector().getCurrentTab().getLaunchType());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        pressBack();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        // Verifies the tab isn't auto deleted from the TabModel.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BackButtonWithTabSwitcher() throws ExecutionException {
        // clang-format on
        singleAsHomepage_BackButtonWithTabSwitcher();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepageV2_BackButtonWithTabSwitcher() throws ExecutionException {
        // clang-format on
        singleAsHomepage_BackButtonWithTabSwitcher();
    }

    private void singleAsHomepage_BackButtonWithTabSwitcher() throws ExecutionException {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForOverviewVisible();
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container), isDisplayed()));
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Launches the first site in mv tiles.
        LinearLayout tilesLayout =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout);
        TestThreadUtils.runOnUiThreadBlocking(() -> tilesLayout.getChildAt(0).performClick());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        // Verifies a new Tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        if (isInstantReturn()
                && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                        && Build.VERSION.SDK_INT < Build.VERSION_CODES.O)) {
            // Fix the issue that failed to perform a single click on the tab switcher button.
            // See code below.
            return;
        }

        // Enters the tab switcher, and choose the new tab. After the tab is opening, press back.
        waitForView(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_button));
        TabUiTestHelper.enterTabSwitcher(cta);
        waitForView(withId(R.id.secondary_tasks_surface_view));
        waitForView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view));
        onView(allOf(withParent(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body)),
                       withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(1, click()));
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        Assert.assertEquals(TabLaunchType.FROM_START_SURFACE,
                cta.getTabModelSelector().getCurrentTab().getLaunchType());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue(StartSurfaceUserData.getKeepTab(
                                cta.getTabModelSelector().getCurrentTab())));

        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), true, false);
        pressBack();
        // Verifies the new Tab isn't deleted, and Start surface is shown.
        overviewModeWatcher.waitForBehavior();
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        // Verifies Chrome is closed.
        try {
            pressBack();
        } catch (Exception e) {
        } finally {
            CriteriaHelper.pollUiThread(
                    ()
                            -> {
                        return ActivityLifecycleMonitorRegistry.getInstance().getLifecycleStageOf(
                                       mActivityTestRule.getActivity())
                                == Stage.STOPPED;
                    },
                    "Tapping back button should close Chrome.", MAX_TIMEOUT_MS,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    @FlakyTest(message = "https://crbug.com/1185984")
    public void testShow_SingleAsHomepage_BackButtonOnCarouselTabSwitcher() {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForOverviewVisible();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.search_box_text))
                .perform(replaceText("about:blank"));
        onView(withId(org.chromium.chrome.start_surface.R.id.url_bar))
                .perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        TabUiTestHelper.mergeAllNormalTabsToAGroup(cta);
        pressHomePageButton();
        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager().overviewVisible());

        onViewWaiting(
                allOf(withParent(withId(
                              org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                        withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        onViewWaiting(allOf(
                withId(org.chromium.chrome.tab_ui.R.id.dialog_container_view), isDisplayed()));

        pressBack();
        waitForView(withId(org.chromium.chrome.tab_ui.R.id.dialog_container_view), VIEW_GONE);
        onView(withId(R.id.primary_tasks_surface_view)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BackButtonOnTabSwitcherWithDialogShowing()
            throws ExecutionException {
        // clang-format on
        backButtonOnTabSwitcherWithDialogShowingImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepageV2_BackButtonOnTabSwitcherWithDialogShowing()
            throws ExecutionException {
        // clang-format on
        backButtonOnTabSwitcherWithDialogShowingImpl();
    }

    private void backButtonOnTabSwitcherWithDialogShowingImpl() throws ExecutionException {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForOverviewVisible();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        onViewWaiting(withId(R.id.logo));

        // Launches the first site in mv tiles.
        LinearLayout tilesLayout =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout);
        TestThreadUtils.runOnUiThreadBlocking(() -> tilesLayout.getChildAt(0).performClick());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        // Verifies a new Tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        List<Tab> tabs =
                getTabsInCurrentTabModel(mActivityTestRule.getActivity().getCurrentTabModel());
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_button));
        TabUiTestHelper.enterTabSwitcher(cta);

        waitForView(withId(R.id.secondary_tasks_surface_view));
        StartSurfaceCoordinator startSurfaceCoordinator = getStartSurfaceFromUIThread();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> startSurfaceCoordinator.showTabSelectionEditorForTesting(tabs));
        robot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(
                        org.chromium.chrome.tab_ui.R.string.tab_selection_editor_group)
                .verifyToolbarSelectionTextWithResourceId(
                        org.chromium.chrome.tab_ui.R.string
                                .tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(tabs.size())
                .verifyHasAtLeastNItemVisible(2);

        // Verifies that tapping the back button will close the TabSelectionEditor.
        pressBack();
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));

        // Groups the two tabs.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> startSurfaceCoordinator.showTabSelectionEditorForTesting(tabs));
        robot.resultRobot.verifyToolbarActionButtonWithResourceId(
                org.chromium.chrome.tab_ui.R.string.tab_selection_editor_group);
        robot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarActionButton();
        robot.resultRobot.verifyTabSelectionEditorIsHidden();

        // Opens the TabGridDialog by clicking the first group card.
        onViewWaiting(Matchers.allOf(withParent(withId(
                                             org.chromium.chrome.tab_ui.R.id.tasks_surface_body)),
                              withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isTabGridDialogShown(cta));

        // Verifies that the TabGridDialog is closed by tapping back button.
        pressBack();
        CriteriaHelper.pollUiThread(() -> isTabGridDialogHidden(cta));
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BackButtonOnHomepageWithGroupTabsDialog()
        throws ExecutionException {
        // clang-format on
        backButtonOnHomepageWithGroupTabsDialogImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepageV2_BackButtonOnHomepageWithGroupTabsDialog()
        throws ExecutionException {
        // clang-format on
        backButtonOnHomepageWithGroupTabsDialogImpl();
    }

    private void backButtonOnHomepageWithGroupTabsDialogImpl() throws ExecutionException {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        waitForOverviewVisible();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        onViewWaiting(withId(R.id.logo));

        // Launches the first site in MV tiles to create the second tab for grouping.
        LinearLayout tilesLayout =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout);
        TestThreadUtils.runOnUiThreadBlocking(() -> tilesLayout.getChildAt(0).performClick());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        // Verifies a new Tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        // When show_last_active_tab_only is enabled, we need to enter the tab switcher first to
        // initialize the secondary task surface which shows the TabSelectionEditor dialog.
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_button));
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        TabUiTestHelper.enterTabSwitcher(cta);
        waitForView(withId(R.id.secondary_tasks_surface_view));
        List<Tab> tabs =
                getTabsInCurrentTabModel(mActivityTestRule.getActivity().getCurrentTabModel());
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();

        // Enters the homepage, and shows the TabSelectionEditor dialog.
        pressHomePageButton();
        waitForView(withId(R.id.primary_tasks_surface_view));

        StartSurfaceCoordinator startSurfaceCoordinator = getStartSurfaceFromUIThread();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> startSurfaceCoordinator.showTabSelectionEditorForTesting(tabs));
        robot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(
                        org.chromium.chrome.tab_ui.R.string.tab_selection_editor_group)
                .verifyToolbarSelectionTextWithResourceId(
                        org.chromium.chrome.tab_ui.R.string
                                .tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(tabs.size())
                .verifyHasAtLeastNItemVisible(2);

        // Verifies that tapping the back button will close the TabSelectionEditor.
        pressBack();
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @DisableIf.Build(sdk_is_less_than = M, message = "https://crbug.com/1170553")
    @DisableIf.Build(sdk_is_less_than = N, supported_abis_includes = "x86",
            message = "https://crbug.com/1170553")
    @CommandLineFlags.Add({BASE_PARAMS + "/single/omnibox_focused_on_new_tab/true"})
    public void
    testOmnibox_FocusedOnNewTabInSingleSurface() {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();
        waitForTabModel();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(1));

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(()
                                            -> isKeyboardShown() && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(
                mActivityTestRule.getActivity().findViewById(R.id.toolbar_buttons).getVisibility(),
                View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                mActivityTestRule.getActivity().getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        // Navigates the new created Tab.
        TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.setText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Launches a new Tab from the newly navigated tab, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 3, 0);
        waitForView(withId(R.id.search_box_text));
        CriteriaHelper.pollUiThread(()
                                            -> isKeyboardShown() && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        // Navigates the Tab to show home button.
        TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.setText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Goes to the Start surface from tapping home button, and navigate from the Omnibox. The
        // new created Tab shouldn't get focus.
        pressHomePageButton();
        waitForOverviewVisible();

        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        waitForView(withId(R.id.primary_tasks_surface_view), VIEW_GONE);

        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 4, 0);
        waitForView(withId(R.id.search_box_text));
        waitForView(withId(R.id.toolbar_buttons));
        Assert.assertFalse(urlBar.isFocused());
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @DisableIf.Build(sdk_is_less_than = M, message = "https://crbug.com/1170553")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1170553")
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true" +
            "/exclude_mv_tiles/true/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurfaceV2() {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();
        waitForTabModel();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(1));

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(() -> isKeyboardShown() && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(
                mActivityTestRule.getActivity().findViewById(R.id.toolbar_buttons).getVisibility(),
                View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                mActivityTestRule.getActivity().getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        // Navigates the new created Tab.
        TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.setText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Launches a new Tab from the newly navigated tab, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 3, 0);
        waitForView(withId(R.id.search_box_text));
        CriteriaHelper.pollUiThread(() -> isKeyboardShown() && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        // Navigates the Tab to show home button.
        TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.setText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Goes to the Start surface from tapping home button, and navigate from the Omnibox. The
        // new created Tab shouldn't get focus.
        pressHomePageButton();
        waitForOverviewVisible();

        onView(allOf(withId(R.id.search_box_text), isDisplayed()))
                .perform(replaceText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        waitForView(withId(R.id.primary_tasks_surface_view), VIEW_GONE);

        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 4, 0);
        waitForView(withId(R.id.search_box_text));
        waitForView(withId(R.id.toolbar_buttons));
        Assert.assertFalse(urlBar.isFocused());
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @DisableIf.Build(sdk_is_less_than = M, message = "https://crbug.com/1170553")
    @DisableIf.Build(sdk_is_greater_than = M, supported_abis_includes = "x86",
            message = "https://crbug.com/1170553")
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurface_BackButtonDeleteBlankTab() {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();
        waitForTabModel();
        assertThat(
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount(),
                equalTo(1));

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(()
                                            -> isKeyboardShown() && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(
                mActivityTestRule.getActivity().findViewById(R.id.toolbar_buttons).getVisibility(),
                View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                mActivityTestRule.getActivity().getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        pressBack();
        waitForView(withId(R.id.primary_tasks_surface_view));
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 1, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single/home_button_on_grid_tab_switcher/true"})
    public void testHomeButtonOnTabSwitcher() {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }
        waitForOverviewVisible();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 1, 0);

        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO(crbug.com/1186752): Investigate whether this would be a problem for real users.
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }
        waitForView(withId(R.id.secondary_tasks_surface_view));
        onView(withId(org.chromium.chrome.tab_ui.R.id.home_button_on_tab_switcher))
                .check(matches(isDisplayed()));
        HomeButton homeButton = mActivityTestRule.getActivity().findViewById(
                org.chromium.chrome.tab_ui.R.id.home_button_on_tab_switcher);
        Assert.assertFalse(homeButton.isLongClickable());
        onView(withId(R.id.home_button_on_tab_switcher)).perform(click());

        onView(withId(R.id.primary_tasks_surface_view)).check(matches(isDisplayed()));
    }

    private boolean isKeyboardShown() {
        Activity activity = mActivityTestRule.getActivity();
        if (activity.getCurrentFocus() == null) return false;
        return mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                activity, activity.getCurrentFocus());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/exclude_mv_tiles/false"
            + "/new_home_surface_from_home_button/hide_mv_tiles_and_tab_switcher"})
    public void testNewSurfaceFromHomeButton(){
        // clang-format on
        assumeTrue(mImmediateReturn);
        waitForOverviewVisible();
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container), isDisplayed()));
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container));
        onViewWaiting(withId(R.id.start_tab_switcher_button));

        // Open a tab from search box and then press home button to come back to Start Surface.
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 1, 0);
        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.search_box_text))
                .perform(replaceText("about:blank"));
        onView(withId(org.chromium.chrome.start_surface.R.id.url_bar))
                .perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 2, 0);
        pressHomePageButton();

        // MV tiles and carousel tab switcher should not show anymore.
        waitForOverviewVisible();
        onViewWaiting(withId(R.id.start_tab_switcher_button));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/exclude_mv_tiles/false"
            + "/new_home_surface_from_home_button/hide_tab_switcher_only"})
    public void testNewSurfaceHideTabOnlyFromHomeButton(){
        // clang-format on
        assumeTrue(mImmediateReturn);
        waitForOverviewVisible();
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container));
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container));
        onViewWaiting(withId(R.id.start_tab_switcher_button));

        // Open a tab from search box and then press home button to come back to Start Surface.
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 1, 0);
        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onView(withId(org.chromium.chrome.start_surface.R.id.search_box_text))
                .perform(replaceText("about:blank"));
        onView(withId(org.chromium.chrome.start_surface.R.id.url_bar))
                .perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 2, 0);
        pressHomePageButton();

        // MV tiles should shown and carousel tab switcher should not show anymore.
        waitForOverviewVisible();
        onViewWaiting(withId(R.id.start_tab_switcher_button));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @DisableIf.Build(sdk_is_less_than = N, supported_abis_includes = "x86")
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_tabs_in_mru_order/true"})
    public void testShow_SingleAsHomepage_ShowTabsInMRUOrder() throws ExecutionException {
        // clang-format on
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager() != null && cta.getLayoutManager().overviewVisible());
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        onViewWaiting(withId(R.id.logo));
        Tab tab1 = cta.getCurrentTabModel().getTabAt(0);

        // Launches the first site in MV tiles.
        LinearLayout tilesLayout =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout);
        TestThreadUtils.runOnUiThreadBlocking(() -> tilesLayout.getChildAt(0).performClick());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        Tab tab2 = cta.getActivityTab();
        // Verifies that the titles of the two Tabs are different.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertNotEquals(tab1.getTitle(), tab2.getTitle()); });

        // Returns to the Start surface.
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), true, false);
        pressHomePageButton();
        overviewModeWatcher.waitForBehavior();
        waitForView(allOf(
                withParent(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)));

        RecyclerView recyclerView = mActivityTestRule.getActivity().findViewById(
                org.chromium.chrome.tab_ui.R.id.tab_list_view);
        assertEquals(2, recyclerView.getChildCount());
        // Verifies that the tabs are shown in MRU order: the first card in the carousel Tab
        // switcher is the last created Tab by tapping the MV tile; the second card is the Tab
        // created or restored in setup().
        RecyclerView.ViewHolder firstViewHolder = recyclerView.findViewHolderForAdapterPosition(0);
        TextView title1 =
                firstViewHolder.itemView.findViewById(org.chromium.chrome.tab_ui.R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab2.getTitle(), title1.getText()));

        RecyclerView.ViewHolder secondViewHolder = recyclerView.findViewHolderForAdapterPosition(1);
        TextView title2 =
                secondViewHolder.itemView.findViewById(org.chromium.chrome.tab_ui.R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab1.getTitle(), title2.getText()));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @DisableIf.Build(sdk_is_less_than = N, supported_abis_includes = "x86")
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_tabs_in_mru_order/true"})
    public void testShow_TabSwitcher_ShowTabsInMRUOrder() throws ExecutionException {
        // clang-format on
        tabSwitcher_ShowTabsInMRUOrderImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS +
        "/single/show_tabs_in_mru_order/true/show_last_active_tab_only/true"})
    public void testShowV2_TabSwitcher_ShowTabsInMRUOrder() throws ExecutionException {
        // clang-format on
        tabSwitcher_ShowTabsInMRUOrderImpl();
    }

    private void tabSwitcher_ShowTabsInMRUOrderImpl() throws ExecutionException {
        if (!mImmediateReturn) {
            pressHomePageButton();
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager() != null && cta.getLayoutManager().overviewVisible());
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        onViewWaiting(withId(R.id.logo));
        Tab tab1 = cta.getCurrentTabModel().getTabAt(0);

        // Launches the first site in MV tiles.
        LinearLayout tilesLayout =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout);
        TestThreadUtils.runOnUiThreadBlocking(() -> tilesLayout.getChildAt(0).performClick());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        Tab tab2 = cta.getActivityTab();

        // Verifies that the titles of the two Tabs are different.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertNotEquals(tab1.getTitle(), tab2.getTitle()); });

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        // Enter the Tab switcher.
        TabUiTestHelper.enterTabSwitcher(cta);
        waitForView(allOf(withParent(withId(R.id.secondary_tasks_surface_view)),
                withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)));

        ViewGroup secondaryTaskSurface =
                mActivityTestRule.getActivity().findViewById(R.id.secondary_tasks_surface_view);
        RecyclerView recyclerView =
                secondaryTaskSurface.findViewById(org.chromium.chrome.tab_ui.R.id.tab_list_view);
        assertEquals(2, recyclerView.getChildCount());
        // Verifies that the tabs are shown in MRU order: the first card in the Tab switcher is the
        // last created Tab by tapping the MV tile; the second card is the Tab created or restored
        // in setup().
        RecyclerView.ViewHolder firstViewHolder = recyclerView.findViewHolderForAdapterPosition(0);
        TextView title1 =
                firstViewHolder.itemView.findViewById(org.chromium.chrome.tab_ui.R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab2.getTitle(), title1.getText()));

        RecyclerView.ViewHolder secondViewHolder = recyclerView.findViewHolderForAdapterPosition(1);
        TextView title2 =
                secondViewHolder.itemView.findViewById(org.chromium.chrome.tab_ui.R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab1.getTitle(), title2.getText()));
    }

    private void scrollToolbar() {
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 1, 0);

        /* Drag the {@link R.id.placeholders_layout} to scroll the toolbar to the top. */
        int toY = -mActivityTestRule.getActivity().getResources().getDimensionPixelOffset(
                R.dimen.toolbar_height_no_shadow);
        TestTouchUtils.dragCompleteView(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(
                        org.chromium.chrome.tab_ui.R.id.tab_switcher_title),
                0, 0, 0, toY, 1);

        // The start surface toolbar should be scrolled up and not be displayed.
        assertThat(mActivityTestRule.getActivity()
                           .findViewById(R.id.tab_switcher_toolbar)
                           .getTranslationY(),
                lessThanOrEqualTo(
                        (float) -mActivityTestRule.getActivity()
                                .getResources()
                                .getDimensionPixelOffset(R.dimen.toolbar_height_no_shadow)));

        // Toolbar layout view should show.
        onViewWaiting(withId(R.id.toolbar));

        // The start surface toolbar should be scrolled up and not be displayed.
        onView(withId(R.id.tab_switcher_toolbar)).check(matches(not(isDisplayed())));

        // Check the toolbar's background color.
        ToolbarPhone toolbar =
                mActivityTestRule.getActivity().findViewById(org.chromium.chrome.R.id.toolbar);
        Assert.assertEquals(toolbar.getToolbarDataProvider().getPrimaryColor(),
                toolbar.getBackgroundDrawable().getColor());
    }

    private List<Tab> getTabsInCurrentTabModel(TabModel currentTabModel) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            tabs.add(currentTabModel.getTabAt(i));
        }
        return tabs;
    }

    private StartSurfaceCoordinator getStartSurfaceFromUIThread() {
        AtomicReference<StartSurface> startSurface = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            startSurface.set(
                    ((ChromeTabbedActivity) mActivityTestRule.getActivity()).getStartSurface());
        });
        return (StartSurfaceCoordinator) startSurface.get();
    }

    private boolean isTabGridDialogShown(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(org.chromium.chrome.tab_ui.R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.VISIBLE && dialogView.getAlpha() == 1f;
    }

    private boolean isTabGridDialogHidden(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(org.chromium.chrome.tab_ui.R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.GONE;
    }

    private void waitForOverviewVisible() {
        CriteriaHelper.pollUiThread(
                ()
                        -> mActivityTestRule.getActivity().getLayoutManager() != null
                        && mActivityTestRule.getActivity().getLayoutManager().overviewVisible(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}

// TODO(crbug.com/1033909): Add more integration tests.
