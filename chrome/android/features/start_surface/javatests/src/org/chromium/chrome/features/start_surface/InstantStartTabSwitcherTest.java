// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.KeyEvent;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Integration tests of tab switcher with Instant Start which requires 2-stage initialization for
 * Clank startup.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
// clang-format off
@CommandLineFlags.
    Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study,",
    ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INSTANT_START,
    ChromeFeatureList.EMPTY_STATES})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE})
@DoNotBatch(reason = "This test suite tests startup behaviours and thus can't be batched.")
public class InstantStartTabSwitcherTest {
    // clang-format on
    private static final String SHADOW_VIEW_TAG = "TabListViewShadow";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(4)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    public BrowserControlsStateProvider mBrowserControlsStateProvider;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    /**
     * {@link ParameterProvider} used for parameterized test that provides whether it's single tab
     * switcher or carousel tab switcher and whether last visited tab is a search result page.
     */
    public static class LastVisitedTabIsSRPTestParams implements ParameterProvider {
        private static final List<ParameterSet> sLVTIsSRPTestParams =
                Arrays.asList(new ParameterSet().value(false, false).name("CarouselTab_NotSRP"),
                        new ParameterSet().value(true, false).name("SingleTab_NotSRP"),
                        new ParameterSet().value(false, true).name("CarouselTab_SRP"),
                        new ParameterSet().value(true, true).name("SingleTab_SRP"));

        @Override
        public List<ParameterSet> getParameters() {
            return sLVTIsSRPTestParams;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ReturnToChromeUtil.setSkipInitializationCheckForTesting(true);
    }

    @After
    public void tearDown() {
        if (mActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        }
    }

