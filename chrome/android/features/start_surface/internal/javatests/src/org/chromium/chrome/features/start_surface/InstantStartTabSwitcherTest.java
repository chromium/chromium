// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.view.View;
import android.widget.ImageView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.MathUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Integration tests of tab switcher with Instant Start which requires 2-stage initialization for
 * Clank startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.
    Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
    ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
    ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INSTANT_START})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE})
public class InstantStartTabSwitcherTest {
    // clang-format on
    private static final String SHADOW_VIEW_TAG = "TabListViewShadow";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().setRevision(1).build();

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
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));
        Assert.assertTrue(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue());

        mActivityTestRule.waitForActivityNativeInitializationComplete();

        StartSurfaceTestUtils.clickMoreTabs(cta);

        onViewWaiting(allOf(withParent(withId(org.chromium.chrome.test.R.id.tasks_surface_body)),
                withId(org.chromium.chrome.test.R.id.tab_list_view)));
        Assert.assertFalse(cta.findViewById(org.chromium.chrome.test.R.id.url_bar).isFocused());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    @DisabledTest(message = "Test doesn't work with FeedV2. FeedV1 is removed crbug.com/1165828.")
    public void renderTabSwitcher() throws IOException, InterruptedException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(2);
        TabAttributeCache.setTitleForTesting(0, "title");
        TabAttributeCache.setTitleForTesting(1, "漢字");
        TabAttributeCache.setTitleForTesting(2, "اَلْعَرَبِيَّةُ");

        // Must be after StartSurfaceTestUtils.createTabStateFile() to read these files.
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        RecyclerView recyclerView = cta.findViewById(org.chromium.chrome.test.R.id.tab_list_view);
        CriteriaHelper.pollUiThread(() -> allCardsHaveThumbnail(recyclerView));
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
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    @DisableIf.Build(message = "Flaky. See https://crbug.com/1091311",
        sdk_is_greater_than = Build.VERSION_CODES.O)
    public void renderTabGroups() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(2);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(3);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(4);
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
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        RecyclerView recyclerView = cta.findViewById(org.chromium.chrome.test.R.id.tab_list_view);
        CriteriaHelper.pollUiThread(() -> allCardsHaveThumbnail(recyclerView));
        // TODO(crbug.com/1065314): Tab group cards should not have favicons.
        mRenderTestRule.render(cta.findViewById(org.chromium.chrome.test.R.id.tab_list_view),
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
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    @DisableIf.Build(message = "Flaky. See https://crbug.com/1091311",
        sdk_is_greater_than = Build.VERSION_CODES.O)
    public void renderTabGroups_ThemeRefactor() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(2);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(3);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(4);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(5);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(6);
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
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        RecyclerView recyclerView = cta.findViewById(org.chromium.chrome.test.R.id.tab_list_view);
        CriteriaHelper.pollUiThread(() -> allCardsHaveThumbnail(recyclerView));
        mRenderTestRule.render(cta.findViewById(org.chromium.chrome.test.R.id.tab_list_view),
                "tabSwitcher_tabGroups_theme_enforcement");
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    public void testSingleAsHomepage_CloseTabInCarouselTabSwitcher()
            throws IOException, ExecutionException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        onView(allOf(withParent(
                             withId(org.chromium.chrome.test.R.id.carousel_tab_switcher_container)),
                       withId(org.chromium.chrome.test.R.id.tab_list_view)))
                .check(matches(isDisplayed()));
        RecyclerView tabListView = cta.findViewById(org.chromium.chrome.test.R.id.tab_list_view);
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
    public void testScrollToSelectedTab() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, null, 5);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        StartSurfaceTestUtils.clickMoreTabs(cta);
        onViewWaiting(withId(org.chromium.chrome.test.R.id.secondary_tasks_surface_view));

        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        onView(allOf(withId(org.chromium.chrome.test.R.id.tab_list_view),
                       withParent(withId(org.chromium.chrome.test.R.id.tasks_surface_body))))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;
                    Assert.assertTrue(v instanceof RecyclerView);
                    LinearLayoutManager layoutManager =
                            (LinearLayoutManager) ((RecyclerView) v).getLayoutManager();
                    assertEquals(7, layoutManager.findLastVisibleItemPosition());
                });

        // On tab switcher page, shadow is handled by TabListRecyclerView itself, so toolbar shadow
        // shouldn't show.
        onView(withId(org.chromium.chrome.test.R.id.toolbar_shadow))
                .check(matches(not(isDisplayed())));

        // Scroll the tab list a little bit and shadow should show.
        onView(allOf(withId(org.chromium.chrome.test.R.id.tab_list_view),
                       withParent(withId(org.chromium.chrome.test.R.id.tasks_surface_body))))
                .perform(swipeUp());
        onView(allOf(withTagValue(is(SHADOW_VIEW_TAG)),
                       isDescendantOfA(withId(org.chromium.chrome.test.R.id.tasks_surface_body))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    public void doNotRestoreEmptyTabs() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(
                new int[] {0, 1}, new String[] {"", "about:blank"});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setTitleForTesting(0, "");
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForOverviewVisible(mActivityTestRule.getActivity());
        ViewUtils.onViewWaiting(withId(org.chromium.chrome.test.R.id.tab_list_view));
        Assert.assertEquals(
                1, PseudoTab.getAllPseudoTabsFromStateFile(mActivityTestRule.getActivity()).size());
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    public void testSingleAsHomepage_Landscape_TabSize() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        onViewWaiting(
                allOf(withId(org.chromium.chrome.start_surface.R.id.feed_stream_recycler_view),
                        isDisplayed()));

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
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        onViewWaiting(allOf(withId(org.chromium.chrome.test.R.id.tab_thumbnail), isDisplayed()));

        View tabThumbnail = cta.findViewById(org.chromium.chrome.test.R.id.tab_thumbnail);
        float defaultRatio = (float) TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO.getValue();
        defaultRatio = MathUtils.clamp(defaultRatio, 0.5f, 2.0f);
        assertEquals(tabThumbnail.getMeasuredHeight(),
                (int) (tabThumbnail.getMeasuredWidth() * 1.0 / defaultRatio), 2);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testShowTabSwitcherWhenHomepageDisabled() throws IOException {
        // clang-format on
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        testShowTabSwitcherWhenHomepageDisabledWithImmediateReturnImpl();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testShowTabSwitcherWhenHomepageDisabled_NoInstant() throws IOException {
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        testShowTabSwitcherWhenHomepageDisabledWithImmediateReturnImpl();
    }

    private void testShowTabSwitcherWhenHomepageDisabledWithImmediateReturnImpl()
            throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(false));
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        // Launches Chrome and verifies that the Tab switcher is showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        Assert.assertTrue(cta.getLayoutManager().overviewVisible());
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(startSurfaceCoordinator.getController().getStartSurfaceState(),
                    StartSurfaceState.NOT_SHOWN);
        });
    }

    private boolean allCardsHaveThumbnail(RecyclerView recyclerView) {
        RecyclerView.Adapter adapter = recyclerView.getAdapter();
        assert adapter != null;
        for (int i = 0; i < adapter.getItemCount(); i++) {
            RecyclerView.ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(i);
            if (viewHolder != null) {
                ImageView thumbnail = viewHolder.itemView.findViewById(
                        org.chromium.chrome.test.R.id.tab_thumbnail);
                if (!(thumbnail.getDrawable() instanceof BitmapDrawable)) return false;
                BitmapDrawable drawable = (BitmapDrawable) thumbnail.getDrawable();
                Bitmap bitmap = drawable.getBitmap();
                if (bitmap == null) return false;
            }
        }
        return true;
    }

    private int getRelatedTabListSizeOnUiThread(TabModelFilter tabModelFilter) {
        AtomicInteger res = new AtomicInteger();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { res.set(tabModelFilter.getRelatedTabList(2).size()); });
        return res.get();
    }
}
