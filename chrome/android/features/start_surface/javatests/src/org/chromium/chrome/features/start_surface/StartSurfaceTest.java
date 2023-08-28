// Copyright 2019 The Chromium Authors
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
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS;
import static org.chromium.chrome.features.start_surface.StartSurfaceMediator.FEED_VISIBILITY_CONSISTENCY;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_BASE_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_SINGLE_ENABLED_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.getStartSurfaceLayoutType;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForStableView;

import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.google.android.material.appbar.AppBarLayout;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
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
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNTP;
import org.chromium.chrome.features.tasks.SingleTabSwitcherMediator;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.RenderProcessHostUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.List;

/**
 * Integration tests of the {@link StartSurface} for cases with tabs. See {@link
 * StartSurfaceNoTabsTest} for test that have no tabs. See {@link StartSurfaceTabSwitcherTest},
 * {@link StartSurfaceMVTilesTest}, {@link StartSurfaceBackButtonTest} for more tests.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures(
        {ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.EMPTY_STATES})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@DoNotBatch(reason = "This test suite tests startup behaviors.")
public class StartSurfaceTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;

    private static final String HISTOGRAM_START_SURFACE_MODULE_CLICK = "StartSurface.Module.Click";
    private static final String HISTOGRAM_SPARE_TAB_FINAL_STATUS = "Android.SpareTab.FinalStatus";
    private static final String HISTOGRAM_START_SURFACE_SPARE_TAB_SHOW_AND_CREATE =
            "StartSurface.SpareTab.TimeBetweenShowAndCreate";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    /**
     * Whether feature {@link ChromeFeatureList#INSTANT_START} is enabled.
     */
    private final boolean mUseInstantStart;

    /**
     * Whether feature {@link ChromeFeatureList#START_SURFACE_RETURN_TIME} is enabled as
     * "immediately". When immediate return is enabled, the Start surface is showing when Chrome is
     * launched.
     */
    private final boolean mImmediateReturn;

    private CallbackHelper mLayoutChangedCallbackHelper;
    private LayoutStateProvider.LayoutStateObserver mLayoutObserver;
    @LayoutType
    private int mCurrentlyActiveLayout;

    public StartSurfaceTest(boolean useInstantStart, boolean immediateReturn) {
        ChromeFeatureList.sInstantStart.setForTesting(useInstantStart);

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
            mCurrentlyActiveLayout = StartSurfaceTestUtils.getStartSurfaceLayoutType();
        }

        mLayoutObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onFinishedShowing(@LayoutType int layoutType) {
                mCurrentlyActiveLayout = layoutType;
                // Let all observers finish running.
                ThreadUtils.postOnUiThread(mLayoutChangedCallbackHelper::notifyCalled);
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManagerSupplier().addObserver((manager) -> {
                if (manager.getActiveLayout() != null) {
                    mCurrentlyActiveLayout = manager.getActiveLayout().getLayoutType();
                    // Let all observers finish running.
                    ThreadUtils.postOnUiThread(mLayoutChangedCallbackHelper::notifyCalled);
                }
                manager.addObserver(mLayoutObserver);
            });
        });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(withId(R.id.search_box_text)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.mv_tiles_container)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.tab_switcher_title)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.tab_switcher_module_container)).check(matches(isDisplayed()));
        onView(withId(R.id.tasks_surface_body)).check(matches(isDisplayed()));

        StartSurfaceTestUtils.clickMoreTabs(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        ViewUtils.waitForVisibleView(
                allOf(withParent(withId(TabUiTestHelper.getTabSwitcherParentId(cta))),
                        withId(R.id.tab_list_view)));

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));

        StartSurfaceTestUtils.clickFirstTabInCarousel();
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.
    Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS + "/hide_switch_when_no_incognito_tabs/false"})
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testShow_SingleAsHomepage_NoIncognitoSwitch() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(withId(R.id.search_box_text));
        onViewWaiting(withId(R.id.mv_tiles_container)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.tab_switcher_title)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.tab_switcher_module_container)).check(matches(isDisplayed()));
        onView(withId(R.id.tasks_surface_body)).check(matches(isDisplayed()));

        // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
        onView(withId(R.id.incognito_toggle_tabs)).check(matches(withEffectiveVisibility(GONE)));

        StartSurfaceTestUtils.clickMoreTabs(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        onView(withId(R.id.incognito_toggle_tabs)).check(matches(withEffectiveVisibility(VISIBLE)));

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        if (isInstantReturn() && Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // TODO(crbug.com/1139515): Fix incognito_toggle_tabs visibility AssertionFailedError
            // issue.
            // TODO(crbug.com/1092642): Fix androidx.test.espresso.PerformException issue when
            // performing a single click on position: 0. See code below.
            return;
        }

        onView(withId(R.id.incognito_toggle_tabs)).check(matches(withEffectiveVisibility(GONE)));

        StartSurfaceTestUtils.clickFirstTabInCarousel();
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.
    Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS + "/hide_switch_when_no_incognito_tabs/false"})
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testShow_SingleAsHomepage_NoIncognitoSwitch_RefactorEnabled() {
        testShow_SingleAsHomepage_NoIncognitoSwitch();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS
            + "open_ntp_instead_of_start/false/open_start_as_homepage/true"})
    // clang-format off
    public void testShow_SingleAsHomepage_SingleTab() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(withId(R.id.search_box_text));
        onView(withId(R.id.mv_tiles_container)).check(matches(isDisplayed()));
        onView(withId(R.id.tab_switcher_title)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.tab_switcher_module_container)).check(matches(isDisplayed()));
        onView(withId(R.id.single_tab_view)).check(matches(isDisplayed()));
        onView(withId(R.id.tasks_surface_body)).check(matches(isDisplayed()));

        if (!isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to make incognito switch part of the view.
            onView(withId(R.id.incognito_toggle_tabs))
                    .check(matches(withEffectiveVisibility(GONE)));
        }
        onViewWaiting(allOf(withId(R.id.tab_title_view), withText(not(is("")))));

        StartSurfaceTestUtils.clickTabSwitcherButton(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        ModuleTypeOnStartAndNTP.TAB_SWITCHER_BUTTON));

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        onViewWaiting(withId(R.id.single_tab_view)).perform(click());
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS
            + "open_ntp_instead_of_start/false/open_start_as_homepage/true"})
    // clang-format off
    public void testShow_SingleAsHomepage_SingleTab_RefactorEnabled() {
        // clang-format on
        testShow_SingleAsHomepage_SingleTab();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage_FromResumeShowStart() throws Exception {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(()
                                            -> cta.getLayoutManager() != null
                        && cta.getLayoutManager().isLayoutVisible(
                                StartSurfaceTestUtils.getStartSurfaceLayoutType()));
        StartSurfaceTestUtils.waitForTabModel(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getTabModelSelector().getModel(false).closeAllTabs(); });
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertTrue(cta.getLayoutManager().isLayoutVisible(getStartSurfaceLayoutType()));
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
            StartSurfaceTestUtils.waitForTabSwitcherVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
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
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "crbug.com/1170673 - NoInstant_NoReturn version is flaky")
    public void testSearchInSingleSurface() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        CriteriaHelper.pollInstrumentationThread(
                () -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule));
        onViewWaiting(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        ChromeTabUtils.waitForTabPageLoaded(cta.getActivityTab(), (String) null);

        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        Assert.assertFalse(urlBar.isFocused());
        waitForStableView(cta.findViewById(R.id.search_box_text));
        onView(withId(R.id.search_box_text)).perform(click());
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisabledTest(message = "https://crbug.com/1434823")
    @CommandLineFlags.
    Add({START_SURFACE_TEST_BASE_PARAMS + "hide_switch_when_no_incognito_tabs/false"})
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
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        TabUiTestHelper.enterTabSwitcher(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);

        // Click plus button from top toolbar should create NTP instead of showing start surface.
        onViewWaiting(withId(R.id.new_tab_button)).perform(click());
        TabUiTestHelper.verifyTabModelTabCount(cta, 3, 0);
        assertTrue(cta.getLayoutManager().isLayoutVisible(LayoutType.BROWSING));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
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
        assertFalse(cta.getLayoutManager().isLayoutVisible(
                StartSurfaceTestUtils.getStartSurfaceLayoutType()));
    }

    /**
     * Tests that histograms are recorded only if the StartSurface is shown when Chrome is launched
     * from cold start.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})

    @EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS
                    + "open_ntp_instead_of_start/false/open_start_as_homepage/true",
            // Disable feed placeholder animation because it causes waitForDeferredStartup() to time
            // out.
            FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH})
    // clang-format off
    public void startSurfaceRecordHistogramsTest_SingleTab() {
        // clang-format on
        startSurfaceRecordHistogramsTest(true);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS,
        // Disable feed placeholder animation because it causes waitForDeferredStartup() to time
        // out.
        FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH})
    public void startSurfaceRecordHistogramsTest_CarouselTab() {
        // clang-format on
        startSurfaceRecordHistogramsTest(false);
    }

    private void startSurfaceRecordHistogramsTest(boolean isSingleTabSwitcher) {
        if (!mImmediateReturn) {
            assertNotEquals(0, START_SURFACE_RETURN_TIME_SECONDS.getValue());
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        } else {
            assertEquals(0, START_SURFACE_RETURN_TIME_SECONDS.getValue());
        }

        Assert.assertEquals(isSingleTabSwitcher,
                StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue());
        StartSurfaceTestUtils.waitForStartSurfaceVisible(mLayoutChangedCallbackHelper,
                mCurrentlyActiveLayout, mActivityTestRule.getActivity());
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

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    RecordHistogram.getHistogramTotalCountForTesting(
                            StartSurfaceConfiguration.getHistogramName(
                                    ExploreSurfaceCoordinator.FEED_CONTENT_FIRST_LOADED_TIME_MS_UMA,
                                    isInstantStart)),
                    is(expectedRecordCount));
        });

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
                        ReturnToChromeUtil
                                .LAST_VISITED_TAB_IS_SRP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA));

        Assert.assertEquals(expectedRecordCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeUtil
                                .LAST_ACTIVE_TAB_IS_NTP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA));

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

        if (mImmediateReturn) {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            ReturnToChromeUtil.START_SHOW_STATE_UMA,
                            StartSurfaceState.SHOWING_START));
        } else {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            ReturnToChromeUtil.START_SHOW_STATE_UMA,
                            StartSurfaceState.SHOWING_HOMEPAGE));
        }
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ChromeInactivityTracker.UMA_IS_LAST_VISIBLE_TIME_LOGGED));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage_VoiceSearchButtonShown() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        StartSurfaceTestUtils.waitForStartSurfaceVisible(mLayoutChangedCallbackHelper,
                mCurrentlyActiveLayout, mActivityTestRule.getActivity());

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(withId(R.id.search_box_text)).check(matches(isDisplayed()));
        onView(withId(R.id.voice_search_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1471244")
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage_BottomSheet_WithBottomSheetGtsSupport() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        BottomSheetTestSupport bottomSheetTestSupport = new BottomSheetTestSupport(
                cta.getRootUiCoordinatorForTesting().getBottomSheetController());
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
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
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        /** Verifies the case of navigating to a tab -> start surface -> tab switcher. */
        StartSurfaceTestUtils.clickFirstTabInCarousel();
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());

        StartSurfaceTestUtils.clickTabSwitcherButton(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        assertFalse(bottomSheetTestSupport.hasSuppressionTokens());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "https://crbug.com/1470714")
    public void testShow_SingleAsHomepage_ResetScrollPosition() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Scroll the toolbar.
        StartSurfaceTestUtils.scrollToolbar(cta);
        AppBarLayout taskSurfaceHeader = cta.findViewById(R.id.task_surface_header);
        assertNotEquals(taskSurfaceHeader.getBottom(), taskSurfaceHeader.getHeight());

        // Verifies the case of scrolling Start surface ->  tab switcher -> tap "+1" button ->
        // Start surface. The Start surface should reset its scroll position.
        StartSurfaceTestUtils.clickTabSwitcherButton(cta);

        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        onViewWaiting(withId(R.id.primary_tasks_surface_view));

        // The Start surface should reset its scroll position.
        CriteriaHelper.pollInstrumentationThread(
                () -> taskSurfaceHeader.getBottom() == taskSurfaceHeader.getHeight());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "https://crbug.com/1470714")
    public void testShow_SingleAsHomepage_DoNotResetScrollPositionFromBack() {
        assumeTrue(mImmediateReturn);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Scroll the toolbar.
        StartSurfaceTestUtils.scrollToolbar(cta);
        AppBarLayout taskSurfaceHeader = cta.findViewById(R.id.task_surface_header);
        assertNotEquals(taskSurfaceHeader.getBottom(), taskSurfaceHeader.getHeight());

        // Verifies the case of scrolling Start surface ->  MV tile -> tapping back ->
        // Start surface. The Start surface should not reset its scroll position.
        StartSurfaceTestUtils.launchFirstMVTile(cta, 1);
        Assert.assertEquals("The launched tab should have the launch type FROM_START_SURFACE",
                TabLaunchType.FROM_START_SURFACE,
                cta.getActivityTabProvider().get().getLaunchType());
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        // Back gesture on the tab should take us back to the start surface homepage.
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        // The Start surface should not reset its scroll position.
        CriteriaHelper.pollInstrumentationThread(
                () -> taskSurfaceHeader.getBottom() != taskSurfaceHeader.getHeight());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void singleAsHomepage_PressHomeButtonWillKeepTab() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        onViewWaiting(allOf(withId(R.id.mv_tiles_container), isDisplayed()));

        // Launches the first site in mv tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);

        if (isInstantReturn() && Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // Fix the issue that failed to perform a single click on the tab switcher button.
            // See code below.
            return;
        }

        Tab tab = cta.getActivityTab();
        StartSurfaceTestUtils.pressHomePageButton(cta);

        ViewUtils.waitForVisibleView(withId(R.id.primary_tasks_surface_view));
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        Assert.assertEquals(TabLaunchType.FROM_START_SURFACE, tab.getLaunchType());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(StartSurfaceUserData.getKeepTab(tab)); });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1, supported_abis_includes = "x86",
            message = "Flaky, see crbug.com/1258154")
    public void testNotShowIncognitoHomepage() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getTabModelSelector().getModel(false).closeAllTabs(); });
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertTrue(cta.getLayoutManager().isLayoutVisible(
                StartSurfaceTestUtils.getStartSurfaceLayoutType()));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> cta.getTabCreator(true /*incognito*/).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 1);

        // Simulates pressing the home button. Incognito tab should stay and homepage shouldn't
        // show.
        onView(withId(R.id.home_button)).perform(click());
        int container_id = ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_NTP_REVAMP)
                ? R.id.revamped_incognito_ntp_container
                : R.id.new_tab_incognito_container;
        onViewWaiting(withId(container_id)).check(matches(isDisplayed()));
        assertFalse(cta.getLayoutManager().isLayoutVisible(
                StartSurfaceTestUtils.getStartSurfaceLayoutType()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void test_DoNotLoadLastSelectedTabOnStartup() {
        // clang-format on
        doTestNotLoadLastSelectedTabOnStartupImpl();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    public void test_DoNotLoadLastSelectedTabOnStartupV2() {
        // clang-format on
        doTestNotLoadLastSelectedTabOnStartupImpl();
    }

    /**
     * Test whether the clicking action on the profile button in {@link StartSurface} is been
     * recorded in histogram correctly.
     */
    @Test
    @SmallTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY)
    public void testRecordHistogramProfileButtonClick_StartSurface() {
        Assume.assumeTrue(mImmediateReturn);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.PROFILE_BUTTON);
        onViewWaiting(withId(R.id.identity_disc_button)).perform(click());
        histogramWatcher.assertExpected(HISTOGRAM_START_SURFACE_MODULE_CLICK
                + " is not recorded correctly when click on the profile button.");
    }

    /**
     * Test whether the clicking action on the menu button in {@link StartSurface} is been
     * recorded in histogram correctly.
     */
    @Test
    @SmallTest
    @Feature({"StartSurface"})
    public void testRecordHistogramMenuButtonClick_StartSurface() {
        Assume.assumeTrue(mImmediateReturn);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MENU_BUTTON);
        onView(allOf(withId(R.id.menu_button_wrapper), withParent(withId(R.id.menu_anchor))))
                .perform(click());
        histogramWatcher.assertExpected(HISTOGRAM_START_SURFACE_MODULE_CLICK
                + " is not recorded correctly when click on the menu button.");
    }

    private void doTestNotLoadLastSelectedTabOnStartupImpl() {
        assumeTrue(mImmediateReturn);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        Assert.assertEquals(0, RenderProcessHostUtils.getCurrentRenderProcessCount());

        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        StartSurfaceTestUtils.waitForCurrentTabLoaded(mActivityTestRule);
        Assert.assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
    }

    /**
     * @return Whether both features {@link ChromeFeatureList#INSTANT_START} and
     * {@link ChromeFeatureList#START_SURFACE_RETURN_TIME} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }

    /**
     * Tests that on navigation from start surface using MV tiles should use spare tab.
     */
    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB, ChromeFeatureList.START_SURFACE_SPARE_TAB})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void test_UsesSpareTabForNavigationFromMVTiles() {
        if (!mImmediateReturn) return;

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_START_SURFACE_SPARE_TAB_SHOW_AND_CREATE);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // SpareTab should be created when the start surface is shown.
        CriteriaHelper.pollUiThread(() -> { WarmupManager.getInstance().hasSpareTab(); });

        // The renderer process count should be 0 as spareTab doesn't initialize renderer by
        // default.
        Assert.assertEquals(0, RenderProcessHostUtils.getCurrentRenderProcessCount());

        StartSurfaceTestUtils.launchFirstMVTile(cta, 1);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        StartSurfaceTestUtils.waitForCurrentTabLoaded(mActivityTestRule);

        // SpareTab should be used when we navigate from start surface.
        CriteriaHelper.pollUiThread(() -> !WarmupManager.getInstance().hasSpareTab());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(HISTOGRAM_SPARE_TAB_FINAL_STATUS,
                        WarmupManager.SpareTabFinalStatus.TAB_USED));
        histogramWatcher.assertExpected();
    }

    /**
     * Tests that on navigation from start surface using search box should use spare tab.
     */
    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB, ChromeFeatureList.START_SURFACE_SPARE_TAB})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "https://crbug.com/1470714")
    public void test_UsesSpareTabForNavigationFromSearchBox() {
        if (!mImmediateReturn) return;

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // SpareTab should be created when the start surface is shown.
        CriteriaHelper.pollUiThread(() -> { WarmupManager.getInstance().hasSpareTab(); });

        // Navigate from StartSurface using search box.
        onViewWaiting(withId(R.id.search_box_text)).perform(replaceText("about:blank"));
        CriteriaHelper.pollInstrumentationThread(
                () -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule));
        onViewWaiting(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        ChromeTabUtils.waitForTabPageLoaded(cta.getActivityTab(), (String) null);

        // SpareTab should be used when we navigate from start surface.
        CriteriaHelper.pollUiThread(() -> !WarmupManager.getInstance().hasSpareTab());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(HISTOGRAM_SPARE_TAB_FINAL_STATUS,
                        WarmupManager.SpareTabFinalStatus.TAB_USED));
    }

    /**
     * Tests that on navigation from start surface using carousel tab switcher shouldn't use spare
     * tab.
     */
    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB, ChromeFeatureList.START_SURFACE_SPARE_TAB})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void test_DoesntUseSpareTabForNavigationFromCarouselTabSwitcher() {
        if (!mImmediateReturn) return;

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // SpareTab should be created when the start surface is shown.
        CriteriaHelper.pollUiThread(() -> { WarmupManager.getInstance().hasSpareTab(); });

        // Navigate from StartSurface using carousel tab switcher.
        StartSurfaceTestUtils.clickFirstTabInCarousel();
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);

        // This shouldn't use spare tab and deletes spare tab once the start surface gets hidden.
        CriteriaHelper.pollUiThread(() -> !WarmupManager.getInstance().hasSpareTab());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(HISTOGRAM_SPARE_TAB_FINAL_STATUS,
                        WarmupManager.SpareTabFinalStatus.TAB_DESTROYED));
    }

    /**
     * Tests that we create a new renderer process during spare tab creation when
     * initialize_renderer param enabled.
     */
    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB, ChromeFeatureList.START_SURFACE_SPARE_TAB})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void test_SpareTabCreatesNewRendererProcessWithParamEnabled() {
        if (!mImmediateReturn) return;

        // Set the param to true allowing renderer initialization.
        WarmupManager.SPARE_TAB_INITIALIZE_RENDERER.setForTesting(true);

        // Show Start Surface.
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // SpareTab should be created when the start surface is shown.
        CriteriaHelper.pollUiThread(() -> { WarmupManager.getInstance().hasSpareTab(); });

        // The renderer process count should be 1 as spareTab also initializes renderer when the
        // flag is set
        CriteriaHelper.pollUiThread(
                () -> RenderProcessHostUtils.getCurrentRenderProcessCount() == 1);
    }

    /**
     * Tests that spare tab is not used when we navigate from different surface other than start
     * surface, using new tab button or loading a new URL directly.
     */
    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB, ChromeFeatureList.START_SURFACE_SPARE_TAB})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void test_SpareTabNotUsedOnOtherSurface() {
        if (!mImmediateReturn) return;

        // Show Start Surface.
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // SpareTab should be created when the start surface is shown.
        CriteriaHelper.pollUiThread(() -> { WarmupManager.getInstance().hasSpareTab(); });

        // Navigate from start surface using link
        mActivityTestRule.loadUrlInNewTab("about:blank", false, TabLaunchType.FROM_LINK);

        // This shouldn't use spare tab and deletes spare tab once the start surface gets hidden.
        CriteriaHelper.pollUiThread(() -> !WarmupManager.getInstance().hasSpareTab());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(HISTOGRAM_SPARE_TAB_FINAL_STATUS,
                        WarmupManager.SpareTabFinalStatus.TAB_DESTROYED));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SURFACE_POLISH + "<Study"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testStartSurfaceBackgroundColorAfterPolish() {
        if (!mImmediateReturn || mUseInstantStart) return;
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onViewWaiting(withId(R.id.tab_switcher_toolbar));
        onViewWaiting(withId(R.id.search_box_text)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.mv_tiles_container)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.tab_switcher_title)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.tab_switcher_module_container)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.tasks_surface_body));

        View startSurfaceView =
                cta.findViewById(org.chromium.chrome.test.R.id.primary_tasks_surface_view);
        assertEquals("The background color for start surface is wrong.",
                ChromeColors.getSurfaceColor(cta,
                        org.chromium.chrome.test.R.dimen.home_surface_background_color_elevation),
                ((ColorDrawable) startSurfaceView.getBackground()).getColor());
        View startSurfaceViewHeader =
                cta.findViewById(org.chromium.chrome.test.R.id.task_surface_header);
        assertEquals("The background color for start surface is wrong.",
                ChromeColors.getSurfaceColor(cta,
                        org.chromium.chrome.test.R.dimen.home_surface_background_color_elevation),
                ((ColorDrawable) startSurfaceViewHeader.getBackground()).getColor());
        View startSurfaceToolbar =
                cta.findViewById(org.chromium.chrome.test.R.id.tab_switcher_toolbar);
        assertEquals("The background color for start surface toolbar is wrong.",
                ChromeColors.getSurfaceColor(cta,
                        org.chromium.chrome.test.R.dimen.home_surface_background_color_elevation),
                ((ColorDrawable) startSurfaceToolbar.getBackground()).getColor());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisableFeatures({ChromeFeatureList.SURFACE_POLISH, ChromeFeatureList.START_SURFACE_REFACTOR})
    public void testStatusBarColor_RefactorDisabled_SurfacePolishDisabled() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        final int expectedDefaultStandardColor = ChromeColors.getDefaultThemeColor(cta, false);
        testStatusBarColorImpl(expectedDefaultStandardColor, expectedDefaultStandardColor);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    @EnableFeatures({ChromeFeatureList.SURFACE_POLISH})
    public void testStatusBarColor_RefactorDisabled_SurfacePolishEnabled() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        final int expectedPolishedStandardColor =
                ChromeColors.getSurfaceColor(cta, R.dimen.home_surface_background_color_elevation);
        final int expectedDefaultStandardColor = ChromeColors.getDefaultThemeColor(cta, false);
        testStatusBarColorImpl(expectedPolishedStandardColor, expectedDefaultStandardColor);
    }

    private void testStatusBarColorImpl(
            int expectedStartSurfaceColor, int expectedTabSwitcherColor) {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        StartSurfaceTestUtils.waitForStatusBarColor(cta, expectedStartSurfaceColor);

        StartSurfaceTestUtils.clickTabSwitcherButton(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        StartSurfaceTestUtils.waitForStatusBarColor(cta, expectedTabSwitcherColor);

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        StartSurfaceTestUtils.waitForStatusBarColor(cta, expectedStartSurfaceColor);
    }
}
