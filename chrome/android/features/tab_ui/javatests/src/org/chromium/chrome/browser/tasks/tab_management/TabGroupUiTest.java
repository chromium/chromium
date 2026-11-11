// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.INVISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertFalse;

import static org.chromium.base.test.transit.ViewElement.displayingAtLeastOption;
import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.finishActivity;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.UiSwitches;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** End-to-end tests for TabGroupUi component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.DATA_SHARING, ChromeFeatureList.DATA_SHARING_JOIN_ONLY})
public class TabGroupUiTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(6)
                    .build();

    private RegularNewTabPageStation mNtp;

    @Before
    public void setUp() {
        mNtp = mActivityTestRule.startOnNtp();
        TabUiTestHelper.verifyTabSwitcherLayoutType(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add(UiSwitches.ENABLE_EDGE_TO_EDGE_DEBUG_LAYERS)
    public void testStripShownOnGroupTabPage_JavaViews() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 1st tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);
        assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.HUB));
        waitForTabGroupStrip();
        verifyTabStripFaviconCount(cta, 2);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderStrip_Select5thTabIn10Tabs() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        addBlankTabsAndSettle(cta, 9);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 5th tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 4);

        RecyclerView stripRecyclerView =
                waitForTabGroupStripToSettle(cta, /* expectedCount= */ 10, /* selectedIndex= */ 4);
        mRenderTestRule.render(stripRecyclerView, "5th_tab_selected");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "crbug.com/359640997")
    public void testRenderStrip_Select10thTabIn10Tabs() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        addBlankTabsAndSettle(cta, 9);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 10th tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 9);

        RecyclerView stripRecyclerView =
                waitForTabGroupStripToSettle(cta, /* expectedCount= */ 10, /* selectedIndex= */ 9);
        mRenderTestRule.render(stripRecyclerView, "10th_tab_selected");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderStrip_toggleNotificationBubble() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        AtomicReference<ViewGroup> controlsReference = new AtomicReference<>();
        addBlankTabsAndSettle(cta, 1);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 2nd tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 1);

        waitForTabGroupStripToSettle(cta, /* expectedCount= */ 2, /* selectedIndex= */ 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup bottomToolbar = cta.findViewById(R.id.tab_group_ui_container);
                    RecyclerView stripRecyclerView =
                            bottomToolbar.findViewById(R.id.tab_list_recycler_view);

                    ImageView notificationView =
                            stripRecyclerView.findViewById(R.id.tab_strip_notification_bubble);
                    notificationView.setVisibility(View.VISIBLE);
                    controlsReference.set(bottomToolbar);
                });
        mRenderTestRule.render(
                controlsReference.get(), "bottom_controls_tab_strip_notification_bubble_on");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup bottomToolbar = cta.findViewById(R.id.tab_group_ui_container);
                    RecyclerView stripRecyclerView =
                            bottomToolbar.findViewById(R.id.tab_list_recycler_view);

                    ImageView notificationView =
                            stripRecyclerView.findViewById(R.id.tab_strip_notification_bubble);
                    notificationView.setVisibility(View.GONE);
                    controlsReference.set(bottomToolbar);
                });
        mRenderTestRule.render(
                controlsReference.get(), "bottom_controls_tab_strip_notification_bubble_off");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderStrip_AddTab() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        addBlankTabsAndSettle(cta, 9);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the first tab in group and add one new tab to group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 0);
        waitForTabGroupStripToSettle(cta, /* expectedCount= */ 10, /* selectedIndex= */ 0);
        onView(
                        allOf(
                                withId(R.id.toolbar_new_tab_button),
                                withParent(withId(R.id.main_content)),
                                withEffectiveVisibility(VISIBLE)))
                .perform(click());
        RecyclerView stripRecyclerView =
                waitForTabGroupStripToSettle(cta, /* expectedCount= */ 11, /* selectedIndex= */ 10);
        mRenderTestRule.render(stripRecyclerView, "11th_tab_selected");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableFeatures({ChromeFeatureList.DATA_SHARING, ChromeFeatureList.DATA_SHARING_JOIN_ONLY})
    public void testRenderStrip_BackgroundAddTab() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        addBlankTabsAndSettle(cta, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the first tab in group and add one new tab to group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 0);
        waitForTabGroupStripToSettle(cta, /* expectedCount= */ 3, /* selectedIndex= */ 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab =
                            cta.getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams("about:blank"),
                                            "About Test",
                                            TabLaunchType.FROM_SYNC_BACKGROUND,
                                            null,
                                            TabModel.INVALID_TAB_INDEX);
                    TabModel tabModel = cta.getTabModelSelector().getModel(false);
                    tabModel.mergeListOfTabsToGroup(
                            List.of(tab),
                            tabModel.getTabAt(0),
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
                });
        RecyclerView stripRecyclerView =
                waitForTabGroupStripToSettle(cta, /* expectedCount= */ 4, /* selectedIndex= */ 0);
        mRenderTestRule.render(stripRecyclerView, "background_add_tab");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderStrip_BackgroundAddTab_ResizeableView() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        addBlankTabsAndSettle(cta, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the first tab in group and add one new tab to group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 0);
        waitForTabGroupStripToSettle(cta, /* expectedCount= */ 3, /* selectedIndex= */ 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab =
                            cta.getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams("about:blank"),
                                            "About Test",
                                            TabLaunchType.FROM_SYNC_BACKGROUND,
                                            null,
                                            TabModel.INVALID_TAB_INDEX);
                    TabModel tabModel = cta.getTabModelSelector().getModel(false);
                    tabModel.mergeListOfTabsToGroup(
                            List.of(tab),
                            tabModel.getTabAt(0),
                            /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
                });
        RecyclerView stripRecyclerView =
                waitForTabGroupStripToSettle(cta, /* expectedCount= */ 4, /* selectedIndex= */ 0);
        mRenderTestRule.render(stripRecyclerView, "resizeable_background_add_tab");
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/363049835")
    public void testVisibilityChangeWithOmnibox() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    UserPrefs.get(profile)
                            .setBoolean(Pref.DID_SYNC_TAB_GROUPS_IN_LAST_SESSION, false);
                    TabModelSelector selector =
                            mActivityTestRule.getActivity().getTabModelSelector();
                    TabModel model = selector.getCurrentModel();
                    Tab tab = model.getTabAt(0);
                    model.createSingleTabGroup(tab);
                });

        finishActivity(mActivityTestRule.getActivity());

        // Restart Chrome and make sure tab strip is showing.
        mActivityTestRule.restartMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        waitForTabGroupStrip();

        // The strip should be hidden when omnibox is focused.
        onView(withId(R.id.url_bar)).perform(click());
        onView(
                        allOf(
                                withId(R.id.tab_list_recycler_view),
                                isDescendantOfA(withId(R.id.tab_group_ui_container))))
                .check(matches(withEffectiveVisibility(INVISIBLE)));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add(UiSwitches.ENABLE_EDGE_TO_EDGE_DEBUG_LAYERS)
    @DisableIf.Build(supported_abis_includes = "x86_64", message = "crbug.com/493878143")
    public void testStripShownOnGroupTabPage_BottomControlsSceneLayer() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    UserPrefs.get(profile)
                            .setBoolean(Pref.DID_SYNC_TAB_GROUPS_IN_LAST_SESSION, false);
                    TabModelSelector selector =
                            mActivityTestRule.getActivity().getTabModelSelector();
                    TabModel model = selector.getCurrentModel();
                    Tab tab = model.getTabAt(0);
                    model.createSingleTabGroup(tab);
                });

        finishActivity(mActivityTestRule.getActivity());

        // Restart Chrome and make sure tab strip is showing.
        mActivityTestRule.restartMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        waitForTabGroupStrip();

        BottomControlsCoordinator coordinator =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getToolbarManagerSupplier()
                        .get()
                        .getTabGroupUiBottomControlsCoordinatorForTesting();

        // Scene overlay should be visible.
        CriteriaHelper.pollUiThread(
                () -> coordinator.getSceneLayerForTesting().isSceneOverlayTreeShowing());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    coordinator.simulateEdgeToEdgeChangeForTesting(
                            100,
                            /* isDrawingToEdge= */ true,
                            /* isPageOptedIntoEdgeToEdge= */ true);
                });

        // Scene overlay should remain visible.
        CriteriaHelper.pollUiThread(
                () -> coordinator.getSceneLayerForTesting().isSceneOverlayTreeShowing());
    }

    private static void addBlankTabsAndSettle(ChromeTabbedActivity cta, int count) {
        TabUiTestHelper.addBlankTabs(cta, /* incognito= */ false, count);
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activeTab = cta.getActivityTab();
                    if (activeTab == null || activeTab.isNativePage() || activeTab.isLoading()) {
                        return false;
                    }
                    if (cta.getBrowserControlsManager().getBottomControlOffset() != 0
                            || cta.getBrowserControlsManager().getTopControlOffset() != 0) {
                        return false;
                    }
                    ViewGroup coordinator = cta.findViewById(R.id.coordinator);
                    if (coordinator == null || coordinator.isLayoutRequested()) {
                        return false;
                    }
                    View bottomAppBar = cta.findViewById(R.id.bottom_app_bar_container);
                    if (bottomAppBar != null && bottomAppBar.isLayoutRequested()) {
                        return false;
                    }
                    View tabSwitcherButton =
                            bottomAppBar != null
                                    ? bottomAppBar.findViewById(R.id.tab_switcher_button)
                                    : cta.findViewById(R.id.tab_switcher_button);
                    if (tabSwitcherButton == null) {
                        return false;
                    }
                    return tabSwitcherButton.isShown() && !tabSwitcherButton.isLayoutRequested();
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private RecyclerView waitForTabGroupStripToSettle(
            ChromeTabbedActivity cta, int expectedCount, int selectedIndex) {
        waitForTabGroupStrip();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup bottomToolbar = cta.findViewById(R.id.tab_group_ui_container);
                    if (bottomToolbar != null) {
                        RecyclerView stripRecyclerView =
                                bottomToolbar.findViewById(R.id.tab_list_recycler_view);
                        if (stripRecyclerView != null
                                && stripRecyclerView.getItemAnimator() != null) {
                            stripRecyclerView.getItemAnimator().endAnimations();
                            stripRecyclerView.setItemAnimator(null);
                        }
                    }
                });
        AtomicReference<RecyclerView> recyclerViewReference = new AtomicReference<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    if (cta.getLayoutManager().isLayoutVisible(LayoutType.HUB)) {
                        return false;
                    }
                    ViewGroup bottomToolbar = cta.findViewById(R.id.tab_group_ui_container);
                    if (bottomToolbar == null || bottomToolbar.isLayoutRequested()) {
                        return false;
                    }
                    RecyclerView stripRecyclerView =
                            bottomToolbar.findViewById(R.id.tab_list_recycler_view);
                    if (stripRecyclerView == null) {
                        return false;
                    }
                    if (stripRecyclerView.getAdapter() == null
                            || stripRecyclerView.getAdapter().getItemCount() != expectedCount) {
                        return false;
                    }
                    if (stripRecyclerView.isAnimating()
                            || stripRecyclerView.isLayoutRequested()
                            || stripRecyclerView.getScrollState()
                                    != RecyclerView.SCROLL_STATE_IDLE) {
                        return false;
                    }
                    LinearLayoutManager layoutManager =
                            (LinearLayoutManager) stripRecyclerView.getLayoutManager();
                    if (layoutManager == null) {
                        return false;
                    }
                    int firstVisible = layoutManager.findFirstVisibleItemPosition();
                    int lastVisible = layoutManager.findLastVisibleItemPosition();
                    if (firstVisible == RecyclerView.NO_POSITION
                            || lastVisible == RecyclerView.NO_POSITION) {
                        return false;
                    }
                    if (firstVisible > selectedIndex || selectedIndex > lastVisible) {
                        return false;
                    }
                    // TabGroupUiViewBinder centers the selected tab via scrollToPositionWithOffset
                    // using Math.max(0, index - showingItemsCount / 2). If the strip is
                    // scrollable and can scroll forward, wait until the strip has scrolled to
                    // at least this centered offset rather than capturing the initial offset 0.
                    int visibleCount = Math.max(0, lastVisible - firstVisible);
                    int expectedFirst = Math.max(0, selectedIndex - visibleCount / 2);
                    if (expectedFirst > 0
                            && firstVisible < expectedFirst
                            && stripRecyclerView.canScrollHorizontally(1)) {
                        return false;
                    }
                    View firstChild = layoutManager.findViewByPosition(firstVisible);
                    if (firstChild == null) {
                        return false;
                    }
                    if (firstVisible == 0 || stripRecyclerView.canScrollHorizontally(1)) {
                        if (firstChild.getLeft() != stripRecyclerView.getPaddingLeft()) {
                            return false;
                        }
                    }
                    RecyclerView.ViewHolder selectedHolder =
                            stripRecyclerView.findViewHolderForAdapterPosition(selectedIndex);
                    if (selectedHolder == null
                            || !selectedHolder.itemView.isShown()
                            || selectedHolder.itemView.isLayoutRequested()
                            || selectedHolder.itemView.getForeground() == null) {
                        return false;
                    }
                    if (selectedHolder instanceof SimpleRecyclerViewAdapter.ViewHolder selectedVh) {
                        if (selectedVh.model == null
                                || !Boolean.TRUE.equals(
                                        selectedVh.model.get(TabProperties.IS_SELECTED))) {
                            return false;
                        }
                    }
                    for (int i = 0; i < stripRecyclerView.getChildCount(); i++) {
                        View child = stripRecyclerView.getChildAt(i);
                        if (child == null || child.isLayoutRequested() || child.getAlpha() < 1.0f) {
                            return false;
                        }
                        RecyclerView.ViewHolder vh = stripRecyclerView.getChildViewHolder(child);
                        if (vh instanceof SimpleRecyclerViewAdapter.ViewHolder simpleVh) {
                            if (simpleVh.model == null
                                    || !Boolean.TRUE.equals(
                                            simpleVh.model.get(TabProperties.FAVICON_FETCHED))) {
                                return false;
                            }
                        }
                        ImageButton button = child.findViewById(R.id.tab_strip_item_button);
                        if (button == null || button.getDrawable() == null) {
                            return false;
                        }
                    }
                    recyclerViewReference.set(stripRecyclerView);
                    return true;
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return recyclerViewReference.get();
    }

    private void waitForTabGroupStrip() {
        waitForView(
                allOf(
                        withId(R.id.tab_list_recycler_view),
                        isDescendantOfA(withId(R.id.tab_group_ui_container))),
                displayingAtLeastOption(100));
    }
}