    /**
     * Tests that clicking the "more_tabs" button won't make Omnibox get focused when single tab is
     * shown on the StartSurface.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS + "/show_last_active_tab_only/true"})
    public void startSurfaceMoreTabsButtonTest() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));
        Assert.assertTrue(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue());

        mActivityTestRule.waitForActivityNativeInitializationComplete();

        StartSurfaceTestUtils.clickTabSwitcherButton(cta);

        onViewWaiting(allOf(withParent(withId(TabUiTestHelper.getTabSwitcherParentId(
                                    mActivityTestRule.getActivity()))),
                withId(org.chromium.chrome.test.R.id.tab_list_recycler_view)));
        Assert.assertFalse(cta.findViewById(org.chromium.chrome.test.R.id.url_bar).isFocused());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS + "/show_last_active_tab_only/false"})
    public void renderTabSwitcher() throws IOException, InterruptedException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(2, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "title");
        TabAttributeCache.setTitleForTesting(1, "漢字");
        TabAttributeCache.setTitleForTesting(2, "اَلْعَرَبِيَّةُ");

        // Must be after StartSurfaceTestUtils.createTabStateFile() to read these files.
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        RecyclerView recyclerView =
                (RecyclerView) StartSurfaceTestUtils.getCarouselTabSwitcherTabListView(cta);
        TabUiTestHelper.waitForThumbnailsToFetch(recyclerView);
        mRenderTestRule.render(recyclerView, "tabSwitcher_3tabs");

        // Resume native initialization and make sure the GTS looks the same.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);

        Assert.assertEquals(3, cta.getTabModelSelector().getCurrentModel().getCount());
        // TODO(crbug.com/1065314): find a better way to wait for a stable rendering.
        Thread.sleep(2000);
        // The titles on the tab cards changes to "Google" because we use M26_GOOGLE_COM.
        mRenderTestRule.render(recyclerView, "tabSwitcher_3tabs_postNative");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS + "/show_last_active_tab_only/false"})
    @DisableIf.Build(message = "Flaky. See https://crbug.com/1091311",
        sdk_is_greater_than = Build.VERSION_CODES.O)
    public void renderTabGroups() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(2, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(3, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(4, mBrowserControlsStateProvider);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        TabAttributeCache.setRootIdForTesting(2, 0);
        TabAttributeCache.setRootIdForTesting(3, 3);
        TabAttributeCache.setRootIdForTesting(4, 3);
        // StartSurfaceTestUtils.createTabStateFile() has to be after setRootIdForTesting() to get
        // root IDs.
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2, 3, 4});

        // Must be after StartSurfaceTestUtils.createTabStateFile() to read these files.
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        RecyclerView recyclerView =
                (RecyclerView) StartSurfaceTestUtils.getCarouselTabSwitcherTabListView(cta);
        TabUiTestHelper.waitForThumbnailsToFetch(recyclerView);
        // TODO(crbug.com/1065314): Tab group cards should not have favicons.
        mRenderTestRule.render(StartSurfaceTestUtils.getCarouselTabSwitcherTabListView(cta),
                "tabSwitcher_tabGroups_aspect_ratio_point85");

        // Resume native initialization and make sure the GTS looks the same.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);

        Assert.assertEquals(5, cta.getTabModelSelector().getCurrentModel().getCount());
        Assert.assertEquals(2,
                cta.getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getCount());
        Assert.assertEquals(3,
                getRelatedTabListSizeOnUiThread(cta.getTabModelSelector()
                                                        .getTabModelFilterProvider()
                                                        .getCurrentTabModelFilter()));
        // TODO(crbug.com/1065314): fix thumbnail changing in post-native rendering and make sure
        //  post-native GTS looks the same.
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS + "/show_last_active_tab_only/false"})
    @DisableIf.Build(message = "Flaky. See https://crbug.com/1091311",
        sdk_is_greater_than = Build.VERSION_CODES.O)
    public void renderTabGroups_ThemeRefactor() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(2, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(3, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(4, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(5, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(6, mBrowserControlsStateProvider);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        TabAttributeCache.setRootIdForTesting(2, 0);
        TabAttributeCache.setRootIdForTesting(3, 0);
        TabAttributeCache.setRootIdForTesting(4, 0);
        TabAttributeCache.setRootIdForTesting(5, 5);
        TabAttributeCache.setRootIdForTesting(6, 5);

        // StartSurfaceTestUtils.createTabStateFile() has to be after setRootIdForTesting() to get
        // root IDs.
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2, 3, 4, 5, 6});

        // Must be after StartSurfaceTestUtils.createTabStateFile() to read these files.
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        RecyclerView recyclerView =
                (RecyclerView) StartSurfaceTestUtils.getCarouselTabSwitcherTabListView(cta);
        TabUiTestHelper.waitForThumbnailsToFetch(recyclerView);
        mRenderTestRule.render(StartSurfaceTestUtils.getCarouselTabSwitcherTabListView(cta),
                "tabSwitcher_tabGroups_theme_enforcement");
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS + "/show_last_active_tab_only/false"})
    public void testSingleAsHomepage_CloseTabInCarouselTabSwitcher()
            throws IOException, ExecutionException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        onView(allOf(withParent(
                             withId(org.chromium.chrome.test.R.id.tab_switcher_module_container)),
                       withId(org.chromium.chrome.test.R.id.tab_list_recycler_view)))
                .check(matches(isDisplayed()));
        RecyclerView tabListView =
                (RecyclerView) StartSurfaceTestUtils.getCarouselTabSwitcherTabListView(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> tabListView.getChildAt(0)
                                   .findViewById(org.chromium.chrome.test.R.id.action_button)
                                   .performClick());

        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertEquals(
                cta.findViewById(org.chromium.chrome.test.R.id.tab_switcher_title).getVisibility(),
                View.GONE);
    }

    @Test
    @LargeTest
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS,
        FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH})
    public void testScrollToSelectedTab() throws Exception {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, null, 5);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        StartSurfaceTestUtils.clickTabSwitcherButton(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);

        int tabSwitcherParentViewId = TabUiTestHelper.getTabSwitcherParentId(cta);
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        onView(allOf(withId(org.chromium.chrome.test.R.id.tab_list_recycler_view),
                       withParent(withId(tabSwitcherParentViewId))))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;
                    Assert.assertTrue(v instanceof RecyclerView);
                    LinearLayoutManager layoutManager =
                            (LinearLayoutManager) ((RecyclerView) v).getLayoutManager();
                    assertEquals(2, layoutManager.findFirstVisibleItemPosition());
                    assertTrue(layoutManager.isViewPartiallyVisible(
                            layoutManager.getChildAt(5), false, false));
                    assertEquals(7, layoutManager.findLastVisibleItemPosition());
                });

        // On tab switcher page, shadow is handled by TabListRecyclerView itself, so toolbar shadow
        // shouldn't show.
        onView(withId(org.chromium.chrome.test.R.id.toolbar_hairline))
                .check(matches(not(isDisplayed())));

        // Scroll the tab list a little bit and shadow should show.
        onView(allOf(withId(org.chromium.chrome.test.R.id.tab_list_recycler_view),
                       withParent(withId(tabSwitcherParentViewId))))
                .perform(swipeUp());
        onView(allOf(withTagValue(is(SHADOW_VIEW_TAG)),
                       isDescendantOfA(withId(tabSwitcherParentViewId))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            INSTANT_START_TEST_BASE_PARAMS + "/show_last_active_tab_only/false"
                    + "/open_ntp_instead_of_start/false/open_start_as_homepage/true"})
    // clang-format off
    public void testSingleAsHomepage_Landscape_TabSize() throws IOException {
     testSingleAsHomepage_Landscape_TabSize_impl();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS + "/show_last_active_tab_only/false"
            + "/open_ntp_instead_of_start/false/open_start_as_homepage/true"})
    // clang-format off
    public void testSingleAsHomepage_Landscape_TabSize_RefactorEnabled() throws IOException {
        testSingleAsHomepage_Landscape_TabSize_impl();
    }

   private void testSingleAsHomepage_Landscape_TabSize_impl() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        onViewWaiting(allOf(withId(R.id.feed_stream_recycler_view), isDisplayed()));

        // Rotate to landscape mode.
        ActivityTestUtils.rotateActivityToOrientation(cta, ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    cta.getResources().getConfiguration().orientation, is(ORIENTATION_LANDSCAPE));
        });

        // Launch the first MV tile to open a tab.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 0);
        StartSurfaceTestUtils.pressHomePageButton(cta);

        // Wait for thumbnail to show.
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        onViewWaiting(allOf(withId(org.chromium.chrome.test.R.id.tab_thumbnail), isDisplayed()));

        RecyclerView recyclerView =
                (RecyclerView) StartSurfaceTestUtils.getCarouselTabSwitcherTabListView(cta);
        View tabThumbnail = recyclerView.findViewById(org.chromium.chrome.test.R.id.tab_thumbnail);
        assertEquals(tabThumbnail.getMeasuredHeight(),
                (int) (tabThumbnail.getMeasuredWidth() * 1.0 / TabUtils.THUMBNAIL_ASPECT_RATIO), 2);

        ActivityTestUtils.clearActivityOrientation(cta);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testShowStartWhenHomepageDisabledWithImmediateReturn() throws IOException {
        // clang-format on
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertEquals(
                0, StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.getValue());
        testShowStartWhenHomepageDisabledWithImmediateReturnImpl();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testShowStartWhenHomepageDisabledWithImmediateReturn_NoInstant()
            throws IOException {
        Assert.assertFalse(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertEquals(
                0, StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.getValue());
        testShowStartWhenHomepageDisabledWithImmediateReturnImpl();
    }

    private void testShowStartWhenHomepageDisabledWithImmediateReturnImpl() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "Google");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(false));
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        // Launches Chrome and verifies that the Tab switcher is showing.
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        // After the Start surface refactoring is enabled, the StartSurfaceState.SHOWN_TABSWITCHER
        // will go away.
        if (!TabUiTestHelper.getIsStartSurfaceRefactorEnabledFromUIThread(cta)) {
            StartSurfaceCoordinator startSurfaceCoordinator =
                    StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                Assert.assertEquals(startSurfaceCoordinator.getStartSurfaceState(),
                        StartSurfaceState.SHOWN_TABSWITCHER);
            });
        }
    }

    @Test
    @MediumTest
    @UseMethodParameter(LastVisitedTabIsSRPTestParams.class)
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    // clang-format on
    public void testRecordLastVisitedTabIsSRPHistogram_Instant(
            boolean isSingleTabSwitcher, boolean isSRP) throws IOException {
        testRecordLastVisitedTabIsSRPHistogram(isSingleTabSwitcher, isSRP);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @UseMethodParameter(LastVisitedTabIsSRPTestParams.class)
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    // clang-format on
    public void testRecordLastVisitedTabIsSRPHistogram_NoInstant(
            boolean isSingleTabSwitcher, boolean isSRP) throws IOException {
        testRecordLastVisitedTabIsSRPHistogram(isSingleTabSwitcher, isSRP);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    // clang-format on
    @DisableIf.Build(message = "https://crbug.com/1470412", sdk_is_greater_than = VERSION_CODES.M,
            sdk_is_less_than = VERSION_CODES.Q)
    public void
    testSaveIsLastVisitedTabSRP() throws Exception {
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        StartSurfaceTestUtils.waitForDeferredStartup(mActivityTestRule);

        // Create a new search result tab by perform a query search in fake box.
        onViewWaiting(withId(R.id.search_box_text))
                .check(matches(isCompletelyDisplayed()))
                .perform(replaceText("test"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        StartSurfaceTestUtils.pressHome();
        Assert.assertTrue(SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, false));

        // Simulates pressing Chrome's icon and launching Chrome from warm start.
        mActivityTestRule.resumeMainActivityFromLauncher();

        // Create a non search result tab and check the shared preferences.
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceTestUtils.launchFirstMVTile(cta, 1);

        StartSurfaceTestUtils.pressHome();
        Assert.assertFalse(SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, false));
    }

    private void testRecordLastVisitedTabIsSRPHistogram(boolean isSingleTabSwitcher, boolean isSRP)
            throws IOException {
        StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.setForTesting(
                isSingleTabSwitcher);
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1},
                new String[] {"https://www.google.com/search?q=test", "https://www.google.com"},
                isSRP ? 0 : 1);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "Google SRP");
        TabAttributeCache.setTitleForTesting(1, "Google Homepage");
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, isSRP);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        StartSurfaceTestUtils.waitForDeferredStartup(mActivityTestRule);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ReturnToChromeUtil
                                .LAST_VISITED_TAB_IS_SRP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA,
                        isSRP ? 1 : 0));
    }

    private int getRelatedTabListSizeOnUiThread(TabModelFilter tabModelFilter) {
        AtomicInteger res = new AtomicInteger();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { res.set(tabModelFilter.getRelatedTabList(2).size()); });
        return res.get();
    }
}
