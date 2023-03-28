// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
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
@EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Batch(Batch.PER_CLASS)
public class TabGroupsTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private TabModel mTabModel;
    private TabGroupModelFilter mTabGroupModelFilter;

    @Before
    public void setUp() {
        mTabModel = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        mTabGroupModelFilter = (TabGroupModelFilter) sActivityTestRule.getActivity()
                                       .getTabModelSelector()
                                       .getTabModelFilterProvider()
                                       .getTabModelFilter(false);
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
