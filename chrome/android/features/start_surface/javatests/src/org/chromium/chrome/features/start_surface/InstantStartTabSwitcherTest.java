// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;


import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Build.VERSION_CODES;
import android.view.KeyEvent;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Integration tests of tab switcher with Instant Start which requires 2-stage initialization for
 * Clank startup.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@EnableFeatures({
    ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study,",
    ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
    ChromeFeatureList.INSTANT_START,
})
@DisableFeatures({ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID})
@Restriction({
    Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE
})
@DoNotBatch(reason = "This test suite tests startup behaviours and thus can't be batched.")
public class InstantStartTabSwitcherTest {
    private static final String SHADOW_VIEW_TAG = "TabListViewShadow";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(4)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock public BrowserControlsStateProvider mBrowserControlsStateProvider;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    /**
     * {@link ParameterProvider} used for parameterized test that provides whether last visited tab
     * is a search result page.
     */
    public static class LastVisitedTabIsSRPTestParams implements ParameterProvider {
        private static final List<ParameterSet> sLVTIsSRPTestParams =
                Arrays.asList(
                        new ParameterSet().value(false).name("SingleTab_NotSRP"),
                        new ParameterSet().value(true).name("SingleTab_SRP"));

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
     * Tests that clicking the tab switcher button won't make Omnibox get focused when single tab is
     * shown on the StartSurface.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void startSurfaceTabSwitcherButtonTest() throws IOException {
        StartSurfaceTestUtils.createTabStatesAndMetadataFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));

        mActivityTestRule.waitForActivityNativeInitializationComplete();

        StartSurfaceTestUtils.clickTabSwitcherButton(cta);

        onViewWaiting(
                allOf(
                        isDescendantOfA(
                                withId(
                                        TabUiTestHelper.getTabSwitcherAncestorId(
                                                mActivityTestRule.getActivity()))),
                        withId(org.chromium.chrome.test.R.id.tab_list_recycler_view)));
        Assert.assertFalse(cta.findViewById(org.chromium.chrome.test.R.id.url_bar).isFocused());
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS,
        FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH
    })
    // TODO(https://crbug.com/1500080): Fix this test with "start surface refactor" enabled. Hub
    // is disabled because it requires "start surface refactor" to be enabled.
    @DisableFeatures({ChromeFeatureList.START_SURFACE_REFACTOR, ChromeFeatureList.ANDROID_HUB})
    public void testScrollToSelectedTab() throws Exception {
        StartSurfaceTestUtils.createTabStatesAndMetadataFile(
                new int[] {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, null, 5);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        StartSurfaceTestUtils.clickTabSwitcherButton(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);

        int tabSwitcherAncestorId = TabUiTestHelper.getTabSwitcherAncestorId(cta);
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        onView(
                        allOf(
                                withId(org.chromium.chrome.test.R.id.tab_list_recycler_view),
                                isDescendantOfA(withId(tabSwitcherAncestorId))))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            Assert.assertTrue(v instanceof RecyclerView);
                            LinearLayoutManager layoutManager =
                                    (LinearLayoutManager) ((RecyclerView) v).getLayoutManager();
                            assertEquals(2, layoutManager.findFirstVisibleItemPosition());
                            assertTrue(
                                    layoutManager.isViewPartiallyVisible(
                                            layoutManager.getChildAt(5), false, false));
                            assertEquals(7, layoutManager.findLastVisibleItemPosition());
                        });

        // On tab switcher page, shadow is handled by TabListRecyclerView itself, so toolbar shadow
        // shouldn't show.
        onView(withId(org.chromium.chrome.test.R.id.toolbar_hairline))
                .check(matches(not(isDisplayed())));

        // Scroll the tab list a little bit and shadow should show.
        onView(
                        allOf(
                                withId(org.chromium.chrome.test.R.id.tab_list_recycler_view),
                                isDescendantOfA(withId(tabSwitcherAncestorId))))
                .perform(swipeUp());
        onView(
                        allOf(
                                withTagValue(is(SHADOW_VIEW_TAG)),
                                isDescendantOfA(withId(tabSwitcherAncestorId))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testShowStartWhenHomepageDisabledWithImmediateReturn() throws IOException {
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
        StartSurfaceTestUtils.createTabStatesAndMetadataFile(new int[] {0});
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
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Assert.assertEquals(
                                startSurfaceCoordinator.getStartSurfaceState(),
                                StartSurfaceState.SHOWN_TABSWITCHER);
                    });
        }
    }

    @Test
    @MediumTest
    @UseMethodParameter(LastVisitedTabIsSRPTestParams.class)
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS
    })
    public void testRecordLastVisitedTabIsSRPHistogram_Instant(boolean isSRP) throws IOException {
        testRecordLastVisitedTabIsSRPHistogram(isSRP);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @UseMethodParameter(LastVisitedTabIsSRPTestParams.class)
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS
    })
    public void testRecordLastVisitedTabIsSRPHistogram_NoInstant(boolean isSRP) throws IOException {
        testRecordLastVisitedTabIsSRPHistogram(isSRP);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS
    })
    @DisableIf.Build(
            message = "https://crbug.com/1470412",
            sdk_is_greater_than = VERSION_CODES.M,
            sdk_is_less_than = VERSION_CODES.Q)
    public void testSaveIsLastVisitedTabSRP() throws Exception {
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
        Assert.assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, false));

        // Simulates pressing Chrome's icon and launching Chrome from warm start.
        mActivityTestRule.resumeMainActivityFromLauncher();

        // Create a non search result tab and check the shared preferences.
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceTestUtils.launchFirstMVTile(cta, 1);

        StartSurfaceTestUtils.pressHome();
        Assert.assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, false));
    }

    private void testRecordLastVisitedTabIsSRPHistogram(boolean isSRP) throws IOException {
        StartSurfaceTestUtils.createTabStatesAndMetadataFile(
                new int[] {0, 1},
                new String[] {"https://www.google.com/search?q=test", "https://www.google.com"},
                isSRP ? 0 : 1);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "Google SRP");
        TabAttributeCache.setTitleForTesting(1, "Google Homepage");
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, isSRP);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        StartSurfaceTestUtils.waitForDeferredStartup(mActivityTestRule);

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ReturnToChromeUtil
                                .LAST_VISITED_TAB_IS_SRP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA,
                        isSRP ? 1 : 0));
    }

    private int getRelatedTabListSizeOnUiThread(TabModelFilter tabModelFilter) {
        AtomicInteger res = new AtomicInteger();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    res.set(tabModelFilter.getRelatedTabList(2).size());
                });
        return res.get();
    }
}
