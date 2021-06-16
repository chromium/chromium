// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

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
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import static org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS;
import static org.chromium.chrome.features.start_surface.StartSurfaceMediator.FEED_VISIBILITY_CONSISTENCY;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForStableView;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.support.test.uiautomator.UiDevice;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import com.google.android.material.appbar.AppBarLayout;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
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
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.MvTilesLayout;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.SingleTabSwitcherMediator;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorTestingRobot;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.start_surface.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
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
import java.util.concurrent.TimeoutException;

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

    private static final long MAX_TIMEOUT_MS = 40000L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    /**
     * Whether feature {@link ChromeFeatureList.INSTANT_START} is enabled.
     */
    private final boolean mUseInstantStart;

    /**
     * Whether feature {@link ChromeFeatureList.TAB_SWITCHER_ON_RETURN} is enabled as "immediately".
     * When immediate return is enabled, the Start surface is showing when Chrome is launched.
     */
    private final boolean mImmediateReturn;

    private CallbackHelper mLayoutChangedCallbackHelper;
    private LayoutStateProvider.LayoutStateObserver mLayoutObserver;
    @LayoutType
    private int mCurrentlyActiveLayout;
    private FakeMostVisitedSites mMostVisitedSites;

    public StartSurfaceTest(boolean useInstantStart, boolean immediateReturn) {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.INSTANT_START, useInstantStart);

        mUseInstantStart = useInstantStart;
        mImmediateReturn = immediateReturn;
    }

    @Before
    public void setUp() throws IOException {
        mLayoutChangedCallbackHelper = new CallbackHelper();
        mMostVisitedSites = StartSurfaceTestUtils.setMVTiles(mSuggestionsDeps);

        int expectedTabs = 1;
        int additionalTabs = expectedTabs - (mImmediateReturn ? 0 : 1);
        if (additionalTabs > 0) {
            int[] tabIDs = new int[additionalTabs];
            for (int i = 0; i < additionalTabs; i++) {
                tabIDs[i] = i;
                StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(i);
            }
            StartSurfaceTestUtils.createTabStateFile(tabIDs);
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
            StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        } else {
            assertFalse(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));
            // Cannot use StartSurfaceTestUtils.startMainActivityFromLauncher().
            // Otherwise tab switcher could be shown immediately if single-pane is enabled.
            mActivityTestRule.startMainActivityOnBlankPage();
            onViewWaiting(withId(R.id.home_button));
        }

        if (isInstantReturn()) {
            // Assume start surface is shown immediately, and the LayoutStateObserver may miss the
            // first onFinishedShowing event.
            mCurrentlyActiveLayout = LayoutType.TAB_SWITCHER;
        }

        mLayoutObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onFinishedShowing(@LayoutType int layoutType) {
                mCurrentlyActiveLayout = layoutType;
                mLayoutChangedCallbackHelper.notifyCalled();
            }
        };
        mActivityTestRule.getActivity().getLayoutManagerSupplier().addObserver((manager) -> {
            if (manager.getActiveLayout() != null) {
                mCurrentlyActiveLayout = manager.getActiveLayout().getLayoutType();
                mLayoutChangedCallbackHelper.notifyCalled();
            }
            manager.addObserver(mLayoutObserver);
        });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single/home_button_on_grid_tab_switcher/false"})
    public void testShow_SingleAsHomepage() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(withId(R.id.search_box_text)).check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_title))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));

        StartSurfaceTestUtils.clickMoreTabs(cta);
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        waitForView(allOf(withParent(withId(R.id.secondary_tasks_surface_view)),
                withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)));
        assertEquals(cta.findViewById(R.id.home_button_on_tab_switcher).getVisibility(), View.GONE);

        pressBack();
        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));

        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        StartSurfaceTestUtils.clickFirstTabInCarousel();
        hideWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_NoIncognitoSwitch() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

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

        // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
        onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                .check(matches(withEffectiveVisibility(GONE)));

        StartSurfaceTestUtils.clickMoreTabs(cta);
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

        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        StartSurfaceTestUtils.clickFirstTabInCarousel();
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
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

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

        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }

        StartSurfaceTestUtils.clickMoreTabs(cta);
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

        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        StartSurfaceTestUtils.clickFirstTabInCarousel();
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
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

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

        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(org.chromium.chrome.tab_ui.R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }
        onViewWaiting(allOf(
                withId(org.chromium.chrome.tab_ui.R.id.tab_title_view), withText(not(is("")))));

        StartSurfaceTestUtils.clickMoreTabs(cta);
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        pressBack();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.single_tab_view)).perform(click());
        hideWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsTabSwitcher() {
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForOverviewVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
            StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());
            if (isInstantReturn()) {
                // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
                // omnibox.
                return;
            }
            // Single surface is shown as homepage. Clicks "more_tabs" button to get into tab
            // switcher.
            StartSurfaceTestUtils.clickMoreTabs(mActivityTestRule.getActivity());
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
    public void testShow_SingleAsHomepage_FromResumeShowStart() throws Exception {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager() != null && cta.getLayoutManager().overviewVisible());
        StartSurfaceTestUtils.waitForTabModel(cta);
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

        StartSurfaceTestUtils.waitForTabModel(cta);
        assertTrue(cta.getTabModelSelector().getCurrentModel().isIncognito());
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForOverviewVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
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
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(1));

        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        onViewWaiting(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(2));

        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        onViewWaiting(withId(R.id.search_box_text));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        Assert.assertFalse(urlBar.isFocused());
        waitForStableView(cta.findViewById(R.id.search_box_text));
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
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): hide toolbar to make incognito switch visible.
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { cta.getTabModelSelector().selectModel(true); });

            // TODO(crbug.com/1097001): remove after fixing the default focus issue, which might
            // relate to crbug.com/1076274 above since it doesn't exist for the other combinations.
            assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        } else {
            onViewWaiting(withId(R.id.incognito_toggle_tabs)).perform(click());
        }
        assertTrue(cta.getTabModelSelector().isIncognitoSelected());

        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(1));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testTapMVTilesInSingleSurface() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        // Press back button should close the tab opened from the Start surface.
        OverviewModeBehaviorWatcher showWatcher = TabUiTestHelper.createOverviewShowWatcher(cta);
        pressBack();
        showWatcher.waitForBehavior();
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(1));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/open_ntp_instead_of_start/true"})
    @FlakyTest(message = "https://crbug.com/1201548")
    public void testCreateNewTab_OpenNTPInsteadOfStart() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
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
        StartSurfaceTestUtils.waitForTabModel(cta);
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
        StartSurfaceTestUtils.pressHomePageButton(cta);
        CriteriaHelper.pollUiThread(
                () -> UrlUtilities.isNTPUrl(cta.getTabModelSelector().getCurrentTab().getUrl()));
        assertFalse(cta.getOverviewModeBehavior().overviewVisible());
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
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        } else {
            assertEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        }

        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue());
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
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
                                FeedPlaceholderCoordinator.FEEDS_PLACEHOLDER_SHOWN_TIME_UMA,
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
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
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
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1});

        // Restart and open tab grid dialog.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                             .getTabModelFilterProvider()
                                             .getTabModelFilter(false);
        if (mImmediateReturn) {
            StartSurfaceTestUtils.clickFirstTabInCarousel();
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
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 2);

        // Verify a tab is created within the group by checking the tab strip and tab model.
        onView(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))
                .check(waitForView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                        isCompletelyDisplayed())));
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                       withParent(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(3));
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
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

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
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        BottomSheetTestSupport bottomSheetTestSupport = new BottomSheetTestSupport(
                cta.getRootUiCoordinatorForTesting().getBottomSheetController());
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }

        /** Verifies the case of start surface -> a tab -> tab switcher -> start surface. */
        StartSurfaceTestUtils.clickFirstTabInCarousel();
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        TabUiTestHelper.enterTabSwitcher(cta);
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        assertTrue(bottomSheetTestSupport.hasSuppressionTokens());

        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        /** Verifies the case of navigating to a tab -> start surface -> tab switcher. */
        StartSurfaceTestUtils.clickFirstTabInCarousel();
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        StartSurfaceTestUtils.pressHomePageButton(cta);
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        StartSurfaceTestUtils.clickMoreTabs(cta);
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        assertTrue(bottomSheetTestSupport.hasSuppressionTokens());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_ResetScrollPosition() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Scroll the toolbar.
        StartSurfaceTestUtils.scrollToolbar(cta);
        AppBarLayout taskSurfaceHeader =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.task_surface_header);
        assertNotEquals(taskSurfaceHeader.getBottom(), taskSurfaceHeader.getHeight());

        // Verifies the case of scrolling Start surface ->  tab switcher -> tap "+1" button ->
        // Start surface. The Start surface should reset its scroll position.
        StartSurfaceTestUtils.clickMoreTabs(cta);

        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        // The Start surface should reset its scroll position.
        CriteriaHelper.pollInstrumentationThread(
                () -> taskSurfaceHeader.getBottom() == taskSurfaceHeader.getHeight());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BackButton() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);

        // Case 1:
        // Launches the first site in mv tiles, and press back button.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        pressBack();

        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager().overviewVisible());
        // Verifies the new Tab is deleted.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Case 2:
        // Launches the first site in mv tiles, and press home button to return to the Start
        // surface.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        StartSurfaceTestUtils.pressHomePageButton(cta);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view), isDisplayed()));

        // Launches the new tab from the carousel tab switcher, and press back button.
        StartSurfaceTestUtils.clickTabInCarousel(/* position = */ 1);
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
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BackButtonWithTabSwitcher() {
        singleAsHomepage_BackButtonWithTabSwitcher();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepageV2_BackButtonWithTabSwitcher() {
        // clang-format on
        singleAsHomepage_BackButtonWithTabSwitcher();
    }

    private void singleAsHomepage_BackButtonWithTabSwitcher() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container), isDisplayed()));

        // Launches the first site in mv tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);

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

        OverviewModeBehaviorWatcher overviewModeWatcher =
                new OverviewModeBehaviorWatcher(cta.getLayoutManager(), true, false);
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
                                       cta)
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
    public void testShow_SingleAsHomepage_BackButtonOnCarouselTabSwitcher() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.search_box_text))
                .perform(replaceText("about:blank"));
        onView(withId(org.chromium.chrome.start_surface.R.id.url_bar))
                .perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        TabUiTestHelper.mergeAllNormalTabsToAGroup(cta);
        StartSurfaceTestUtils.pressHomePageButton(cta);
        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager().overviewVisible());

        StartSurfaceTestUtils.clickFirstTabInCarousel();
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
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BackButtonOnTabSwitcherWithDialogShowing() {
        backButtonOnTabSwitcherWithDialogShowingImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepageV2_BackButtonOnTabSwitcherWithDialogShowing() {
        // clang-format on
        backButtonOnTabSwitcherWithDialogShowingImpl();
    }

    private void backButtonOnTabSwitcherWithDialogShowingImpl() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        onViewWaiting(withId(R.id.logo));

        // Launches the first site in mv tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);

        List<Tab> tabs = getTabsInCurrentTabModel(cta.getCurrentTabModel());
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_button));
        TabUiTestHelper.enterTabSwitcher(cta);

        waitForView(withId(R.id.secondary_tasks_surface_view));
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
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
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShow_SingleAsHomepage_BackButtonOnHomepageWithGroupTabsDialog() {
        backButtonOnHomepageWithGroupTabsDialogImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepageV2_BackButtonOnHomepageWithGroupTabsDialog() {
        // clang-format on
        backButtonOnHomepageWithGroupTabsDialogImpl();
    }

    private void backButtonOnHomepageWithGroupTabsDialogImpl() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        onViewWaiting(withId(R.id.logo));

        // Launches the first site in MV tiles to create the second tab for grouping.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);

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
        List<Tab> tabs = getTabsInCurrentTabModel(cta.getCurrentTabModel());
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();

        // Enters the homepage, and shows the TabSelectionEditor dialog.
        StartSurfaceTestUtils.pressHomePageButton(cta);
        waitForView(withId(R.id.primary_tasks_surface_view));

        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
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
    @CommandLineFlags.Add({BASE_PARAMS + "/single/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurface() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(cta.findViewById(R.id.toolbar_buttons).getVisibility(), View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                cta.getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        // Navigates the new created Tab.
        TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.setText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Launches a new Tab from the newly navigated tab, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 3, 0);
        waitForView(withId(R.id.search_box_text));
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_last_active_tab_only/true" +
            "/exclude_mv_tiles/true/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurfaceV2() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(cta.findViewById(R.id.toolbar_buttons).getVisibility(), View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                cta.getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        // Navigates the new created Tab.
        TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.setText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Launches a new Tab from the newly navigated tab, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 3, 0);
        waitForView(withId(R.id.search_box_text));
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_TabOpenedFromOmniboxShouldNotGetFocused() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        onViewWaiting(allOf(withId(R.id.search_box_text), isDisplayed()))
                .perform(replaceText("about:blank"));
        onViewWaiting(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        waitForView(withId(R.id.primary_tasks_surface_view), ViewUtils.VIEW_INVISIBLE);

        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        waitForView(withId(R.id.search_box_text));
        waitForView(withId(R.id.toolbar_buttons));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        Assert.assertFalse(urlBar.isFocused());
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurface_BackButtonDeleteBlankTab() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(1));

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(cta.findViewById(R.id.toolbar_buttons).getVisibility(), View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                cta.getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        pressBack();
        waitForView(withId(R.id.primary_tasks_surface_view));
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single/home_button_on_grid_tab_switcher/true"})
    public void testHomeButtonOnTabSwitcher() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        StartSurfaceTestUtils.clickMoreTabs(cta);
        waitForView(withId(R.id.secondary_tasks_surface_view));
        onView(withId(org.chromium.chrome.tab_ui.R.id.home_button_on_tab_switcher))
                .check(matches(isDisplayed()));
        HomeButton homeButton =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.home_button_on_tab_switcher);
        Assert.assertFalse(homeButton.isLongClickable());
        onView(withId(R.id.home_button_on_tab_switcher)).perform(click());

        onView(withId(R.id.primary_tasks_surface_view)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single/exclude_mv_tiles/false"
            + "/new_home_surface_from_home_button/hide_mv_tiles_and_tab_switcher"})
    public void testNewSurfaceFromHomeButton(){
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForOverviewVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);

            onViewWaiting(
                    allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout), isDisplayed()));
            onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container));
            onViewWaiting(withId(R.id.start_tab_switcher_button));

            // Launch a tab. The home button should show on the normal tab.
            StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        }

        // Go back to the home surface, MV tiles and carousel tab switcher should not show anymore.
        StartSurfaceTestUtils.pressHomePageButton(cta);

        // MV tiles and carousel tab switcher should not show anymore.
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
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
    public void testNewSurfaceHideTabOnlyFromHomeButton() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForOverviewVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);

            onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout));
            onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container));
            onViewWaiting(withId(R.id.start_tab_switcher_button));

            // Launch a tab. The home button should show on the normal tab.
            StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
            onViewWaiting(withId(R.id.home_button)).check(matches(isDisplayed()));
        }

        // Go back to the home surface, MV tiles and carousel tab switcher should not show anymore.
        StartSurfaceTestUtils.pressHomePageButton(cta);

        // MV tiles should shown and carousel tab switcher should not show anymore.
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        onViewWaiting(withId(R.id.start_tab_switcher_button));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout))
                .check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_tabs_in_mru_order/true"})
    public void testShow_SingleAsHomepage_ShowTabsInMRUOrder() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager() != null && cta.getLayoutManager().overviewVisible());
        StartSurfaceTestUtils.waitForTabModel(cta);
        onViewWaiting(withId(R.id.logo));
        Tab tab1 = cta.getCurrentTabModel().getTabAt(0);

        // Launches the first site in MV tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Tab tab2 = cta.getActivityTab();
        // Verifies that the titles of the two Tabs are different.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertNotEquals(tab1.getTitle(), tab2.getTitle()); });

        // Returns to the Start surface.
        OverviewModeBehaviorWatcher overviewModeWatcher =
                new OverviewModeBehaviorWatcher(cta.getLayoutManager(), true, false);
        StartSurfaceTestUtils.pressHomePageButton(cta);
        overviewModeWatcher.waitForBehavior();
        waitForView(allOf(
                withParent(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)));

        RecyclerView recyclerView = cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_list_view);
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
    @CommandLineFlags.Add({BASE_PARAMS + "/single/show_tabs_in_mru_order/true"})
    public void testShow_TabSwitcher_ShowTabsInMRUOrder() {
        tabSwitcher_ShowTabsInMRUOrderImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS +
        "/single/show_tabs_in_mru_order/true/show_last_active_tab_only/true"})
    public void testShowV2_TabSwitcher_ShowTabsInMRUOrder() {
        // clang-format on
        tabSwitcher_ShowTabsInMRUOrderImpl();
    }

    private void tabSwitcher_ShowTabsInMRUOrderImpl() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager() != null && cta.getLayoutManager().overviewVisible());
        StartSurfaceTestUtils.waitForTabModel(cta);
        onViewWaiting(withId(R.id.logo));
        Tab tab1 = cta.getCurrentTabModel().getTabAt(0);

        // Launches the first site in MV tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
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

        ViewGroup secondaryTaskSurface = cta.findViewById(R.id.secondary_tasks_surface_view);
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

    /* MV tiles context menu tests starts. */
    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testDismissTileWithContextMenuAndUndo() throws Exception {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);

        SiteSuggestion siteToDismiss = mMostVisitedSites.getCurrentSites().get(1);
        final View tileView = getTileViewFor(siteToDismiss);

        // Dismiss the tile using the context menu.
        invokeContextMenu(tileView, ContextMenuManager.ContextMenuItemId.REMOVE);
        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(siteToDismiss.url));

        // Ensure that the removal is reflected in the ui.
        Assert.assertEquals(8, getMvTilesLayout().getChildCount());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mMostVisitedSites.setTileSuggestions(getNewSitesAfterDismiss(siteToDismiss)));
        waitForTileRemoved(siteToDismiss);
        Assert.assertEquals(7, getMvTilesLayout().getChildCount());

        // Undo the dismiss through snack bar.
        final View snackbarButton = waitForSnackbar();
        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(siteToDismiss.url));
        TestThreadUtils.runOnUiThreadBlocking((Runnable) snackbarButton::callOnClick);
        Assert.assertFalse(mMostVisitedSites.isUrlBlocklisted(siteToDismiss.url));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testOpenTileInNewTabWithContextMenu() throws ExecutionException {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);

        SiteSuggestion siteToOpen = mMostVisitedSites.getCurrentSites().get(1);
        final View tileView = getTileViewFor(siteToOpen);

        // Open the tile using the context menu.
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        invokeContextMenu(tileView, ContextMenuManager.ContextMenuItemId.OPEN_IN_NEW_TAB);
        hideWatcher.waitForBehavior();
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        // Verifies a new Tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testOpenTileInIncognitoTabWithContextMenu() throws ExecutionException {
        Assume.assumeFalse("https://crbug.com/1210554", mUseInstantStart && mImmediateReturn);
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);

        SiteSuggestion siteToOpen = mMostVisitedSites.getCurrentSites().get(1);
        final View tileView = getTileViewFor(siteToOpen);

        // Open the tile using the context menu.
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        invokeContextMenu(tileView, ContextMenuManager.ContextMenuItemId.OPEN_IN_INCOGNITO_TAB);
        hideWatcher.waitForBehavior();
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        // Verifies a new Tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 1);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testCleanUpMVTilesAfterHiding() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(startSurfaceCoordinator.isMVTilesCleanedUpForTesting());
        });

        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Assert.assertEquals("The launched tab should have the launch type FROM_START_SURFACE",
                TabLaunchType.FROM_START_SURFACE,
                cta.getActivityTabProvider().get().getLaunchType());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(startSurfaceCoordinator.isMVTilesCleanedUpForTesting());
        });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testMVTilesInitialized() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);

        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(startSurfaceCoordinator.isMVTilesInitializedForTesting());
        });

        TabUiTestHelper.enterTabSwitcher(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(startSurfaceCoordinator.isMVTilesInitializedForTesting());
        });
    }

    private MvTilesLayout getMvTilesLayout() {
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout));
        MvTilesLayout mvTilesLayout = mActivityTestRule.getActivity().findViewById(
                org.chromium.chrome.tab_ui.R.id.mv_tiles_layout);
        Assert.assertNotNull("Unable to retrieve the MvTilesLayout.", mvTilesLayout);
        return mvTilesLayout;
    }

    private View getTileViewFor(SiteSuggestion suggestion) {
        View tileView = getMvTilesLayout().getTileViewForTesting(suggestion);
        Assert.assertNotNull("Tile not found for suggestion " + suggestion.url, tileView);

        return tileView;
    }

    private List<SiteSuggestion> getNewSitesAfterDismiss(SiteSuggestion siteToDismiss) {
        List<SiteSuggestion> newSites = new ArrayList<>();
        for (SiteSuggestion site : mMostVisitedSites.getCurrentSites()) {
            if (!site.url.equals(siteToDismiss.url)) {
                newSites.add(site);
            }
        }
        return newSites;
    }

    private void invokeContextMenu(View view, int contextMenuItemId) throws ExecutionException {
        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), contextMenuItemId, 0));
    }

    private void waitForTileRemoved(final SiteSuggestion suggestion) throws TimeoutException {
        MvTilesLayout mvTilesLayout = getMvTilesLayout();
        final SuggestionsTileView removedTile = mvTilesLayout.getTileViewForTesting(suggestion);
        if (removedTile == null) return;

        final CallbackHelper callback = new CallbackHelper();
        mvTilesLayout.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
            @Override
            public void onChildViewAdded(View parent, View child) {}

            @Override
            public void onChildViewRemoved(View parent, View child) {
                if (child == removedTile) callback.notifyCalled();
            }
        });
        callback.waitForCallback("The expected tile was not removed.", 0);
        mvTilesLayout.setOnHierarchyChangeListener(null);
    }

    /** Wait for the snackbar associated to a tile dismissal to be shown and returns its button. */
    private View waitForSnackbar() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        final String expectedSnackbarMessage =
                cta.getResources().getString(R.string.most_visited_item_removed);
        CriteriaHelper.pollUiThread(() -> {
            SnackbarManager snackbarManager = cta.getSnackbarManager();
            Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(true));
            TextView snackbarMessage = cta.findViewById(R.id.snackbar_message);
            Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
            Criteria.checkThat(
                    snackbarMessage.getText().toString(), Matchers.is(expectedSnackbarMessage));
        });

        return cta.findViewById(R.id.snackbar_button);
    }
    /* MV tiles context menu tests ends. */

    /**
     * @return Whether both features {@link ChromeFeatureList.InstantStart} and
     * {@link ChromeFeatureList.TAB_SWITCHER_ON_RETURN} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }

    private void pressHome() {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.pressHome();
        ChromeApplicationTestUtils.waitUntilChromeInBackground();
    }

    private void pressBack() {
        // ChromeTabbedActivity expects the native libraries to be loaded when back is pressed.
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
    }

    private List<Tab> getTabsInCurrentTabModel(TabModel currentTabModel) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            tabs.add(currentTabModel.getTabAt(i));
        }
        return tabs;
    }

    private boolean isTabGridDialogShown(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(org.chromium.chrome.tab_ui.R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.VISIBLE && dialogView.getAlpha() == 1f;
    }

    private boolean isTabGridDialogHidden(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(org.chromium.chrome.tab_ui.R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.GONE;
    }
}

// TODO(crbug.com/1033909): Add more integration tests.
