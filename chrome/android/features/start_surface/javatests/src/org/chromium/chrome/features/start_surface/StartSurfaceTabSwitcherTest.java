// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.feed.FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_BASE_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_SINGLE_ENABLED_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.KeyEvent;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

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
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.List;

/**
 * Integration tests of the tab switcher when {@link StartSurface} is enabled.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures(
        {ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.EMPTY_STATES})
@DoNotBatch(reason = "StartSurface*Test tests startup behaviours and thus can't be batched.")
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class StartSurfaceTabSwitcherTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private BrowserControlsStateProvider mBrowserControlsStateProvider;

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
    public StartSurfaceTabSwitcherTest(boolean useInstantStart, boolean immediateReturn) {
        ChromeFeatureList.sInstantStart.setForTesting(useInstantStart);

        mUseInstantStart = useInstantStart;
        mImmediateReturn = immediateReturn;
    }

    @Before
    public void setUp() throws IOException {
        MockitoAnnotations.initMocks(this);
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
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsTabSwitcher() {
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForStartSurfaceVisible(mLayoutChangedCallbackHelper,
                    mCurrentlyActiveLayout, mActivityTestRule.getActivity());
            if (isInstantReturn()) {
                // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
                // omnibox.
                return;
            }
            // Single surface is shown as homepage. Clicks "more_tabs" button to get into tab
            // switcher.
            onViewWaiting(withId(R.id.primary_tasks_surface_view));
            StartSurfaceTestUtils.clickTabSwitcherButton(mActivityTestRule.getActivity());
        } else {
            TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!ChromeFeatureList.sStartSurfaceRefactor.isEnabled()) {
            onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        }

        onViewWaiting(allOf(withParent(withId(TabUiTestHelper.getTabSwitcherParentId(cta))),
                              withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testShow_SingleAsHomepage_CloseAllTabsShouldHideTabSwitcher() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        assertEquals(cta.findViewById(R.id.tab_switcher_title).getVisibility(), View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getTabModelSelector().getModel(false).closeAllTabs(); });
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertEquals(cta.findViewById(R.id.tab_switcher_title).getVisibility(), View.GONE);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface", "TabGroup"})
    @CommandLineFlags.Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    @DisabledTest(message = "https://crbug.com/1232695")
    public void testCreateTabWithinTabGroup() throws Exception {
        // Create tab state files for a group with two tabs.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1, mBrowserControlsStateProvider);
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
            onViewWaiting(allOf(withId(R.id.toolbar_left_button),
                                  isDescendantOfA(withId(R.id.bottom_controls))))
                    .perform(click());
        }
        onViewWaiting(
                allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(2));

        // Show start surface through tab grid dialog toolbar plus button and create a new tab by
        // clicking on MV tiles.
        onView(allOf(withId(R.id.toolbar_right_button),
                       isDescendantOfA(withId(R.id.dialog_container_view))))
                .perform(click());
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 2);

        // Verify a tab is created within the group by checking the tab strip and tab model.
        onView(withId(R.id.toolbar_container_view))
                .check(ViewUtils.isEventuallyVisible(
                        allOf(withId(R.id.tab_list_view), isCompletelyDisplayed())));
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.toolbar_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(3));
        assertEquals(1, filter.getTabGroupCount());

        // Show start surface through tab strip plus button and create a new tab by perform a query
        // search in fake box.
        onView(allOf(withId(R.id.toolbar_right_button),
                       isDescendantOfA(withId(R.id.bottom_controls))))
                .perform(click());
        onViewWaiting(withId(R.id.search_box_text))
                .check(matches(isCompletelyDisplayed()))
                .perform(replaceText("wfh tips"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Verify a tab is created within the group by checking the tab strip and tab model.
        onView(withId(R.id.toolbar_container_view))
                .check(ViewUtils.isEventuallyVisible(
                        allOf(withId(R.id.tab_list_view), isCompletelyDisplayed())));
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.toolbar_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(4));
        assertEquals(4, cta.getTabModelSelector().getCurrentModel().getCount());
        assertEquals(1, filter.getTabGroupCount());
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.
    Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS + "/show_tabs_in_mru_order/true"})
    public void test_CarouselTabSwitcherShowTabsInMRUOrder() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout, cta);
        onViewWaiting(withId(R.id.logo));
        Tab tab1 = cta.getCurrentTabModel().getTabAt(0);

        // Launches the first site in MV tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Tab tab2 = cta.getActivityTab();
        // Verifies that the titles of the two Tabs are different.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertNotEquals(tab1.getTitle(), tab2.getTitle()); });

        // Returns to the Start surface.
        StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        ViewUtils.waitForVisibleView(allOf(withParent(withId(R.id.tab_switcher_module_container)),
                withId(R.id.tab_list_view)));

        RecyclerView recyclerView =
                (RecyclerView) StartSurfaceTestUtils.getCarouselTabSwitcherTabListView(cta);
        CriteriaHelper.pollUiThread(() -> 2 == recyclerView.getChildCount());
        // Verifies that the tabs are shown in MRU order: the first card in the carousel Tab
        // switcher is the last created Tab by tapping the MV tile; the second card is the Tab
        // created or restored in setup().
        RecyclerView.ViewHolder firstViewHolder = recyclerView.findViewHolderForAdapterPosition(0);
        TextView title1 = firstViewHolder.itemView.findViewById(R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab2.getTitle(), title1.getText()));

        RecyclerView.ViewHolder secondViewHolder = recyclerView.findViewHolderForAdapterPosition(1);
        TextView title2 = secondViewHolder.itemView.findViewById(R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab1.getTitle(), title2.getText()));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.
    Add({START_SURFACE_TEST_SINGLE_ENABLED_PARAMS + "/show_tabs_in_mru_order/true"})
    public void testShow_GridTabSwitcher_AlwaysShowTabsInCreationOrder() {
        tabSwitcher_AlwaysShowTabsInGridTabSwitcherInCreationOrderImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS
        + "show_tabs_in_mru_order/true/open_ntp_instead_of_start/false/open_start_as_homepage/true",
        DISABLE_ANIMATION_SWITCH})
    public void testShowV2_GridTabSwitcher_AlwaysShowTabsInCreationOrder() {
        // clang-format on
        tabSwitcher_AlwaysShowTabsInGridTabSwitcherInCreationOrderImpl();
    }

    @SuppressWarnings("CheckReturnValue")
    private void tabSwitcher_AlwaysShowTabsInGridTabSwitcherInCreationOrderImpl() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager() != null);
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
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
        int parentViewId = TabUiTestHelper.getIsStartSurfaceRefactorEnabledFromUIThread(cta)
                ? R.id.compositor_view_holder
                : R.id.secondary_tasks_surface_view;
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(
                allOf(withParent(withId(parentViewId)), withId(R.id.tab_list_view)));

        RecyclerView recyclerView = cta.findViewById(parentViewId).findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(() -> 2 == recyclerView.getChildCount());
        // Verifies that the tabs are shown in MRU order: the first card in the Tab switcher is the
        // last created Tab by tapping the MV tile; the second card is the Tab created or restored
        // in setup().
        RecyclerView.ViewHolder firstViewHolder = recyclerView.findViewHolderForAdapterPosition(0);
        TextView title1 = firstViewHolder.itemView.findViewById(R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab1.getTitle(), title1.getText()));

        RecyclerView.ViewHolder secondViewHolder = recyclerView.findViewHolderForAdapterPosition(1);
        TextView title2 = secondViewHolder.itemView.findViewById(R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab2.getTitle(), title2.getText()));
    }

    /**
     * @return Whether both features {@link ChromeFeatureList#INSTANT_START} and
     * {@link ChromeFeatureList#START_SURFACE_RETURN_TIME} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }
}
