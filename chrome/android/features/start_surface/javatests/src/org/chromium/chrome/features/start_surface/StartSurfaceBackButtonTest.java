// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_BASE_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_SINGLE_ENABLED_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.os.Build;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCarouselLayout;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests of the back action when {@link StartSurface} is enabled.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
@DoNotBatch(reason = "StartSurface*Test tests startup behaviours and thus can't be batched.")
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class StartSurfaceBackButtonTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;

    private static final long MAX_TIMEOUT_MS = 40000L;

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
    private FakeMostVisitedSites mMostVisitedSites;

    public StartSurfaceBackButtonTest(boolean useInstantStart, boolean immediateReturn) {
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

        mMostVisitedSites = StartSurfaceTestUtils.setMVTiles(mSuggestionsDeps);
    }

    // Test that back press on start surface should exit app rather than closing tab.
    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage_BackButton_ClosableTab() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        mActivityTestRule.loadUrlInNewTab("about:blank", false, TabLaunchType.FROM_LINK);
        StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        StartSurfaceTestUtils.pressBackAndVerifyChromeToBackground(mActivityTestRule);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage_BackButton_ClosableTab_BackGestureRefactor() {
        testShow_SingleAsHomepage_BackButton_ClosableTab();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage_BackButton() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        // Case 1:
        // Launches the first site in mv tiles, and press back button.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        StartSurfaceTestUtils.pressBack(mActivityTestRule);

        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        // Verifies the new Tab is deleted.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Case 2:
        // Launches the first site in mv tiles, and press home button to return to the Start
        // surface.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        StartSurfaceTestUtils.pressHomePageButton(cta);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        onView(allOf(withId(R.id.tab_list_view), isDisplayed()));

        // Launches the new tab from the carousel tab switcher, and press back button.
        StartSurfaceTestUtils.clickTabInCarousel(/* position = */ 1);
        Assert.assertEquals(TabLaunchType.FROM_START_SURFACE,
                cta.getTabModelSelector().getCurrentTab().getLaunchType());
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
        // Verifies the tab isn't auto deleted from the TabModel.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage_BackButtonWithTabSwitcher() {
        // clang-format on
        singleAsHomepage_BackButtonWithTabSwitcher();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS +
        "open_ntp_instead_of_start/false/open_start_as_homepage/true"})
    public void testShow_SingleAsHomepageV2_BackButtonWithTabSwitcher() {
        // clang-format on
        singleAsHomepage_BackButtonWithTabSwitcher();
    }

    private void singleAsHomepage_BackButtonWithTabSwitcher() {
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

        // Enters the tab switcher, and choose the new tab. After the tab is opening, press back.
        waitForView(withId(R.id.tab_switcher_button));
        TabUiTestHelper.enterTabSwitcher(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        waitForView(withId(R.id.tab_list_view));
        onView(allOf(withParent(withId(TabUiTestHelper.getTabSwitcherParentId(cta))),
                       withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(1, click()));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        Assert.assertEquals(TabLaunchType.FROM_START_SURFACE,
                cta.getTabModelSelector().getCurrentTab().getLaunchType());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue(StartSurfaceUserData.getKeepTab(
                                cta.getTabModelSelector().getCurrentTab())));

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        // Verifies the new Tab isn't deleted, and Start surface is shown.
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        // Verifies that Chrome goes to the background.
        StartSurfaceTestUtils.pressBackAndVerifyChromeToBackground(mActivityTestRule);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testOpenRecentTabOnStartAndTapBackButtonReturnToStartSurface()
            throws ExecutionException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Taps on the "Recent tabs" menu item.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.recent_tabs_menu_id);
        CriteriaHelper.pollUiThread(() -> cta.getActivityTabProvider().get() != null);
        Assert.assertEquals("The launched tab should have the launch type FROM_START_SURFACE",
                TabLaunchType.FROM_START_SURFACE,
                cta.getActivityTabProvider().get().getLaunchType());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        StartSurfaceTestUtils.pressBack(mActivityTestRule);

        // Tap the back on the "Recent tabs" should take us back to the start surface homepage, and
        // the Tab should be deleted.
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS
            + "open_ntp_instead_of_start/false/open_start_as_homepage/true"})
    // clang-format off
    public void testUserActionLoggedWhenBackToStartSurfaceHomePage() throws ExecutionException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        UserActionTester actionTester = new UserActionTester();

        // Open a MV tile and back.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Assert.assertEquals("The launched tab should have the launch type FROM_START_SURFACE",
                TabLaunchType.FROM_START_SURFACE,
                cta.getActivityTabProvider().get().getLaunchType());
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        // Back gesture on the tab should take us back to the start surface homepage.
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        Assert.assertTrue(
                actionTester.getActions().contains("StartSurface.ShownFromBackNavigation.FromTab"));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "https://crbug.com/1246457")
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testSwipeBackOnStartSurfaceHomePage() throws ExecutionException {
        // clang-format on
        verifySwipeBackOnStartSurfaceHomePage();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "https://crbug.com/1246457")
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testSwipeBackOnStartSurfaceHomePage_BackGestureRefactor()
            throws ExecutionException {
        verifySwipeBackOnStartSurfaceHomePage();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "https://crbug.com/1246457")
    public void testSwipeBackOnTabOfLaunchTypeStartSurface() throws ExecutionException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Assert.assertEquals("The launched tab should have the launch type FROM_START_SURFACE",
                TabLaunchType.FROM_START_SURFACE,
                cta.getActivityTabProvider().get().getLaunchType());

        StartSurfaceTestUtils.gestureNavigateBack(mActivityTestRule);

        // Back gesture on the tab should take us back to the start surface homepage.
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "https://crbug.com/1429106")
    public void testBackButtonOnIncognitoTabOpenedFromStart() throws ExecutionException {
        // This is a test for crbug.com/1315915 to make sure when clicking back button on the
        // incognito tab opened from Start, the non-incognito homepage should show.
        Assume.assumeTrue(mImmediateReturn && !mUseInstantStart);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        onViewWaiting(withId(R.id.logo));

        // Open an incognito tab from Start.
        MostVisitedTilesCarouselLayout mvTilesLayout =
                mActivityTestRule.getActivity().findViewById(R.id.mv_tiles_layout);
        View tileView =
                mvTilesLayout.findTileViewForTesting(mMostVisitedSites.getCurrentSites().get(1));
        openMvTileInAnIncognitoTab(cta, tileView, 1);

        // Go back to Start homepage.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);

        // Open an incognito tab from Start again.
        mvTilesLayout = mActivityTestRule.getActivity().findViewById(R.id.mv_tiles_layout);
        tileView = mvTilesLayout.findTileViewForTesting(mMostVisitedSites.getCurrentSites().get(1));
        openMvTileInAnIncognitoTab(cta, tileView, 2);

        // Press back button and Start homepage should show.
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 1);
        onViewWaiting(allOf(withId(R.id.mv_tiles_layout), isDisplayed()));
    }

    private void openMvTileInAnIncognitoTab(
            ChromeTabbedActivity cta, View tileView, int incognitoTabs) throws ExecutionException {
        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), tileView);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(),
                ContextMenuManager.ContextMenuItemId.OPEN_IN_INCOGNITO_TAB, 0));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        // Verifies a new incognito tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, incognitoTabs);
    }

    private void verifySwipeBackOnStartSurfaceHomePage() {
        // TODO(https://crbug.com/1093632): Requires 2 back press/gesture events now. Make this
        // work with a single event.
        Assume.assumeFalse(mImmediateReturn);
        StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        StartSurfaceTestUtils.waitForStartSurfaceVisible(mLayoutChangedCallbackHelper,
                mCurrentlyActiveLayout, mActivityTestRule.getActivity());

        StartSurfaceTestUtils.gestureNavigateBack(mActivityTestRule);

        // Back gesture on the start surface puts Chrome background.
        ChromeApplicationTestUtils.waitUntilChromeInBackground();
    }

    /**
     * @return Whether both features {@link ChromeFeatureList#INSTANT_START} and
     * {@link ChromeFeatureList#START_SURFACE_RETURN_TIME} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }
}
