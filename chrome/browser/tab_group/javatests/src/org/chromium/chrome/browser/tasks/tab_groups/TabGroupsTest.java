// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Integration tests for the Tab Groups feature on Android.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_STARTUP_PROMOS})
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
@Batch(Batch.PER_CLASS)
public class TabGroupsTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Mock
    private TabModelObserver mTabModelFilterObserver;

    private TabModel mTabModel;
    private TabGroupModelFilter mTabGroupModelFilter;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabModel = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        mTabGroupModelFilter = (TabGroupModelFilter) sActivityTestRule.getActivity()
                                       .getTabModelSelector()
                                       .getTabModelFilterProvider()
                                       .getTabModelFilter(false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabGroupModelFilter.addObserver(mTabModelFilterObserver); });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabGroupModelFilter.removeObserver(mTabModelFilterObserver); });
    }

    private void prepareTabs(List<Integer> tabsPerGroup) {
        for (int tabsToCreate : tabsPerGroup) {
            List<Tab> tabs = new ArrayList<>();
            for (int i = 0; i < tabsToCreate; i++) {
                Tab tab = ChromeTabUtils.fullyLoadUrlInNewTab(
                        InstrumentationRegistry.getInstrumentation(),
                        sActivityTestRule.getActivity(), "about:blank", /*incognito=*/false);
                tabs.add(tab);
            }
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mTabGroupModelFilter.mergeListOfTabsToGroup(tabs, tabs.get(0), false, false);
            });
        }
    }

    @Test
    @SmallTest
    public void testPreventAddingUngroupedTabInsideTabGroup() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab 1, 2, 3
        // Tab (tab added here)
        // Tab 4
        Tab tab = addTabAt(/*index=*/3, /*parent=*/null);
        tabs.add(4, tab);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(true);
    }

    @Test
    @SmallTest
    public void testPreventAddingGroupedTabAwayFromGroup_BeforeGroup() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab (tab added here), 1, 2, 3
        // Tab 4
        Tab tab = addTabAt(/*index=*/0, /*parent=*/tabs.get(1));
        tabs.add(1, tab);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(true);
    }

    @Test
    @SmallTest
    public void testPreventAddingGroupedTabAwayFromGroup_AfterGroup() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab 1, 2, 3, (tab added here)
        // Tab 4
        Tab tab = addTabAt(/*index=*/mTabModel.getCount(), /*parent=*/tabs.get(1));
        tabs.add(4, tab);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(true);
    }

    @Test
    @SmallTest
    public void testAllowAddingGroupedTabInsideGroup() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab 1, (tab added here), 2, 3
        // Tab 4
        Tab tab = addTabAt(/*index=*/2, /*parent=*/tabs.get(1));
        tabs.add(2, tab);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(true);
    }

    @Test
    @SmallTest
    public void testOrderValid_WithIncorrectOrder() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab 1, 3
        // Tab 4
        // Move tab 2 here still grouped with tab 1
        Tab tab2 = tabs.get(2);
        moveTab(tab2, 5);
        tabs.remove(tab2);
        tabs.add(tab2);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(false);
    }

    @Test
    @SmallTest
    public void testOrderValid_WithIncorrectOrder_NestedGroup() {
        prepareTabs(Arrays.asList(new Integer[] {3, 2, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab 1, (group 4, 5), 2, 3
        // Tab 6
        Tab tab4 = tabs.get(4);
        Tab tab5 = tabs.get(5);
        moveTab(tab4, 2);
        moveTab(tab5, 3);
        tabs.remove(tab4);
        tabs.remove(tab5);
        tabs.add(2, tab4);
        tabs.add(3, tab5);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(false);
    }

    @Test
    @SmallTest
    public void testOrderValid_WithValidOrder() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 2, 3, 1
        // Tab 4
        // Move tab 0 here
        Tab tab0 = tabs.get(0);
        moveTab(tab0, 5);
        tabs.remove(tab0);
        tabs.add(tab0);
        Tab tab1 = tabs.get(0);
        moveTab(tab1, 3);
        tabs.remove(tab1);
        tabs.add(2, tab1);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(true);
    }

    @Test
    @SmallTest
    public void testTabModelFilterObserverUndoClosure() {
        // Four tabs plus the first blank one.
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        final List<Tab> tabs = getCurrentTabs();
        assertEquals(5, tabs.size());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabModel.setIndex(2, TabSelectionType.FROM_USER, false);
            mTabModel.closeAllTabs();
        });

        List<Tab> noTabs = getCurrentTabs();
        assertTrue(noTabs.isEmpty());

        InOrder calledInOrder = inOrder(mTabModelFilterObserver);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (Tab tab : tabs) {
                mTabModel.cancelTabClosure(tab.getId());
            }
        });
        // Ensure didSelectTab is called and the call occurs after the tab closure is actually
        // undone.
        calledInOrder.verify(mTabModelFilterObserver).tabClosureUndone(eq(tabs.get(0)));
        calledInOrder.verify(mTabModelFilterObserver)
                .didSelectTab(eq(tabs.get(0)), eq(TabSelectionType.FROM_UNDO), /*lastId=*/eq(-1));
        calledInOrder.verify(mTabModelFilterObserver).tabClosureUndone(eq(tabs.get(1)));
        calledInOrder.verify(mTabModelFilterObserver).tabClosureUndone(eq(tabs.get(2)));
        calledInOrder.verify(mTabModelFilterObserver).tabClosureUndone(eq(tabs.get(3)));
        calledInOrder.verify(mTabModelFilterObserver).tabClosureUndone(eq(tabs.get(4)));

        // Closing all tabs enters the tab switcher. Exit it.
        ChromeTabbedActivity cta = (ChromeTabbedActivity) sActivityTestRule.getActivity();
        assertTrue(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.onBackPressed());
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);

        List<Tab> finalTabs = getCurrentTabs();
        assertEquals(5, finalTabs.size());
        assertEquals(tabs, finalTabs);
    }

    private void assertOrderValid(boolean expectedState) {
        boolean isOrderValid = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return mTabGroupModelFilter.isOrderValid(); });
        assertEquals(expectedState, isOrderValid);
    }

    private void moveTab(Tab tab, int index) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabModel.moveTab(tab.getId(), index); });
    }

    /**
     * Create and add a tab at an explicit index. This bypasses {@link ChromeTabCreator} so that the
     * index used can be directly specified.
     */
    private Tab addTabAt(int index, Tab parent) {
        Tab tab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            @TabLaunchType
            int type =
                    parent != null ? TabLaunchType.FROM_TAB_GROUP_UI : TabLaunchType.FROM_CHROME_UI;
            TabCreator tabCreator =
                    sActivityTestRule.getActivity().getTabCreator(/*incognito=*/false);
            return tabCreator.createNewTab(new LoadUrlParams("about:blank"), type, parent, index);
        });
        return tab;
    }

    private List<Tab> getCurrentTabs() {
        List<Tab> tabs = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < mTabModel.getCount(); i++) {
                tabs.add(mTabModel.getTabAt(i));
            }
        });
        return tabs;
    }

    private List<Integer> getCurrentTabIds() {
        List<Integer> tabIds = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < mTabModel.getCount(); i++) {
                tabIds.add(mTabModel.getTabAt(i).getId());
            }
        });
        return tabIds;
    }
}
