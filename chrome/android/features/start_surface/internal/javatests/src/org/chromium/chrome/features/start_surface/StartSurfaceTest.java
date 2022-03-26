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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceMediator.FEED_VISIBILITY_CONSISTENCY;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_BASE_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForStableView;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.widget.TextView;

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

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.SingleTabSwitcherMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.start_surface.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.RenderProcessHostUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests of the {@link StartSurface} for cases with tabs. See {@link
 * StartSurfaceNoTabsTest} for test that have no tabs. See {@link StartSurfaceTabSwitcherTest},
 * {@link StartSurfaceMVTilesTest}, {@link StartSurfaceBackButtonTest}, {@link
 * StartSurfaceFinaleTest} for more tests.
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
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;

    private static final long MAX_TIMEOUT_MS = 40000L;
    private static final long MILLISECONDS_PER_MINUTE = TimeUtils.SECONDS_PER_MINUTE * 1000;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    /**
     * Whether feature {@link ChromeFeatureList#INSTANT_START} is enabled.
     */
    private final boolean mUseInstantStart;

    /**
     * Whether feature {@link ChromeFeatureList#TAB_SWITCHER_ON_RETURN} is enabled as "immediately".
     * When immediate return is enabled, the Start surface is showing when Chrome is launched.
     */
    private final boolean mImmediateReturn;

    private CallbackHelper mLayoutChangedCallbackHelper;
    private LayoutStateProvider.LayoutStateObserver mLayoutObserver;
    @LayoutType
    private int mCurrentlyActiveLayout;

    public StartSurfaceTest(boolean useInstantStart, boolean immediateReturn) {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.INSTANT_START, useInstantStart);

        mUseInstantStart = useInstantStart;
        mImmediateReturn = immediateReturn;
    }

    @Before
    public void setUp() throws IOException {
        StartSurfaceTestUtils.setUpStartSurfaceTests(mImmediateReturn, mActivityTestRule);

        mLayoutChangedCallbackHelper = new CallbackHelper();

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManagerSupplier().addObserver((manager) -> {
                if (manager.getActiveLayout() != null) {
                    mCurrentlyActiveLayout = manager.getActiveLayout().getLayoutType();
                    mLayoutChangedCallbackHelper.notifyCalled();
                }
                manager.addObserver(mLayoutObserver);
            });
        });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS +
        "/home_button_on_grid_tab_switcher/false"})
    @DisabledTest(message = "https://crbug.com/1291957")
    public void testShow_SingleAsHomepage() {
        // clang-format on
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

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));

        StartSurfaceTestUtils.clickFirstTabInCarousel();
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.O, message = "https://crbug.com/1291957")
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

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
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

        StartSurfaceTestUtils.clickFirstTabInCarousel();
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS +
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

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        if (isInstantReturn()
                && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                        && Build.VERSION.SDK_INT < Build.VERSION_CODES.O)) {
            // TODO(crbug.com/1092642): Fix androidx.test.espresso.PerformException issue when
            // performing a single click on position: 0. See code below.
            return;
        }

        StartSurfaceTestUtils.clickFirstTabInCarousel();
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/exclude_mv_tiles/true" +
        "/hide_switch_when_no_incognito_tabs/true/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepage_SingleTabNoMVTiles() {
        // clang-format on
        Assume.assumeFalse("https://crbug.com/1205642, https://crbug.com/1214303",
                !mUseInstantStart && mImmediateReturn && VERSION.SDK_INT == VERSION_CODES.M);
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
                .check(matches(withEffectiveVisibility(GONE)));
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
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.single_tab_view)).perform(click());
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    @DisableIf.
        Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "Flaky, see crbug.com/1246457")
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
        StartSurfaceTestUtils.pressHome();

        // Simulates pressing Chrome's icon and launching Chrome from warm start.
        mActivityTestRule.resumeMainActivityFromLauncher();

        StartSurfaceTestUtils.waitForTabModel(cta);
        assertTrue(cta.getTabModelSelector().getCurrentModel().isIncognito());
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForOverviewVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
            onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        } else {
            int container_id = ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_NTP_REVAMP)
                    ? R.id.revamped_incognito_ntp_container
                    : R.id.new_tab_incognito_container;
            onViewWaiting(withId(container_id)).check(matches(isDisplayed()));
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/false"})
    @DisabledTest(message = "crbug.com/1170673 - NoInstant_NoReturn version is flaky")
    public void testSearchInSingleSurface() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(1));

        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        onViewWaiting(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
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
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
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

        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(1));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/open_ntp_instead_of_start/true"})
    @FlakyTest(message = "https://crbug.com/1201548")
    public void testCreateNewTab_OpenNTPInsteadOfStart() {
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
    @DisabledTest(message = "https://crbug.com/1119322")
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/open_ntp_instead_of_start/true"})
    public void testHomeButton_OpenNTPInsteadOfStart() {
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
    @CommandLineFlags.Add({
            START_SURFACE_TEST_BASE_PARAMS + "/show_last_active_tab_only/true",
            // Disable feed placeholder animation because it causes waitForDeferredStartup() to time
            // out.
            FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH})
    public void startSurfaceRecordHistogramsTest_SingleTab() {
        // clang-format on
        startSurfaceRecordHistogramsTest(true);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
        ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS,
        // Disable feed placeholder animation because it causes waitForDeferredStartup() to time
        // out.
        FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH})
    public void startSurfaceRecordHistogramsTest_CarouselTab() {
        // clang-format on
        startSurfaceRecordHistogramsTest(false);
    }

    private void startSurfaceRecordHistogramsTest(boolean isSingleTabSwitcher) {
        if (!mImmediateReturn) {
            assertNotEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        } else {
            assertEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        }

        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertEquals(isSingleTabSwitcher,
                StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue());
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        StartSurfaceTestUtils.waitForDeferredStartup(mActivityTestRule);

        boolean isInstantStart =
                TabUiFeatureUtilities.supportInstantStart(false, mActivityTestRule.getActivity());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        StartSurfaceConfiguration.getHistogramName(
                                AsyncInitializationActivity.FIRST_DRAW_COMPLETED_TIME_MS_UMA,
                                isInstantStart)));
        int expectedRecordCount = mImmediateReturn ? 1 : 0;
        // Histograms should be only recorded when StartSurface is shown immediately after
        // launch.
        if (isSingleTabSwitcher) {
            Assert.assertEquals(expectedRecordCount,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            StartSurfaceConfiguration.getHistogramName(
                                    SingleTabSwitcherMediator.SINGLE_TAB_TITLE_AVAILABLE_TIME_UMA,
                                    isInstantStart)));
        }

        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil
                                .LAST_VISITED_TAB_IS_SRP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA));

        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        StartSurfaceConfiguration.getHistogramName(
                                ExploreSurfaceCoordinator.FEED_CONTENT_FIRST_LOADED_TIME_MS_UMA,
                                isInstantStart)));

        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        StartSurfaceConfiguration.getHistogramName(
                                ExploreSurfaceCoordinator.FEED_STREAM_CREATED_TIME_MS_UMA,
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
        int showAtStartup = mImmediateReturn ? 1 : 0;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        StartSurfaceCoordinator.START_SHOWN_AT_STARTUP_UMA, showAtStartup));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
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
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testShow_SingleAsHomepage_BottomSheet() {
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

        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        StartSurfaceTestUtils.clickMoreTabs(cta);
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        assertTrue(bottomSheetTestSupport.hasSuppressionTokens());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testShow_SingleAsHomepage_ResetScrollPosition() {
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
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void singleAsHomepage_PressHomeButtonWillKeepTab() {
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

        Tab tab = cta.getActivityTab();
        StartSurfaceTestUtils.pressHomePageButton(cta);

        waitForView(withId(R.id.primary_tasks_surface_view));
        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager().overviewVisible());
        Assert.assertEquals(TabLaunchType.FROM_START_SURFACE, tab.getLaunchType());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(StartSurfaceUserData.getKeepTab(tab)); });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS +
        "/check_sync_before_show_start_at_startup/true"})
    public void testShowStartWithSyncCheck() throws Exception {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        assertEquals(1, cta.getTabModelSelector().getTotalTabCount());
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        Assert.assertFalse(cta.getLayoutManager().overviewVisible());

        Assert.assertFalse(ReturnToChromeExperimentsUtil.isPrimaryAccountSync());
        Assert.assertFalse(ReturnToChromeExperimentsUtil.shouldShowOverviewPageOnStart(cta,
                cta.getIntent(), cta.getTabModelSelector(), cta.getInactivityTrackerForTesting()));
        ReturnToChromeExperimentsUtil.setSyncForTesting(true);
        Assert.assertTrue(ReturnToChromeExperimentsUtil.isPrimaryAccountSync());
        Assert.assertEquals(mImmediateReturn,
                ReturnToChromeExperimentsUtil.shouldShowOverviewPageOnStart(cta, cta.getIntent(),
                        cta.getTabModelSelector(), cta.getInactivityTrackerForTesting()));
        ReturnToChromeExperimentsUtil.setSyncForTesting(false);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1, supported_abis_includes = "x86",
            message = "Flaky, see crbug.com/1258154")
    public void testNotShowIncognitoHomepage() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getTabModelSelector().getModel(false).closeAllTabs(); });
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertTrue(cta.getLayoutManager().overviewVisible());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> cta.getTabCreator(true /*incognito*/).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 1);

        // Simulates pressing the home button. Incognito tab should stay and homepage shouldn't
        // show.
        onView(withId(R.id.home_button)).perform(click());
        assertFalse(cta.getLayoutManager().overviewVisible());
        int container_id = ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_NTP_REVAMP)
                ? R.id.revamped_incognito_ntp_container
                : R.id.new_tab_incognito_container;
        onViewWaiting(withId(container_id)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/behavioural_targeting/model_mv_tiles"
        + "/user_clicks_threshold/1/num_days_user_click_below_threshold/2"})
    public void testStartWithBehaviouralTargeting() throws Exception {
        // clang-format on
        Assume.assumeTrue(mImmediateReturn);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        Assert.assertFalse(cta.getLayoutManager().overviewVisible());

        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        // Verifies that the START_NEXT_SHOW_ON_STARTUP_DECISION_MS has been set.
        long nextDecisionTime =
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeExperimentsUtil.INVALID_DECISION_TIMESTAMP);
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        Assert.assertEquals(0, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));

        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeExperimentsUtil.ShowChromeStartSegmentationResult.DONT_SHOW);

        StartSurfaceConfiguration.USER_CLICK_THRESHOLD.setForTesting(1);
        int clicksHigherThreshold = StartSurfaceConfiguration.USER_CLICK_THRESHOLD.getValue();
        Assert.assertEquals(1, clicksHigherThreshold);
        ReturnToChromeExperimentsUtil.onMVTileOpened();
        // Verifies that userBehaviourSupported() returns the same result before the next decision
        // time arrives.
        Assert.assertFalse(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertEquals(nextDecisionTime,
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeExperimentsUtil.INVALID_DECISION_TIMESTAMP));
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        Assert.assertEquals(1, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Startup.Android.ShowChromeStartSegmentationResultComparison"));

        // Verifies if the next decision time past and the clicks of MV tiles is higher than the
        // threshold, userBehaviourSupported() returns true. Besides, the next decision time is set
        // to NUM_DAYS_KEEP_SHOW_START_AT_STARTUP day's later, and MV tiles count is reset.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertTrue(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertTrue(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_KEEP_SHOW_START_AT_STARTUP.getValue());
        Assert.assertEquals(0, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Startup.Android.ShowChromeStartSegmentationResultComparison",
                        ReturnToChromeExperimentsUtil.ShowChromeStartSegmentationResultComparison
                                .SEGMENTATION_DISABLED_LOGIC_ENABLED));

        // Verifies if the next decision time past and the clicks of MV tiles is lower than the
        // threshold, userBehaviourSupported() returns false. Besides, the next decision time is
        // set to NUM_DAYS_USER_CLICK_BELOW_THRESHOLD day's later.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeExperimentsUtil.ShowChromeStartSegmentationResult.SHOW);
        Assert.assertEquals(0, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));
        Assert.assertFalse(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Startup.Android.ShowChromeStartSegmentationResultComparison",
                        ReturnToChromeExperimentsUtil.ShowChromeStartSegmentationResultComparison
                                .SEGMENTATION_ENABLED_LOGIC_DISABLED));

        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("feeds");
        verifyBehaviourTypeRecordedAndChecked(manager);

        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("open_new_tab");
        verifyBehaviourTypeRecordedAndChecked(manager);

        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("open_history");
        verifyBehaviourTypeRecordedAndChecked(manager);

        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("open_recent_tabs");
        verifyBehaviourTypeRecordedAndChecked(manager);

        // Verifies if the key doesn't match the value of
        // StartSurfaceConfiguration.BEHAVIOURAL_TARGETING, e.g., the value isn't set, onUIClicked()
        // doesn't record or increase the count.
        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("");
        String type = "feeds";
        String key = ReturnToChromeExperimentsUtil.getBehaviourTypeKeyForTesting(type);
        ReturnToChromeExperimentsUtil.onUIClicked(key);
        Assert.assertEquals(0, manager.readInt(key, 0));

        // Verifies the combination case that BEHAVIOURAL_TARGETING is set to "all".
        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("all");
        String type1 = "open_history";
        String type2 = "open_recent_tabs";
        String key1 = ReturnToChromeExperimentsUtil.getBehaviourTypeKeyForTesting(type1);
        String key2 = ReturnToChromeExperimentsUtil.getBehaviourTypeKeyForTesting(type2);
        Assert.assertEquals(0, manager.readInt(key1, 0));
        Assert.assertEquals(0, manager.readInt(key2, 0));
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Increase the count of one key.
        ReturnToChromeExperimentsUtil.onHistoryOpened();
        Assert.assertEquals(1, manager.readInt(key1, 0));

        // Verifies that userBehaviourSupported() return true due to the count of this key is higher
        // or equal to the threshold.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertTrue(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertEquals(0, manager.readInt(key1, 0));
        Assert.assertEquals(0, manager.readInt(key2, 0));
        Assert.assertTrue(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Resets the decision.
        manager.writeBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/behavioural_targeting/model"})
    public void testStartSegmentationUsage() throws Exception {
        Assume.assumeTrue(mImmediateReturn);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        Assert.assertFalse(cta.getLayoutManager().overviewVisible());

        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        // Verifies that the START_NEXT_SHOW_ON_STARTUP_DECISION_MS has been set.
        long nextDecisionTime =
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeExperimentsUtil.INVALID_DECISION_TIMESTAMP);
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        Assert.assertEquals(0, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));

        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeExperimentsUtil.ShowChromeStartSegmentationResult.SHOW);

        // Verifies that userBehaviourSupported() returns the same result before the next decision
        // time arrives.
        Assert.assertFalse(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertEquals(nextDecisionTime,
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeExperimentsUtil.INVALID_DECISION_TIMESTAMP));
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Verifies if the next decision time past, userBehaviourSupported() returns true. Besides,
        // the next decision time is set to NUM_DAYS_KEEP_SHOW_START_AT_STARTUP day's later.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertTrue(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertTrue(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_KEEP_SHOW_START_AT_STARTUP.getValue());

        // Verifies if the next decision time past and segmentation says dont show,
        // userBehaviourSupported() returns false. Besides, the next decision time is set to
        // NUM_DAYS_USER_CLICK_BELOW_THRESHOLD day's later.
        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeExperimentsUtil.ShowChromeStartSegmentationResult.DONT_SHOW);
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertFalse(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());

        // Verifies that if segmentation stops returning results, then we continue to use the
        // previous result.
        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeExperimentsUtil.ShowChromeStartSegmentationResult.UNINITIALIZED);
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertFalse(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testStartSurfaceMenu() throws ExecutionException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.icon_row_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.new_tab_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.new_incognito_tab_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.divider_line_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.open_history_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.downloads_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.all_bookmarks_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.recent_tabs_menu_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.divider_line_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.preferences_id));
        assertNotNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.help_id));

        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.menu_group_tabs));
        assertNull(AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.close_all_tabs_menu_id));

        boolean hasUpdateMenuItem =
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.update_menu_id)
                != null;
        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(mActivityTestRule.getAppMenuCoordinator());
        assertEquals(hasUpdateMenuItem ? 11 : 10, menuItemsModelList.size());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void test_DoNotLoadLastSelectedTabOnStartup() {
        // clang-format on
        doTestNotLoadLastSelectedTabOnStartupImpl();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/show_last_active_tab_only/true"})
    public void test_DoNotLoadLastSelectedTabOnStartupV2() {
        // clang-format on
        doTestNotLoadLastSelectedTabOnStartupImpl();
    }

    private void doTestNotLoadLastSelectedTabOnStartupImpl() {
        assumeTrue(mImmediateReturn);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        Assert.assertEquals(0, RenderProcessHostUtils.getCurrentRenderProcessCount());

        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        StartSurfaceTestUtils.waitForCurrentTabLoaded(mActivityTestRule);
        Assert.assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
    }

    /**
     * Check that the next decision time is within |numOfDays| from now.
     * @param numOfDays Number of days to check.
     */
    private void verifyNextDecisionTimeStampInDays(
            SharedPreferencesManager manager, int numOfDays) {
        long approximateTime = System.currentTimeMillis()
                + numOfDays * ReturnToChromeExperimentsUtil.MILLISECONDS_PER_DAY;
        long nextDecisionTime =
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeExperimentsUtil.INVALID_DECISION_TIMESTAMP);

        Assert.assertThat("new decision time lower bound",
                approximateTime - MILLISECONDS_PER_MINUTE,
                Matchers.lessThanOrEqualTo(nextDecisionTime));

        Assert.assertThat("new decision time upper bound",
                approximateTime + MILLISECONDS_PER_MINUTE,
                Matchers.greaterThanOrEqualTo(nextDecisionTime));
    }

    private void verifyBehaviourTypeRecordedAndChecked(SharedPreferencesManager manager) {
        String key = ReturnToChromeExperimentsUtil.getBehaviourTypeKeyForTesting(
                StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue());
        Assert.assertEquals(0, manager.readInt(key, 0));

        // Increase the count of the key.
        ReturnToChromeExperimentsUtil.onUIClicked(key);
        Assert.assertEquals(1, manager.readInt(key, 0));
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Verifies that userBehaviourSupported() return true due to the count of this key is higher
        // or equal to the threshold.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertTrue(ReturnToChromeExperimentsUtil.userBehaviourSupported());
        Assert.assertEquals(0, manager.readInt(key, 0));
        Assert.assertTrue(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Resets the decision.
        manager.writeBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false);
    }

    /**
     * @return Whether both features {@link ChromeFeatureList#INSTANT_START} and
     * {@link ChromeFeatureList#TAB_SWITCHER_ON_RETURN} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }
}
