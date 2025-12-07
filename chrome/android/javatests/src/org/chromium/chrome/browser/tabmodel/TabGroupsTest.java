// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Integration tests for the Tab Groups feature on Android. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Restriction({DeviceFormFactor.PHONE})
@Batch(Batch.PER_CLASS)
public class TabGroupsTest {
    private static final int OTHER_ROOT_ID_1 = 11;
    private static final int OTHER_ROOT_ID_2 = 22;

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelObserver mTabGroupModelFilterObserver;

    private TabModel mTabModel;
    private TabGroupModelFilter mTabGroupModelFilter;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mTabModel = mPage.getTabModel();
        mTabGroupModelFilter = mPage.getTabGroupModelFilter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.addObserver(mTabGroupModelFilterObserver);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupModelFilter.removeObserver(mTabGroupModelFilterObserver);
                });
    }

    private void prepareTabs(List<Integer> tabsPerGroup) {
        for (int tabsToCreate : tabsPerGroup) {
            List<Tab> tabs = new ArrayList<>();
            for (int i = 0; i < tabsToCreate; i++) {
                Tab tab =
                        ChromeTabUtils.fullyLoadUrlInNewTab(
                                InstrumentationRegistry.getInstrumentation(),
                                mActivityTestRule.getActivity(),
                                "about:blank",
                                /* incognito= */ false);
                tabs.add(tab);
            }
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabGroupModelFilter.mergeListOfTabsToGroup(
                                tabs,
                                tabs.get(0),
                                TabGroupModelFilter.MergeNotificationType.DONT_NOTIFY);
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
        Tab tab = addTabAt(/* index= */ 3, /* parent= */ null);
        tabs.add(4, tab);
        assertEquals(tabs, getCurrentTabs());
    }

    @Test
    @SmallTest
    public void testPreventAddingGroupedTabAwayFromGroup_BeforeGroup() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab (tab added here), 1, 2, 3
        // Tab 4
        Tab tab = addTabAt(/* index= */ 0, /* parent= */ tabs.get(1));
        tabs.add(1, tab);
        assertEquals(tabs, getCurrentTabs());
    }

    @Test
    @SmallTest
    public void testPreventAddingGroupedTabAwayFromGroup_AfterGroup() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab 1, 2, 3, (tab added here)
        // Tab 4
        int tabCount = ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.getCount());
        Tab tab = addTabAt(/* index= */ tabCount, /* parent= */ tabs.get(1));
        tabs.add(4, tab);
        assertEquals(tabs, getCurrentTabs());
    }

    @Test
    @SmallTest
    public void testAllowAddingGroupedTabInsideGroup() {
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab 1, (tab added here), 2, 3
        // Tab 4
        Tab tab = addTabAt(/* index= */ 2, /* parent= */ tabs.get(1));
        tabs.add(2, tab);
        assertEquals(tabs, getCurrentTabs());
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
        moveTab(tab0, 4);
        tabs.remove(tab0);
        tabs.add(tab0);
        Tab tab1 = tabs.get(0);
        moveTab(tab1, 2);
        tabs.remove(tab1);
        tabs.add(2, tab1);
        assertEquals(tabs, getCurrentTabs());
    }

    @Test
    @SmallTest
    public void testTabGroupModelFilterObserverUndoClosure() {
        // Four tabs plus the first blank one.
        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        final List<Tab> tabs = getCurrentTabs();
        assertEquals(5, tabs.size());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.setIndex(2, TabSelectionType.FROM_USER);
                    mTabModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeAllTabs().build(),
                                    /* allowDialog= */ false);
                });

        List<Tab> noTabs = getCurrentTabs();
        assertTrue(noTabs.isEmpty());

        // Wait to enter the tab switcher.
        ChromeTabbedActivity cta = (ChromeTabbedActivity) mActivityTestRule.getActivity();
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);

        InOrder calledInOrder = inOrder(mTabGroupModelFilterObserver);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Tab tab : tabs) {
                        mTabModel.cancelTabClosure(tab.getId());
                    }
                });
        // Ensure didSelectTab is called and the call occurs after the tab closure is actually
        // undone.
        calledInOrder.verify(mTabGroupModelFilterObserver).tabClosureUndone(eq(tabs.get(0)));
        calledInOrder
                .verify(mTabGroupModelFilterObserver)
                .didSelectTab(
                        eq(tabs.get(0)), eq(TabSelectionType.FROM_UNDO), /* lastId= */ eq(-1));
        calledInOrder.verify(mTabGroupModelFilterObserver).tabClosureUndone(eq(tabs.get(1)));
        calledInOrder.verify(mTabGroupModelFilterObserver).tabClosureUndone(eq(tabs.get(2)));
        calledInOrder.verify(mTabGroupModelFilterObserver).tabClosureUndone(eq(tabs.get(3)));
        calledInOrder.verify(mTabGroupModelFilterObserver).tabClosureUndone(eq(tabs.get(4)));

        // Exit the tab switcher.
        ThreadUtils.runOnUiThreadBlocking(() -> cta.onBackPressed());
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);

        List<Tab> finalTabs = getCurrentTabs();
        assertEquals(5, finalTabs.size());
        assertEquals(tabs, finalTabs);
    }

    @Test
    @SmallTest
    public void testTabUngrouper() {
        int tabCount = 2;
        prepareTabs(Arrays.asList(new Integer[] {tabCount}));

        // Tab 0
        // Tab group 1, 2
        List<Tab> tabs = getCurrentTabs();
        assertEquals(tabCount + 1, tabs.size());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(0)));
                    assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    assertTrue(mTabGroupModelFilter.isTabInTabGroup(tabs.get(2)));

                    mTabGroupModelFilter
                            .getTabUngrouper()
                            .ungroupTabs(
                                    tabs.subList(1, 3),
                                    /* trailing= */ true,
                                    /* allowDialog= */ false);

                    assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(1)));
                    assertFalse(mTabGroupModelFilter.isTabInTabGroup(tabs.get(2)));
                });
    }

    private void moveTab(Tab tab, int index) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.moveTab(tab.getId(), index);
                });
    }

    /**
     * Create and add a tab at an explicit index. This bypasses {@link ChromeTabCreator} so that the
     * index used can be directly specified.
     */
    private Tab addTabAt(int index, Tab parent) {
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            @TabLaunchType
                            int type =
                                    parent != null
                                            ? TabLaunchType.FROM_TAB_GROUP_UI
                                            : TabLaunchType.FROM_CHROME_UI;
                            TabCreator tabCreator =
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabCreator(/* incognito= */ false);
                            return tabCreator.createNewTab(
                                    new LoadUrlParams("about:blank"), type, parent, index);
                        });
        return tab;
    }

    private List<Tab> getCurrentTabs() {
        List<Tab> tabs = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < mTabModel.getCount(); i++) {
                        tabs.add(mTabModel.getTabAt(i));
                    }
                });
        return tabs;
    }
}
