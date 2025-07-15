// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

/** Integration test for {@link TabCollectionTabModelImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.TAB_COLLECTION_ANDROID})
public class TabCollectionTabModelImplTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private String mTestUrl;
    private WebPageStation mPage;
    private TabModelSelector mTabModelSelector;
    private TabModel mRegularModel;
    private TabCollectionTabModelImpl mCollectionModel;

    @Before
    public void setUp() throws Exception {
        mTestUrl = mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/ok.txt");
        mPage = mActivityTestRule.startOnBlankPage();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        mRegularModel = mTabModelSelector.getModel(/* incognito= */ false);
        if (mRegularModel instanceof TabCollectionTabModelImpl collectionModel) {
            mCollectionModel = collectionModel;
        }
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.runOnTabStateInitialized(
                            mTabModelSelector,
                            (unused) -> {
                                helper.notifyCalled();
                            });
                });
        helper.waitForOnly();
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testInitialState() {
        assertTrue(mCollectionModel.isActiveModel());
        assertTrue(mCollectionModel.isInitializationComplete());
        assertTrue(mCollectionModel.isTabModelRestored());

        assertEquals(1, mCollectionModel.getCount());
        assertEquals(0, mCollectionModel.index());

        Tab currentTab = mCollectionModel.getCurrentTabSupplier().get();
        assertNotNull(currentTab);
        assertEquals(currentTab, mCollectionModel.getTabAt(0));
        assertEquals(0, mCollectionModel.indexOf(currentTab));
    }

    @Test
    @MediumTest
    public void testMoveTabCompatTest() {
        moveTabCompatTest();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TAB_COLLECTION_ANDROID})
    public void testMoveTabCompatTest_Legacy() {
        moveTabCompatTest();
    }

    private void moveTabCompatTest() {
        Tab tab0 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mRegularModel.getCurrentTabSupplier().get());
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        moveTab(tab0, 0);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        moveTab(tab0, 1);
        assertTabsInOrderAre(List.of(tab1, tab0, tab2));

        moveTab(tab0, 2);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, 3);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, 2);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, 1);
        assertTabsInOrderAre(List.of(tab1, tab0, tab2));

        moveTab(tab0, 0);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        moveTab(tab0, 2);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, -1);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
    }

    @Test
    @MediumTest
    public void testRemoveTab_LastTab() throws Exception {
        assertEquals(1, getCount());
        Tab tab0 = getCurrentTab();

        CallbackHelper onTabRemovedHelper = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void tabRemoved(Tab tab) {
                        assertEquals("Incorrect tab removed.", tab0, tab);
                        onTabRemovedHelper.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        fail("didSelectTab should not be called. " + tab.getId());
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.removeTab(tab0);
                });

        onTabRemovedHelper.waitForOnly();

        assertEquals("Tab count should be 0.", 0, getCount());
        assertNull("Current tab should be null.", getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab0.destroy();
                    mCollectionModel.removeObserver(observer);
                });
    }

    @Test
    @MediumTest
    public void testRemoveTab_NotSelected() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.setIndex(0, TabSelectionType.FROM_USER));
        assertEquals(tab0, getCurrentTab());

        CallbackHelper onTabRemovedHelper = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void tabRemoved(Tab tab) {
                        assertEquals("Incorrect tab removed.", tab1, tab);
                        onTabRemovedHelper.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        // Should not be called as the selected tab is not removed.
                        fail("didSelectTab should not be called.");
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.removeTab(tab1);
                });

        onTabRemovedHelper.waitForOnly();

        assertEquals("Tab count is wrong.", 2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab2));
        assertEquals("Selected tab should not change.", tab0, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab1.destroy();
                    mCollectionModel.removeObserver(observer);
                });
    }

    @Test
    @MediumTest
    public void testRemoveTab_SelectsNext() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
        assertEquals(tab2, getCurrentTab());

        CallbackHelper onTabRemovedHelper = new CallbackHelper();
        CallbackHelper didSelectTabHelper = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void tabRemoved(Tab tab) {
                        assertEquals("Incorrect tab removed.", tab2, tab);
                        onTabRemovedHelper.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        assertEquals("Incorrect tab selected.", tab1, tab);
                        assertEquals(
                                "Incorrect selection type.", TabSelectionType.FROM_CLOSE, type);
                        assertEquals("Incorrect last id.", tab2.getId(), lastId);
                        didSelectTabHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.removeTab(tab2);
                });

        didSelectTabHelper.waitForOnly();
        onTabRemovedHelper.waitForOnly();

        assertEquals("Tab count is wrong.", 2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab1));
        assertEquals("Incorrect tab is selected after removal.", tab1, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab2.destroy();
                    mCollectionModel.removeObserver(observer);
                });
    }

    @Test
    @MediumTest
    public void testCloseTab_Single() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.setIndex(1, TabSelectionType.FROM_USER));
        assertEquals(tab1, getCurrentTab());

        CallbackHelper willCloseTabHelper = new CallbackHelper();
        CallbackHelper onFinishingMultipleTabClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingTabClosureHelper = new CallbackHelper();
        CallbackHelper didSelectTabHelper = new CallbackHelper();

        AtomicReference<Tab> tabInWillClose = new AtomicReference<>();
        AtomicReference<Boolean> isSingleInWillClose = new AtomicReference<>();
        AtomicReference<List<Tab>> tabsInFinishingMultiple = new AtomicReference<>();
        AtomicReference<Tab> tabInFinishing = new AtomicReference<>();
        AtomicReference<Tab> tabInDidSelect = new AtomicReference<>();

        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void willCloseTab(Tab tab, boolean isSingle) {
                        tabInWillClose.set(tab);
                        isSingleInWillClose.set(isSingle);
                        willCloseTabHelper.notifyCalled();
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(
                            List<Tab> tabs, boolean saveToTabRestoreService) {
                        tabsInFinishingMultiple.set(tabs);
                        onFinishingMultipleTabClosureHelper.notifyCalled();
                    }

                    @Override
                    public void onFinishingTabClosure(Tab tab, @TabClosingSource int source) {
                        tabInFinishing.set(tab);
                        onFinishingTabClosureHelper.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        tabInDidSelect.set(tab);
                        assertEquals(TabSelectionType.FROM_CLOSE, type);
                        assertEquals(tab1.getId(), lastId);
                        didSelectTabHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.closeTabs(TabClosureParams.closeTab(tab1).build());
                });

        willCloseTabHelper.waitForOnly();
        onFinishingMultipleTabClosureHelper.waitForOnly();
        onFinishingTabClosureHelper.waitForOnly();
        didSelectTabHelper.waitForOnly();

        assertEquals("Incorrect tab in willCloseTab.", tab1, tabInWillClose.get());
        assertTrue("isSingle should be true.", isSingleInWillClose.get());
        assertEquals(
                "Incorrect tabs in onFinishingMultipleTabClosure.",
                List.of(tab1),
                tabsInFinishingMultiple.get());
        assertEquals("Incorrect tab in onFinishingTabClosure.", tab1, tabInFinishing.get());
        assertEquals("Incorrect tab selected.", tab0, tabInDidSelect.get());

        assertEquals("Tab count is wrong.", 2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab2));
        assertEquals("Incorrect tab is selected after removal.", tab0, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    public void testCloseTabs_Multiple() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.setIndex(2, TabSelectionType.FROM_USER));
        assertEquals(tab2, getCurrentTab());

        CallbackHelper willCloseMultipleTabsHelper = new CallbackHelper();
        CallbackHelper willCloseTabHelper = new CallbackHelper();
        CallbackHelper onFinishingMultipleTabClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingTabClosureHelper = new CallbackHelper();
        CallbackHelper didSelectTabHelper = new CallbackHelper();

        List<Tab> tabsToClose = Arrays.asList(tab1, tab2);
        AtomicReference<List<Tab>> tabsInWillCloseMultiple = new AtomicReference<>();
        List<Tab> tabsInWillCloseTab = Collections.synchronizedList(new ArrayList<>());
        AtomicReference<List<Tab>> tabsInFinishingMultiple = new AtomicReference<>();
        List<Tab> tabsInFinishing = Collections.synchronizedList(new ArrayList<>());
        AtomicReference<Tab> tabInDidSelect = new AtomicReference<>();

        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void willCloseMultipleTabs(boolean allowUndo, List<Tab> tabs) {
                        tabsInWillCloseMultiple.set(tabs);
                        willCloseMultipleTabsHelper.notifyCalled();
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean isSingle) {
                        tabsInWillCloseTab.add(tab);
                        assertFalse("isSingle should be false.", isSingle);
                        willCloseTabHelper.notifyCalled();
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(
                            List<Tab> tabs, boolean saveToTabRestoreService) {
                        tabsInFinishingMultiple.set(tabs);
                        onFinishingMultipleTabClosureHelper.notifyCalled();
                    }

                    @Override
                    public void onFinishingTabClosure(Tab tab, @TabClosingSource int source) {
                        tabsInFinishing.add(tab);
                        onFinishingTabClosureHelper.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        tabInDidSelect.set(tab);
                        assertEquals(TabSelectionType.FROM_CLOSE, type);
                        assertEquals(tab2.getId(), lastId);
                        didSelectTabHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.closeTabs(TabClosureParams.closeTabs(tabsToClose).build());
                });

        willCloseMultipleTabsHelper.waitForOnly();
        willCloseTabHelper.waitForCallback(0, 2);
        onFinishingMultipleTabClosureHelper.waitForOnly();
        onFinishingTabClosureHelper.waitForCallback(0, 2);
        didSelectTabHelper.waitForOnly();

        assertEquals(
                "Incorrect tabs in willCloseMultipleTabs.",
                tabsToClose,
                tabsInWillCloseMultiple.get());
        assertEquals("Incorrect number of willCloseTab calls.", 2, tabsInWillCloseTab.size());
        assertTrue("Incorrect tabs in willCloseTab.", tabsInWillCloseTab.containsAll(tabsToClose));
        assertEquals(
                "Incorrect tabs in onFinishingMultipleTabClosure.",
                tabsToClose,
                tabsInFinishingMultiple.get());
        assertEquals("Incorrect number of onFinishingTabClosure calls.", 2, tabsInFinishing.size());
        assertTrue(
                "Incorrect tabs in onFinishingTabClosure.",
                tabsInFinishing.containsAll(tabsToClose));
        assertEquals("Incorrect tab selected.", tab0, tabInDidSelect.get());

        assertEquals("Tab count is wrong.", 2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab3));
        assertEquals("Incorrect tab is selected after removal.", tab0, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    public void testCloseTabs_All() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        List<Tab> allTabs = List.of(tab0, tab1, tab2);
        assertTabsInOrderAre(allTabs);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.setIndex(1, TabSelectionType.FROM_USER));
        assertEquals(tab1, getCurrentTab());

        CallbackHelper willCloseAllTabsHelper = new CallbackHelper();
        CallbackHelper willCloseTabHelper = new CallbackHelper();
        CallbackHelper onFinishingMultipleTabClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingTabClosureHelper = new CallbackHelper();

        List<Tab> tabsInWillCloseTab = Collections.synchronizedList(new ArrayList<>());
        AtomicReference<List<Tab>> tabsInFinishingMultiple = new AtomicReference<>();
        List<Tab> tabsInFinishing = Collections.synchronizedList(new ArrayList<>());

        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void willCloseAllTabs(boolean isIncognito) {
                        willCloseAllTabsHelper.notifyCalled();
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean isSingle) {
                        tabsInWillCloseTab.add(tab);
                        assertFalse("isSingle should be false.", isSingle);
                        willCloseTabHelper.notifyCalled();
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(
                            List<Tab> tabs, boolean saveToTabRestoreService) {
                        tabsInFinishingMultiple.set(tabs);
                        onFinishingMultipleTabClosureHelper.notifyCalled();
                    }

                    @Override
                    public void onFinishingTabClosure(Tab tab, @TabClosingSource int source) {
                        tabsInFinishing.add(tab);
                        onFinishingTabClosureHelper.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        fail("didSelectTab should not be called when closing all tabs.");
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.closeTabs(TabClosureParams.closeAllTabs().build());
                });

        willCloseAllTabsHelper.waitForOnly();
        willCloseTabHelper.waitForCallback(0, 3);
        onFinishingMultipleTabClosureHelper.waitForOnly();
        onFinishingTabClosureHelper.waitForCallback(0, 3);

        assertEquals("Incorrect number of willCloseTab calls.", 3, tabsInWillCloseTab.size());
        assertTrue("Incorrect tabs in willCloseTab.", tabsInWillCloseTab.containsAll(allTabs));
        assertEquals(
                "Incorrect tabs in onFinishingMultipleTabClosure.",
                allTabs,
                tabsInFinishingMultiple.get());
        assertEquals("Incorrect number of onFinishingTabClosure calls.", 3, tabsInFinishing.size());
        assertTrue(
                "Incorrect tabs in onFinishingTabClosure.", tabsInFinishing.containsAll(allTabs));

        assertEquals("Tab count should be 0.", 0, getCount());
        assertNull("Current tab should be null.", getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @SmallTest
    public void testGetIterator() {
        Tab tab = getTabAt(0);
        List<Tab> allTabs = List.of(tab);
        assertTabsInOrderAre(allTabs);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Iterator<Tab> iterator = mCollectionModel.iterator();
                    assertTrue(iterator.hasNext());
                    assertEquals(iterator.next(), tab);
                    assertFalse(iterator.hasNext());
                });
    }

    @Test
    @SmallTest
    public void testGetIterator_multipleTabs() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<Tab> allTabs = List.of(tab0, tab1, tab2);
                    assertTabsInOrderAre(allTabs);

                    Iterator<Tab> iterator = mCollectionModel.iterator();
                    assertTrue(iterator.hasNext());
                    assertEquals(iterator.next(), tab0);
                    assertTrue(iterator.hasNext());
                    assertEquals(iterator.next(), tab1);
                    assertTrue(iterator.hasNext());
                    assertEquals(iterator.next(), tab2);
                    assertFalse(iterator.hasNext());
                });
    }

    @Test
    @SmallTest
    public void testCreateSingleTabGroup() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        List<Tab> tabs = List.of(tab0, tab1);
        assertTabsInOrderAre(tabs);

        CallbackHelper willMergeTabToGroupHelper = new CallbackHelper();
        CallbackHelper didMergeTabToGroupHelper = new CallbackHelper();
        CallbackHelper didCreateNewGroupHelper = new CallbackHelper();

        TabGroupModelFilterObserver observer =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMergeTabToGroup(Tab movedTab, int newRootId, Token tabGroupId) {
                        assertEquals(tab0, movedTab);
                        assertNotNull(tabGroupId);
                        willMergeTabToGroupHelper.notifyCalled();
                    }

                    @Override
                    public void didMergeTabToGroup(Tab movedTab) {
                        assertEquals(tab0, movedTab);
                        didMergeTabToGroupHelper.notifyCalled();
                    }

                    @Override
                    public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                        assertEquals(tab0, destinationTab);
                        assertEquals(mCollectionModel, filter);
                        didCreateNewGroupHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(observer);
                    mCollectionModel.createSingleTabGroup(tab0);
                    assertNotNull(tab0.getTabGroupId());
                    List<Tab> tabsInGroup = mCollectionModel.getTabsInGroup(tab0.getTabGroupId());
                    assertEquals(1, tabsInGroup.size());
                    assertEquals(tab0, tabsInGroup.get(0));
                    mCollectionModel.removeTabGroupObserver(observer);
                });

        willMergeTabToGroupHelper.waitForOnly();
        didMergeTabToGroupHelper.waitForOnly();
        didCreateNewGroupHelper.waitForOnly();

        assertTabsInOrderAre(tabs);
    }

    @Test
    @SmallTest
    public void testGetAllTabGroupIdsAndCount() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();

        // TODO(crbug.com/429145597): Remove this once the implementation is further along.
        // Create a tab that is not in a group to act as the current tab. This is required to
        // prevent TabListMediator from being created and failing a bunch of lookups for
        // representative tabs that are not yet implemented.
        createTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNotNull(tab0);
                    assertNotNull(tab1);

                    assertTrue(
                            "Initially, getAllTabGroupIds should be empty.",
                            mCollectionModel.getAllTabGroupIds().isEmpty());
                    assertEquals(
                            "Initially, getTabGroupCount should be 0.",
                            0,
                            mCollectionModel.getTabGroupCount());

                    mCollectionModel.createSingleTabGroup(tab0);
                    Token groupId0 = tab0.getTabGroupId();
                    assertNotNull(groupId0);

                    Set<Token> groupIds = mCollectionModel.getAllTabGroupIds();
                    assertEquals("Should be 1 group.", 1, groupIds.size());
                    assertTrue("Set should contain group 0 id.", groupIds.contains(groupId0));
                    assertEquals(
                            "getTabGroupCount should be 1.",
                            1,
                            mCollectionModel.getTabGroupCount());

                    mCollectionModel.createSingleTabGroup(tab1);
                    Token groupId1 = tab1.getTabGroupId();
                    assertNotNull(groupId1);

                    groupIds = mCollectionModel.getAllTabGroupIds();
                    assertEquals("Should be 2 groups.", 2, groupIds.size());
                    assertTrue("Set should contain group 0 id.", groupIds.contains(groupId0));
                    assertTrue("Set should contain group 1 id.", groupIds.contains(groupId1));
                    assertEquals(
                            "getTabGroupCount should be 2.",
                            2,
                            mCollectionModel.getTabGroupCount());

                    // Group 0 should be removed as it's now empty.
                    mCollectionModel.moveTabOutOfGroupInDirection(
                            tab0.getId(), /* trailing= */ false);
                    assertNull(tab0.getTabGroupId());

                    groupIds = mCollectionModel.getAllTabGroupIds();
                    assertEquals("Should be 1 group left.", 1, groupIds.size());
                    assertFalse("Set should not contain group 0 id.", groupIds.contains(groupId0));
                    assertTrue("Set should still contain group 1 id.", groupIds.contains(groupId1));
                    assertEquals(
                            "getTabGroupCount should be 1.",
                            1,
                            mCollectionModel.getTabGroupCount());

                    // Group 1 should be removed as it's now also empty.
                    mCollectionModel.moveTabOutOfGroupInDirection(
                            tab1.getId(), /* trailing= */ false);
                    assertNull(tab1.getTabGroupId());

                    assertTrue(
                            "getAllTabGroupIds should be empty again.",
                            mCollectionModel.getAllTabGroupIds().isEmpty());
                    assertEquals(
                            "getTabGroupCount should be 0 again.",
                            0,
                            mCollectionModel.getTabGroupCount());
                });
    }

    @Test
    @SmallTest
    public void testMoveTabOutOfGroupLastTab_Trailing() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.createSingleTabGroup(tab1));
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        Token tab1GroupId = tab1.getTabGroupId();
        assertNotNull(tab1GroupId);

        CallbackHelper willMoveTabOutOfGroupHelper = new CallbackHelper();
        CallbackHelper didMoveTabOutOfGroupHelper = new CallbackHelper();
        CallbackHelper didRemoveTabGroupHelper = new CallbackHelper();

        TabGroupModelFilterObserver observer =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMoveTabOutOfGroup(Tab movedTab, Token destinationTabGroupId) {
                        assertEquals(tab1, movedTab);
                        assertNull(destinationTabGroupId);
                        willMoveTabOutOfGroupHelper.notifyCalled();
                    }

                    @Override
                    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                        assertEquals(tab1, movedTab);
                        assertEquals(1, prevFilterIndex);
                        didMoveTabOutOfGroupHelper.notifyCalled();
                    }

                    @Override
                    public void didRemoveTabGroup(
                            int tabId, Token tabGroupId, @DidRemoveTabGroupReason int reason) {
                        assertEquals(tab1GroupId, tabGroupId);
                        assertEquals(DidRemoveTabGroupReason.UNGROUP, reason);
                        didRemoveTabGroupHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(observer);
                    mCollectionModel.moveTabOutOfGroupInDirection(
                            tab1.getId(), /* trailing= */ true);
                    assertNull(tab1.getTabGroupId());
                    mCollectionModel.removeTabGroupObserver(observer);
                });

        willMoveTabOutOfGroupHelper.waitForOnly();
        didMoveTabOutOfGroupHelper.waitForOnly();
        didRemoveTabGroupHelper.waitForOnly();

        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTabGroupVisualData() throws Exception {
        Tab tab0 = getTabAt(0);
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.createSingleTabGroup(tab0));
        Token tabGroupId = tab0.getTabGroupId();
        assertNotNull(tabGroupId);

        final String testTitle = "Test Title";
        CallbackHelper titleChangedHelper = new CallbackHelper();
        TabGroupModelFilterObserver titleObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didChangeTabGroupTitle(Token id, String newTitle) {
                        assertEquals(tabGroupId, id);
                        assertEquals(testTitle, newTitle);
                        titleChangedHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(titleObserver);
                    mCollectionModel.setTabGroupTitle(tabGroupId, testTitle);
                    assertEquals(testTitle, mCollectionModel.getTabGroupTitle(tabGroupId));
                    assertEquals(testTitle, mCollectionModel.getTabGroupTitle(tab0));
                    mCollectionModel.removeTabGroupObserver(titleObserver);
                });
        titleChangedHelper.waitForOnly("setTabGroupTitle failed");

        CallbackHelper titleDeletedHelper = new CallbackHelper();
        TabGroupModelFilterObserver titleDeleteObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didChangeTabGroupTitle(Token id, String newTitle) {
                        assertEquals(tabGroupId, id);
                        assertEquals("", newTitle);
                        titleDeletedHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(titleDeleteObserver);
                    mCollectionModel.deleteTabGroupTitle(tabGroupId);
                    assertEquals("", mCollectionModel.getTabGroupTitle(tabGroupId));
                    mCollectionModel.removeTabGroupObserver(titleDeleteObserver);
                });
        titleDeletedHelper.waitForOnly("deleteTabGroupTitle failed");

        final int testColor = TabGroupColorId.BLUE;
        CallbackHelper colorChangedHelper = new CallbackHelper();
        TabGroupModelFilterObserver colorObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didChangeTabGroupColor(Token id, int newColor) {
                        assertEquals(tabGroupId, id);
                        assertEquals(testColor, newColor);
                        colorChangedHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(colorObserver);
                    mCollectionModel.setTabGroupColor(tabGroupId, testColor);
                    assertEquals(testColor, mCollectionModel.getTabGroupColor(tabGroupId));
                    assertEquals(
                            testColor, mCollectionModel.getTabGroupColorWithFallback(tabGroupId));
                    assertEquals(testColor, mCollectionModel.getTabGroupColorWithFallback(tab0));
                    mCollectionModel.removeTabGroupObserver(colorObserver);
                });
        colorChangedHelper.waitForOnly("setTabGroupColor failed");

        CallbackHelper colorDeletedHelper = new CallbackHelper();
        TabGroupModelFilterObserver colorDeleteObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didChangeTabGroupColor(Token id, int newColor) {
                        assertEquals(tabGroupId, id);
                        assertEquals(TabGroupColorId.GREY, newColor);
                        colorDeletedHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(colorDeleteObserver);
                    mCollectionModel.deleteTabGroupColor(tabGroupId);
                    assertEquals(
                            TabGroupColorId.GREY,
                            mCollectionModel.getTabGroupColorWithFallback(tabGroupId));
                    mCollectionModel.removeTabGroupObserver(colorDeleteObserver);
                });
        colorDeletedHelper.waitForOnly("deleteTabGroupColor failed");

        CallbackHelper collapsedChangedHelper = new CallbackHelper();
        TabGroupModelFilterObserver collapsedObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didChangeTabGroupCollapsed(
                            Token id, boolean isCollapsed, boolean animate) {
                        assertEquals(tabGroupId, id);
                        assertTrue(isCollapsed);
                        assertFalse(animate);
                        collapsedChangedHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(collapsedObserver);
                    mCollectionModel.setTabGroupCollapsed(tabGroupId, true, false);
                    assertTrue(mCollectionModel.getTabGroupCollapsed(tabGroupId));
                    mCollectionModel.removeTabGroupObserver(collapsedObserver);
                });
        collapsedChangedHelper.waitForOnly("setTabGroupCollapsed true failed");

        CallbackHelper collapsedDeletedHelper = new CallbackHelper();
        TabGroupModelFilterObserver collapsedDeleteObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didChangeTabGroupCollapsed(
                            Token id, boolean isCollapsed, boolean animate) {
                        assertEquals(tabGroupId, id);
                        assertFalse(isCollapsed);
                        assertFalse(animate);
                        collapsedDeletedHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(collapsedDeleteObserver);
                    mCollectionModel.deleteTabGroupCollapsed(tabGroupId);
                    assertFalse(mCollectionModel.getTabGroupCollapsed(tabGroupId));
                    mCollectionModel.removeTabGroupObserver(collapsedDeleteObserver);
                });
        collapsedDeletedHelper.waitForOnly("deleteTabGroupCollapsed failed");

        CallbackHelper deleteAllTitleHelper = new CallbackHelper();
        CallbackHelper deleteAllColorHelper = new CallbackHelper();
        CallbackHelper deleteAllCollapsedHelper = new CallbackHelper();

        TabGroupModelFilterObserver deleteAllObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didChangeTabGroupTitle(Token id, String newTitle) {
                        assertEquals(tabGroupId, id);
                        assertEquals("", newTitle);
                        deleteAllTitleHelper.notifyCalled();
                    }

                    @Override
                    public void didChangeTabGroupColor(Token id, int newColor) {
                        assertEquals(tabGroupId, id);
                        assertEquals(TabGroupColorId.GREY, newColor);
                        deleteAllColorHelper.notifyCalled();
                    }

                    @Override
                    public void didChangeTabGroupCollapsed(
                            Token id, boolean isCollapsed, boolean animate) {
                        assertEquals(tabGroupId, id);
                        assertFalse(isCollapsed);
                        assertFalse(animate);
                        deleteAllCollapsedHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(deleteAllObserver);
                    mCollectionModel.deleteTabGroupVisualData(tabGroupId);
                    assertEquals("", mCollectionModel.getTabGroupTitle(tabGroupId));
                    assertEquals(
                            TabGroupColorId.GREY,
                            mCollectionModel.getTabGroupColorWithFallback(tabGroupId));
                    assertFalse(mCollectionModel.getTabGroupCollapsed(tabGroupId));
                });
        deleteAllTitleHelper.waitForOnly("deleteTabGroupTitle failed");
        deleteAllColorHelper.waitForOnly("deleteTabGroupColor failed");
        deleteAllCollapsedHelper.waitForOnly("deleteTabGroupCollapsed failed");
    }

    private void assertTabsInOrderAre(List<Tab> tabs) {
        assertEquals(
                "Mismatched tab count",
                (long) tabs.size(),
                (long) ThreadUtils.runOnUiThreadBlocking(mRegularModel::getCount));
        for (int i = 0; i < tabs.size(); i++) {
            Tab expected = tabs.get(i);
            Tab actual = getTabAt(i);
            assertEquals(
                    "Mismatched tabs at "
                            + i
                            + " expected, "
                            + expected.getId()
                            + " was, "
                            + actual.getId(),
                    expected,
                    actual);
        }
    }

    private int getCount() {
        return ThreadUtils.runOnUiThreadBlocking(mCollectionModel::getCount);
    }

    private Tab getCurrentTab() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.getCurrentTabSupplier().get());
    }

    private Tab getTabAt(int index) {
        return ThreadUtils.runOnUiThreadBlocking(() -> mRegularModel.getTabAt(index));
    }

    private void moveTab(Tab tab, int index) {
        ThreadUtils.runOnUiThreadBlocking(() -> mRegularModel.moveTab(tab.getId(), index));
    }

    private Tab createTab() {
        return mActivityTestRule.loadUrlInNewTab(mTestUrl, /* incognito= */ false);
    }
}
