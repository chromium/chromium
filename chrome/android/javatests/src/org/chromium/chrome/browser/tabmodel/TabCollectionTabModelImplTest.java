// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;

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
        // Methods that would normally be triggered by snackbar lifecycle are manually invoked in
        // this test.
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();

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
    public void testPinAndUnpinTab() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
        assertFalse(tab0.getIsPinned());
        assertFalse(tab1.getIsPinned());
        assertFalse(tab2.getIsPinned());

        verifyPinOrUnpin(tab1, /* isPinned= */ true, /* willMove= */ true);
        assertTabsInOrderAre(List.of(tab1, tab0, tab2));

        verifyPinOrUnpin(tab2, /* isPinned= */ true, /* willMove= */ true);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        // Ensure pinned tabs stay in the first two indices and unpinned tabs remain outside the
        // pinned range even if the index would cross over the pinned range boundary.
        moveTab(tab1, 10);
        assertTabsInOrderAre(List.of(tab2, tab1, tab0));

        moveTab(tab1, 0);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        moveTab(tab0, 0);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0));

        verifyPinOrUnpin(tab1, /* isPinned= */ false, /* willMove= */ true);
        assertTabsInOrderAre(List.of(tab2, tab1, tab0));

        verifyPinOrUnpin(tab2, /* isPinned= */ false, /* willMove= */ false);
        assertTabsInOrderAre(List.of(tab2, tab1, tab0));
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
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
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
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
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
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
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
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
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
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
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
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
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
    @MediumTest
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
    @MediumTest
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
    @MediumTest
    public void testAddTab_GroupedWithParent() {
        Tab parentTab = getTabAt(0);
        assertNull(parentTab.getTabGroupId());
        Tab childTab = createChildTab(parentTab);
        assertNotNull(parentTab.getTabGroupId());
        assertEquals(parentTab.getTabGroupId(), childTab.getTabGroupId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<Tab> tabsInGroup =
                            mCollectionModel.getTabsInGroup(parentTab.getTabGroupId());
                    assertEquals(2, tabsInGroup.size());
                    assertTrue(tabsInGroup.contains(parentTab));
                    assertTrue(tabsInGroup.contains(childTab));
                    assertTabsInOrderAre(List.of(parentTab, childTab));
                });
    }

    @Test
    @MediumTest
    public void testAddTab_GroupedWithParent_ParentAlreadyInGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        mergeListOfTabsToGroup(List.of(tab0, tab1), tab0);
        Token groupId = tab0.getTabGroupId();
        assertNotNull(groupId);
        Tab childTab = createChildTab(tab0);
        assertEquals(groupId, childTab.getTabGroupId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<Tab> tabsInGroup = mCollectionModel.getTabsInGroup(groupId);
                    assertEquals(3, tabsInGroup.size());
                    assertTrue(tabsInGroup.contains(childTab));
                    assertTabsInOrderAre(List.of(tab0, childTab, tab1));
                });
    }

    @Test
    @MediumTest
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
                    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
                        assertEquals(tab0, movedTab);
                        assertTrue(isDestinationTab);
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
    @MediumTest
    public void testGetAllTabGroupIdsAndCount() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();

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
    @MediumTest
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
    @MediumTest
    public void testMoveRelatedTabs_BasicObserver() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        mergeListOfTabsToGroup(List.of(tab1, tab2), tab1);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));
        Token groupId = tab1.getTabGroupId();
        assertNotNull(groupId);

        CallbackHelper willMoveTabGroupHelper = new CallbackHelper();
        CallbackHelper didMoveTabGroupHelper = new CallbackHelper();

        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMoveTabGroup(Token tabGroupId, int currentIndex) {
                        assertEquals(1, currentIndex);
                        assertEquals(groupId, tabGroupId);
                        willMoveTabGroupHelper.notifyCalled();
                    }

                    @Override
                    public void didMoveTabGroup(Tab movedTab, int oldIndex, int newIndex) {
                        // movedTab is the last tab in the group.
                        assertEquals(tab2, movedTab);
                        assertEquals(2, oldIndex);
                        assertEquals(3, newIndex);
                        didMoveTabGroupHelper.notifyCalled();
                    }
                };
        TabModelObserver modelObserver =
                new TabModelObserver() {
                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int oldIndex) {
                        assertTrue(tab == tab1 || tab == tab2);
                        if (tab == tab1) {
                            assertEquals(2, newIndex);
                            assertEquals(1, oldIndex);
                        } else { // tab2
                            assertEquals(3, newIndex);
                            assertEquals(2, oldIndex);
                        }
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mCollectionModel.addObserver(modelObserver);
                    // Move group to the end.
                    mCollectionModel.moveRelatedTabs(tab1.getId(), 4);
                    mCollectionModel.removeTabGroupObserver(groupObserver);
                    mCollectionModel.removeObserver(modelObserver);
                });

        willMoveTabGroupHelper.waitForOnly();
        didMoveTabGroupHelper.waitForOnly();

        assertTabsInOrderAre(List.of(tab0, tab3, tab1, tab2));
    }

    @Test
    @MediumTest
    public void testMoveRelatedTabs_Advanced() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        Tab tab4 = createTab();
        Tab tab5 = createTab();
        Tab tab6 = createTab();
        Tab tab7 = createTab();
        mergeListOfTabsToGroup(List.of(tab1, tab2), tab1);
        mergeListOfTabsToGroup(List.of(tab4, tab5, tab6), tab4);

        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5, tab6, tab7));
        assertNull(tab0.getTabGroupId());
        Token groupId1 = tab1.getTabGroupId();
        assertNotNull(groupId1);
        assertEquals(groupId1, tab2.getTabGroupId());
        assertNull(tab3.getTabGroupId());
        Token groupId2 = tab4.getTabGroupId();
        assertNotNull(groupId2);
        assertNotEquals(groupId1, groupId2);
        assertEquals(groupId2, tab5.getTabGroupId());
        assertEquals(groupId2, tab6.getTabGroupId());
        assertNull(tab7.getTabGroupId());

        moveRelatedTabs(tab1, 0);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0, tab3, tab4, tab5, tab6, tab7));

        // Moving to an index inside the group does not result in change.
        moveRelatedTabs(tab1, 1);
        assertTabsInOrderAre(List.of(tab1, tab2, tab0, tab3, tab4, tab5, tab6, tab7));

        moveRelatedTabs(tab1, 2);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5, tab6, tab7));

        // Moving to an index inside the group does not result in change.
        moveRelatedTabs(tab1, 1);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5, tab6, tab7));
        moveRelatedTabs(tab1, 2);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5, tab6, tab7));

        moveRelatedTabs(tab1, 3);
        assertTabsInOrderAre(List.of(tab0, tab3, tab1, tab2, tab4, tab5, tab6, tab7));

        moveRelatedTabs(tab1, 4);
        assertTabsInOrderAre(List.of(tab0, tab3, tab1, tab2, tab4, tab5, tab6, tab7));

        moveRelatedTabs(tab1, 5);
        assertTabsInOrderAre(List.of(tab0, tab3, tab1, tab2, tab4, tab5, tab6, tab7));

        moveRelatedTabs(tab1, 6);
        assertTabsInOrderAre(List.of(tab0, tab3, tab4, tab5, tab6, tab1, tab2, tab7));

        moveRelatedTabs(tab1, 1);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5, tab6, tab7));

        moveRelatedTabs(tab1, 50);
        assertTabsInOrderAre(List.of(tab0, tab3, tab4, tab5, tab6, tab7, tab1, tab2));
    }

    @Test
    @MediumTest
    public void testMoveRelatedTabs_IndividualTab() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        CallbackHelper didMoveTabGroupHelper = new CallbackHelper();
        CallbackHelper didMoveTabHelper = new CallbackHelper();

        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMoveTabGroup(Token tabGroupId, int currentIndex) {
                        fail("willMoveTabGroup should not be called for individual tab.");
                    }

                    @Override
                    public void didMoveTabGroup(Tab movedTab, int oldIndex, int newIndex) {
                        assertEquals(tab1, movedTab);
                        assertEquals(2, newIndex);
                        assertEquals(1, oldIndex);
                        didMoveTabGroupHelper.notifyCalled();
                    }
                };
        TabModelObserver modelObserver =
                new TabModelObserver() {
                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int oldIndex) {
                        assertEquals(tab1, tab);
                        assertEquals(2, newIndex);
                        assertEquals(1, oldIndex);
                        didMoveTabHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mCollectionModel.addObserver(modelObserver);
                    // Move tab1 to the end.
                    mCollectionModel.moveRelatedTabs(tab1.getId(), 3);
                    mCollectionModel.removeTabGroupObserver(groupObserver);
                    mCollectionModel.removeObserver(modelObserver);
                });

        didMoveTabGroupHelper.waitForOnly();
        didMoveTabHelper.waitForOnly();

        assertTabsInOrderAre(List.of(tab0, tab2, tab1));
    }

    @Test
    @MediumTest
    public void testMoveTab_InGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));

        mergeListOfTabsToGroup(List.of(tab1, tab2), tab1);
        Token groupId = tab1.getTabGroupId();
        assertNotNull(groupId);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));

        // Move tab1 within the group.
        moveTab(tab1, 2);
        assertTabsInOrderAre(List.of(tab0, tab2, tab1, tab3));
        assertEquals(groupId, tab1.getTabGroupId());
        assertEquals(groupId, tab2.getTabGroupId());

        // Try to move tab1 outside the group to the beginning. It should be constrained to the
        // start of the group.
        moveTab(tab1, 0);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));
        assertEquals(groupId, tab1.getTabGroupId());
        assertEquals(groupId, tab2.getTabGroupId());

        // Try to move tab1 outside the group to the end. It should be constrained to the end of
        // the group.
        moveTab(tab1, 4);
        assertTabsInOrderAre(List.of(tab0, tab2, tab1, tab3));
        assertEquals(groupId, tab1.getTabGroupId());
        assertEquals(groupId, tab2.getTabGroupId());

        // Try to move tabs into the group. They should get pushed to the nearest edge.
        moveTab(tab0, 1);
        assertTabsInOrderAre(List.of(tab0, tab2, tab1, tab3));

        moveTab(tab0, 2);
        assertTabsInOrderAre(List.of(tab2, tab1, tab0, tab3));

        moveTab(tab3, 0);
        assertTabsInOrderAre(List.of(tab3, tab2, tab1, tab0));

        // Merge tabs into the group. They should get put at the end.
        mergeListOfTabsToGroup(List.of(tab3), tab1);
        assertTabsInOrderAre(List.of(tab2, tab1, tab3, tab0));
        assertEquals(groupId, tab3.getTabGroupId());

        mergeListOfTabsToGroup(List.of(tab0), tab1);
        assertTabsInOrderAre(List.of(tab2, tab1, tab3, tab0));
        assertEquals(groupId, tab0.getTabGroupId());
    }

    @Test
    @MediumTest
    public void testMoveTabOutOfGroup_FromMultiTabGroup() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        mergeListOfTabsToGroup(List.of(tab0, tab1), tab0);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
        Token groupId = tab0.getTabGroupId();
        assertNotNull(groupId);

        CallbackHelper willMoveOutOfGroup = new CallbackHelper();
        CallbackHelper didMoveOutOfGroup = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabGroupModelFilterObserver observer =
                            new TabGroupModelFilterObserver() {
                                @Override
                                public void willMoveTabOutOfGroup(
                                        Tab movedTab, Token destinationTabGroupId) {
                                    assertEquals(tab0, movedTab);
                                    assertNull(destinationTabGroupId);
                                    willMoveOutOfGroup.notifyCalled();
                                }

                                @Override
                                public void didMoveTabOutOfGroup(
                                        Tab movedTab, int prevFilterIndex) {
                                    assertEquals(tab0, movedTab);
                                    assertEquals(1, prevFilterIndex);
                                    didMoveOutOfGroup.notifyCalled();
                                }

                                @Override
                                public void didRemoveTabGroup(
                                        int tabId, Token tabGroupId, int reason) {
                                    fail("didRemoveTabGroup should not be called.");
                                }
                            };
                    mCollectionModel.addTabGroupObserver(observer);
                    mCollectionModel.moveTabOutOfGroupInDirection(
                            tab0.getId(), /* trailing= */ false);
                    mCollectionModel.removeTabGroupObserver(observer);

                    assertNull(tab0.getTabGroupId());
                    assertNotNull(tab1.getTabGroupId());
                    assertEquals(groupId, tab1.getTabGroupId());
                    assertTrue(mCollectionModel.tabGroupExists(groupId));
                    assertEquals(1, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                });
        willMoveOutOfGroup.waitForOnly();
        didMoveOutOfGroup.waitForOnly();
    }

    @Test
    @MediumTest
    public void testPinTabInGroup() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.createSingleTabGroup(tab1));
        Token groupId = tab1.getTabGroupId();
        assertNotNull(groupId);
        assertTabsInOrderAre(List.of(tab0, tab1));

        CallbackHelper willMoveOutOfGroup = new CallbackHelper();
        CallbackHelper didMoveOutOfGroup = new CallbackHelper();
        CallbackHelper didRemoveGroup = new CallbackHelper();
        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMoveTabOutOfGroup(Tab movedTab, Token destinationTabGroupId) {
                        assertEquals(tab1, movedTab);
                        assertNull(destinationTabGroupId);
                        willMoveOutOfGroup.notifyCalled();
                    }

                    @Override
                    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                        assertEquals(tab1, movedTab);
                        assertEquals(0, prevFilterIndex);
                        didMoveOutOfGroup.notifyCalled();
                    }

                    @Override
                    public void didRemoveTabGroup(
                            int tabId, Token tabGroupId, @DidRemoveTabGroupReason int reason) {
                        assertEquals(groupId, tabGroupId);
                        assertEquals(DidRemoveTabGroupReason.PIN, reason);
                        didRemoveGroup.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mRegularModel.pinTab(tab1.getId());
                    mCollectionModel.removeTabGroupObserver(groupObserver);
                });

        willMoveOutOfGroup.waitForOnly();
        didMoveOutOfGroup.waitForOnly();
        didRemoveGroup.waitForOnly();

        assertTrue(tab1.getIsPinned());
        assertNull(tab1.getTabGroupId());
        assertTabsInOrderAre(List.of(tab1, tab0));
    }

    @Test
    @MediumTest
    public void testPinTabInMultiTabGroup() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        mergeListOfTabsToGroup(List.of(tab0, tab1), tab0);
        Token groupId = tab0.getTabGroupId();
        assertNotNull(groupId);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        CallbackHelper willMoveOutOfGroup = new CallbackHelper();
        CallbackHelper didMoveOutOfGroup = new CallbackHelper();
        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMoveTabOutOfGroup(Tab movedTab, Token destinationTabGroupId) {
                        assertEquals(tab0, movedTab);
                        assertNull(destinationTabGroupId);
                        willMoveOutOfGroup.notifyCalled();
                    }

                    @Override
                    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                        assertEquals(tab0, movedTab);
                        assertEquals(1, prevFilterIndex);
                        didMoveOutOfGroup.notifyCalled();
                    }

                    @Override
                    public void didRemoveTabGroup(int tabId, Token tabGroupId, int reason) {
                        fail("didRemoveTabGroup should not be called.");
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);

                    mRegularModel.pinTab(tab0.getId());

                    mCollectionModel.removeTabGroupObserver(groupObserver);

                    assertTrue(tab0.getIsPinned());
                    assertNull(tab0.getTabGroupId());
                    assertNotNull(tab1.getTabGroupId());
                    assertEquals(groupId, tab1.getTabGroupId());
                    assertTrue(mCollectionModel.tabGroupExists(groupId));
                    assertEquals(1, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                });
        willMoveOutOfGroup.waitForOnly();
        didMoveOutOfGroup.waitForOnly();
    }

    @Test
    @MediumTest
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
                    mCollectionModel.removeTabGroupObserver(deleteAllObserver);
                });
        deleteAllTitleHelper.waitForOnly("deleteTabGroupTitle failed");
        deleteAllColorHelper.waitForOnly("deleteTabGroupColor failed");
        deleteAllCollapsedHelper.waitForOnly("deleteTabGroupCollapsed failed");
    }

    @Test
    @MediumTest
    public void testRepresentativeTabLogic() {
        // Setup: tab0, {tab1, tab3} (in group), tab2
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab1, tab3), tab1, /* notify= */ false);
                });
        assertTabsInOrderAre(List.of(tab0, tab1, tab3, tab2));
        Token tab1GroupId = tab1.getTabGroupId();
        assertNotNull(tab1GroupId);

        List<Tab> representativeTabs =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mCollectionModel.getRepresentativeTabList());
        assertEquals(3, representativeTabs.size());
        assertEquals(tab0, representativeTabs.get(0));
        assertEquals(tab1, representativeTabs.get(1));
        assertEquals(tab2, representativeTabs.get(2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mCollectionModel.getIndividualTabAndGroupCount());

                    assertEquals(tab0, mCollectionModel.getRepresentativeTabAt(0));
                    assertEquals(tab1, mCollectionModel.getRepresentativeTabAt(1));
                    assertEquals(tab2, mCollectionModel.getRepresentativeTabAt(2));
                    assertNull(mCollectionModel.getRepresentativeTabAt(3));
                    assertNull(mCollectionModel.getRepresentativeTabAt(-1));
                    assertEquals(0, mCollectionModel.representativeIndexOf(tab0));
                    assertEquals(1, mCollectionModel.representativeIndexOf(tab1));
                    assertEquals(1, mCollectionModel.representativeIndexOf(tab3));
                    assertEquals(2, mCollectionModel.representativeIndexOf(tab2));
                    assertEquals(
                            TabList.INVALID_TAB_INDEX,
                            mCollectionModel.representativeIndexOf(null));
                    mCollectionModel.setIndex(0, TabSelectionType.FROM_USER); // Select tab0
                    assertEquals(tab0, mCollectionModel.getCurrentRepresentativeTab());
                    assertEquals(0, mCollectionModel.getCurrentRepresentativeTabIndex());
                    mCollectionModel.setIndex(1, TabSelectionType.FROM_USER); // Select tab1
                    assertEquals(tab1, mCollectionModel.getCurrentRepresentativeTab());
                    assertEquals(1, mCollectionModel.getCurrentRepresentativeTabIndex());

                    mCollectionModel.setIndex(2, TabSelectionType.FROM_USER); // Select tab3
                    assertEquals(tab3, mCollectionModel.getCurrentRepresentativeTab());
                    assertEquals(1, mCollectionModel.getCurrentRepresentativeTabIndex());

                    mCollectionModel.setIndex(3, TabSelectionType.FROM_USER); // Select tab2
                    assertEquals(tab2, mCollectionModel.getCurrentRepresentativeTab());
                    assertEquals(2, mCollectionModel.getCurrentRepresentativeTabIndex());
                });
    }

    @Test
    @MediumTest
    public void testGetGroupLastShownTabId() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.createSingleTabGroup(tab0);
                    Token tab0GroupId = tab0.getTabGroupId();
                    assertNotNull(tab0GroupId);

                    assertEquals(
                            tab0.getId(), mCollectionModel.getGroupLastShownTabId(tab0GroupId));

                    mCollectionModel.setIndex(1, TabSelectionType.FROM_USER);
                    assertEquals(tab1, mCollectionModel.getCurrentTabSupplier().get());
                    assertEquals(
                            tab0.getId(), mCollectionModel.getGroupLastShownTabId(tab0GroupId));

                    assertEquals(Tab.INVALID_TAB_ID, mCollectionModel.getGroupLastShownTabId(null));
                    assertEquals(
                            Tab.INVALID_TAB_ID,
                            mCollectionModel.getGroupLastShownTabId(Token.createRandom()));
                });
    }

    @Test
    @MediumTest
    public void testWillMergingCreateNewGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // No groups exist.
                    assertTrue(mCollectionModel.willMergingCreateNewGroup(List.of(tab0, tab1)));

                    // Create a group with tab0 and tab1.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab0, /* notify= */ false);
                    Token groupId = tab0.getTabGroupId();
                    assertNotNull(groupId);

                    // Merging a list that doesn't contain the full group.
                    assertTrue(mCollectionModel.willMergingCreateNewGroup(List.of(tab0, tab2)));

                    // Merging a list that contains the full group.
                    assertFalse(
                            mCollectionModel.willMergingCreateNewGroup(List.of(tab0, tab1, tab2)));

                    // Merging just the group.
                    assertFalse(mCollectionModel.willMergingCreateNewGroup(List.of(tab0, tab1)));

                    // Merging a list that contains a different full group.
                    mCollectionModel.createSingleTabGroup(tab2);
                    assertFalse(mCollectionModel.willMergingCreateNewGroup(List.of(tab2, tab3)));
                });
    }

    @Test
    @MediumTest
    public void testGetLazyAllTabGroupIds() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        Tab tab4 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create group 1 with tab0, tab1.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab0, /* notify= */ false);
                    Token groupId1 = tab0.getTabGroupId();
                    assertNotNull(groupId1);

                    // Create group 2 with tab2, tab3.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2, tab3), tab2, /* notify= */ false);
                    Token groupId2 = tab2.getTabGroupId();
                    assertNotNull(groupId2);

                    // Case 1: No exclusions, no pending closures. (Fast path)
                    Set<Token> allGroupIds =
                            mCollectionModel
                                    .getLazyAllTabGroupIds(
                                            Collections.emptyList(),
                                            /* includePendingClosures= */ false)
                                    .get();
                    assertEquals(2, allGroupIds.size());
                    assertTrue(allGroupIds.contains(groupId1));
                    assertTrue(allGroupIds.contains(groupId2));

                    // Case 2: No exclusions, with pending closures. (Slow path, but same result)
                    allGroupIds =
                            mCollectionModel
                                    .getLazyAllTabGroupIds(
                                            Collections.emptyList(),
                                            /* includePendingClosures= */ true)
                                    .get();
                    assertEquals(2, allGroupIds.size());
                    assertTrue(allGroupIds.contains(groupId1));
                    assertTrue(allGroupIds.contains(groupId2));

                    // Case 3: Exclude one tab from a group.
                    Set<Token> groupIdsWithExclusion =
                            mCollectionModel
                                    .getLazyAllTabGroupIds(
                                            List.of(tab0), /* includePendingClosures= */ false)
                                    .get();
                    assertEquals(2, groupIdsWithExclusion.size());
                    assertTrue(groupIdsWithExclusion.contains(groupId1));
                    assertTrue(groupIdsWithExclusion.contains(groupId2));

                    // Case 4: Exclude all tabs from a group.
                    groupIdsWithExclusion =
                            mCollectionModel
                                    .getLazyAllTabGroupIds(
                                            List.of(tab0, tab1),
                                            /* includePendingClosures= */ false)
                                    .get();
                    assertEquals(1, groupIdsWithExclusion.size());
                    assertFalse(groupIdsWithExclusion.contains(groupId1));
                    assertTrue(groupIdsWithExclusion.contains(groupId2));

                    // Case 5: Exclude all tabs from a group, with pending closures.
                    groupIdsWithExclusion =
                            mCollectionModel
                                    .getLazyAllTabGroupIds(
                                            List.of(tab0, tab1), /* includePendingClosures= */ true)
                                    .get();
                    assertEquals(1, groupIdsWithExclusion.size());
                    assertFalse(groupIdsWithExclusion.contains(groupId1));
                    assertTrue(groupIdsWithExclusion.contains(groupId2));

                    // Case 6: Exclude an ungrouped tab.
                    groupIdsWithExclusion =
                            mCollectionModel
                                    .getLazyAllTabGroupIds(
                                            List.of(tab4), /* includePendingClosures= */ false)
                                    .get();
                    assertEquals(2, groupIdsWithExclusion.size());
                    assertTrue(groupIdsWithExclusion.contains(groupId1));
                    assertTrue(groupIdsWithExclusion.contains(groupId2));
                });
    }

    @Test
    @MediumTest
    public void testMergeListOfTabsToGroup_CreateNewGroup() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1));

        CallbackHelper didCreateNewGroupHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabGroupModelFilterObserver observer =
                            new TabGroupModelFilterObserver() {
                                @Override
                                public void didCreateNewGroup(
                                        Tab destinationTab, TabGroupModelFilter filter) {
                                    assertEquals(tab0, destinationTab);
                                    didCreateNewGroupHelper.notifyCalled();
                                }
                            };
                    mCollectionModel.addTabGroupObserver(observer);
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab0, /* notify= */ true);
                    mCollectionModel.removeTabGroupObserver(observer);

                    assertNotNull(tab0.getTabGroupId());
                    assertEquals(tab0.getTabGroupId(), tab1.getTabGroupId());
                    assertTabsInOrderAre(List.of(tab0, tab1));
                    assertEquals(2, mCollectionModel.getTabsInGroup(tab0.getTabGroupId()).size());
                });

        didCreateNewGroupHelper.waitForOnly();
    }

    @Test
    @MediumTest
    public void testMergeListOfTabsToGroup_MergeIntoExistingGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create a group with tab0 and tab1.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab0, /* notify= */ false);
                    Token groupId = tab0.getTabGroupId();
                    assertNotNull(groupId);

                    // Merge tab2 into the group.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2), tab0, /* notify= */ false);

                    assertEquals(groupId, tab2.getTabGroupId());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                    assertEquals(3, mCollectionModel.getTabsInGroup(groupId).size());
                });
    }

    @Test
    @MediumTest
    public void testMergeListOfTabsToGroup_MergeGroupIntoGroup() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();

        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));

        CallbackHelper didRemoveTabGroupHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create group 1 with tab0, tab1.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab0, /* notify= */ false);
                    Token groupId1 = tab0.getTabGroupId();
                    assertNotNull(groupId1);

                    // Create group 2 with tab2, tab3.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2, tab3), tab2, /* notify= */ false);
                    Token groupId2 = tab2.getTabGroupId();
                    assertNotNull(groupId2);

                    TabGroupModelFilterObserver observer =
                            new TabGroupModelFilterObserver() {
                                @Override
                                public void didRemoveTabGroup(
                                        int tabId, Token tabGroupId, int reason) {
                                    assertEquals(groupId1, tabGroupId);
                                    assertEquals(DidRemoveTabGroupReason.MERGE, reason);
                                    didRemoveTabGroupHelper.notifyCalled();
                                }
                            };
                    mCollectionModel.addTabGroupObserver(observer);

                    // Merge group 1 into group 2.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab2, /* notify= */ true);

                    mCollectionModel.removeTabGroupObserver(observer);

                    assertEquals(groupId2, tab0.getTabGroupId());
                    assertEquals(groupId2, tab1.getTabGroupId());
                    assertEquals(4, mCollectionModel.getTabsInGroup(groupId2).size());
                    assertFalse(mCollectionModel.tabGroupExists(groupId1));
                    assertTabsInOrderAre(List.of(tab2, tab3, tab0, tab1));
                });

        didRemoveTabGroupHelper.waitForOnly();
    }

    @Test
    @MediumTest
    public void testMergeListOfTabsToGroup_AdoptGroupId() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create a group with tab1.
                    mCollectionModel.createSingleTabGroup(tab1);
                    Token groupId = tab1.getTabGroupId();
                    assertNotNull(groupId);

                    // Merge tab0 and tab1, with tab0 as destination. tab0 is not in a group.
                    // The new group should adopt tab1's group ID.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab0, /* notify= */ false);

                    assertEquals(groupId, tab0.getTabGroupId());
                    assertEquals(groupId, tab1.getTabGroupId());
                    assertEquals(2, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                });
    }

    @Test
    @MediumTest
    public void testCreateTabGroupForTabGroupSync() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Token newGroupId = Token.createRandom();
                    mCollectionModel.createTabGroupForTabGroupSync(List.of(tab0, tab1), newGroupId);

                    assertEquals(newGroupId, tab0.getTabGroupId());
                    assertEquals(newGroupId, tab1.getTabGroupId());
                    List<Tab> tabsInGroup = mCollectionModel.getTabsInGroup(newGroupId);
                    assertEquals(2, tabsInGroup.size());
                    assertTrue(tabsInGroup.containsAll(List.of(tab0, tab1)));
                });
    }

    @Test
    @MediumTest
    public void testMergeTabsToGroup_SingleToSingle() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.mergeTabsToGroup(tab0.getId(), tab1.getId(), false);

                    assertNotNull(tab1.getTabGroupId());
                    assertEquals(tab1.getTabGroupId(), tab0.getTabGroupId());
                    assertEquals(2, mCollectionModel.getTabsInGroup(tab1.getTabGroupId()).size());
                    assertTabsInOrderAre(List.of(tab1, tab0));
                });
    }

    @Test
    @MediumTest
    public void testMergeTabsToGroup_SingleToGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.mergeListOfTabsToGroup(List.of(tab1, tab2), tab1, false);
                    Token groupId = tab1.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.mergeTabsToGroup(tab0.getId(), tab1.getId(), false);

                    assertEquals(groupId, tab0.getTabGroupId());
                    assertEquals(3, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab1, tab2, tab0));
                });
    }

    @Test
    @MediumTest
    public void testMergeTabsToGroup_GroupToSingle() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.mergeListOfTabsToGroup(List.of(tab0, tab1), tab0, false);
                    Token groupId = tab0.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.mergeTabsToGroup(tab0.getId(), tab2.getId(), false);

                    assertEquals(groupId, tab2.getTabGroupId());
                    assertEquals(3, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab2, tab0, tab1));
                });
    }

    @Test
    @MediumTest
    public void testMergeListOfTabsToGroupInternal_MergeWithIndex() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        Tab tab4 = createTab();
        Tab tab5 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab2, tab3), tab2, false, null, null);
                    Token groupId = tab2.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab1, tab5), tab2, false, /* indexInGroup= */ 1, null);

                    assertEquals(
                            "mTab1 should have joined the group.", groupId, tab1.getTabGroupId());
                    assertEquals(
                            "mTab4 should have joined the group.", groupId, tab5.getTabGroupId());
                    assertTabsInOrderAre(List.of(tab0, tab2, tab1, tab5, tab3, tab4));
                });
    }

    @Test
    @MediumTest
    public void testMergeListOfTabsToGroupInternal_MergeToFront() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        Tab tab4 = createTab();
        Tab tab5 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab2, tab3), tab2, false, null, null);
                    Token groupId = tab2.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab1, tab5), tab2, false, /* indexInGroup= */ 0, null);

                    assertEquals(
                            "mTab1 should have joined the group.", groupId, tab1.getTabGroupId());
                    assertEquals(
                            "mTab4 should have joined the group.", groupId, tab5.getTabGroupId());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab5, tab2, tab3, tab4));
                });
    }

    @Test
    @MediumTest
    public void testMergeListOfTabsToGroupInternal_MergeTabsWhereTheyAre() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        Tab tab4 = createTab();
        Tab tab5 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab1, tab2, tab3, tab4, tab5), tab1, false, null, null);
                    Token groupId = tab1.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab2, tab3, tab4), tab2, false, /* indexInGroup= */ 2, null);

                    assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));
                });
    }

    @Test
    @MediumTest
    public void testUndoGroupOperation_MergeGroupIntoGroup() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));

        AtomicReference<UndoGroupMetadata> undoGroupMetadataRef = new AtomicReference<>();
        CallbackHelper showUndoSnackbarHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create group 1 with tab0, tab1.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab0, /* notify= */ false);
                    Token groupId1 = tab0.getTabGroupId();
                    assertNotNull(groupId1);

                    // Create group 2 with tab2, tab3.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2, tab3), tab2, /* notify= */ false);
                    Token groupId2 = tab2.getTabGroupId();
                    assertNotNull(groupId2);

                    assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));

                    TabGroupModelFilterObserver observer =
                            new TabGroupModelFilterObserver() {
                                @Override
                                public void showUndoGroupSnackbar(
                                        UndoGroupMetadata undoGroupMetadata) {
                                    undoGroupMetadataRef.set(undoGroupMetadata);
                                    showUndoSnackbarHelper.notifyCalled();
                                }
                            };
                    mCollectionModel.addTabGroupObserver(observer);

                    // Merge group 1 into group 2.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab2, /* notify= */ true);

                    mCollectionModel.removeTabGroupObserver(observer);

                    assertEquals(groupId2, tab0.getTabGroupId());
                    assertEquals(groupId2, tab1.getTabGroupId());
                    assertEquals(4, mCollectionModel.getTabsInGroup(groupId2).size());
                    assertTrue(mCollectionModel.detachedTabGroupExistsForTesting(groupId1));
                    // The group is detached, but not closed yet. The tabGroupExists check is based
                    // on number of tabs so it will be false.
                    assertFalse(mCollectionModel.tabGroupExists(groupId1));
                    assertTabsInOrderAre(List.of(tab2, tab3, tab0, tab1));
                });

        showUndoSnackbarHelper.waitForOnly();
        assertNotNull(undoGroupMetadataRef.get());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.performUndoGroupOperation(undoGroupMetadataRef.get());

                    // State should be restored.
                    Token groupId1 = tab0.getTabGroupId();
                    Token groupId2 = tab2.getTabGroupId();
                    assertNotNull(groupId1);
                    assertNotNull(groupId2);
                    assertNotEquals(groupId1, groupId2);
                    assertEquals(groupId1, tab1.getTabGroupId());
                    assertEquals(groupId2, tab3.getTabGroupId());
                    assertEquals(2, mCollectionModel.getTabsInGroup(groupId1).size());
                    assertEquals(2, mCollectionModel.getTabsInGroup(groupId2).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));
                });
    }

    @Test
    @MediumTest
    public void testUndoGroupOperation_SingleTabIntoGroup() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        AtomicReference<UndoGroupMetadata> undoGroupMetadataRef = new AtomicReference<>();
        CallbackHelper showUndoSnackbarHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create group with tab1, tab2.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab1, tab2), tab1, /* notify= */ false);
                    Token groupId = tab1.getTabGroupId();
                    assertNotNull(groupId);
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));

                    TabGroupModelFilterObserver observer =
                            new TabGroupModelFilterObserver() {
                                @Override
                                public void showUndoGroupSnackbar(
                                        UndoGroupMetadata undoGroupMetadata) {
                                    undoGroupMetadataRef.set(undoGroupMetadata);
                                    showUndoSnackbarHelper.notifyCalled();
                                }
                            };
                    mCollectionModel.addTabGroupObserver(observer);

                    // Merge tab0 into the group.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0), tab1, /* notify= */ true);

                    mCollectionModel.removeTabGroupObserver(observer);

                    assertEquals(groupId, tab0.getTabGroupId());
                    assertEquals(3, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab1, tab2, tab0));
                });

        showUndoSnackbarHelper.waitForOnly();
        assertNotNull(undoGroupMetadataRef.get());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.performUndoGroupOperation(undoGroupMetadataRef.get());

                    // State should be restored.
                    Token groupId = tab1.getTabGroupId();
                    assertNull(tab0.getTabGroupId());
                    assertNotNull(groupId);
                    assertEquals(groupId, tab2.getTabGroupId());
                    assertEquals(2, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                });
    }

    @Test
    @MediumTest
    public void testUndoGroupOperationExpired() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        AtomicReference<UndoGroupMetadata> undoGroupMetadataRef = new AtomicReference<>();
        AtomicReference<Token> groupId1Ref = new AtomicReference<>();
        CallbackHelper showUndoSnackbarHelper = new CallbackHelper();
        final String group1Title = "Group 1 Title";

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create group 1 with tab0.
                    mCollectionModel.createSingleTabGroup(tab0);
                    Token groupId1 = tab0.getTabGroupId();
                    groupId1Ref.set(groupId1);
                    assertNotNull(groupId1);
                    mCollectionModel.setTabGroupTitle(groupId1, group1Title);

                    // Create group 2 with tab1.
                    mCollectionModel.createSingleTabGroup(tab1);
                    Token groupId2 = tab1.getTabGroupId();
                    assertNotNull(groupId2);

                    TabGroupModelFilterObserver observer =
                            new TabGroupModelFilterObserver() {
                                @Override
                                public void showUndoGroupSnackbar(
                                        UndoGroupMetadata undoGroupMetadata) {
                                    undoGroupMetadataRef.set(undoGroupMetadata);
                                    showUndoSnackbarHelper.notifyCalled();
                                }
                            };
                    mCollectionModel.addTabGroupObserver(observer);

                    // Merge group 1 into group 2.
                    mCollectionModel.mergeListOfTabsToGroup(List.of(tab0), tab1, true);
                    mCollectionModel.removeTabGroupObserver(observer);

                    // Group 1 is now detached. Its title should still be available.
                    assertEquals(group1Title, mCollectionModel.getTabGroupTitle(groupId1));
                });

        showUndoSnackbarHelper.waitForOnly();
        assertNotNull(undoGroupMetadataRef.get());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            mCollectionModel.detachedTabGroupExistsForTesting(groupId1Ref.get()));

                    mCollectionModel.undoGroupOperationExpired(undoGroupMetadataRef.get());
                    assertFalse(
                            mCollectionModel.detachedTabGroupExistsForTesting(groupId1Ref.get()));
                });
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

    private void moveRelatedTabs(Tab tab, int index) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.moveRelatedTabs(tab.getId(), index));
    }

    private void moveTab(Tab tab, int index) {
        ThreadUtils.runOnUiThreadBlocking(() -> mRegularModel.moveTab(tab.getId(), index));
    }

    private Tab createTab() {
        return mActivityTestRule.loadUrlInNewTab(mTestUrl, /* incognito= */ false);
    }

    private void mergeListOfTabsToGroup(List<Tab> tabs, Tab destinationTab) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mCollectionModel.mergeListOfTabsToGroup(
                                tabs, destinationTab, /* notify= */ false));
    }

    private Tab createChildTab(Tab parentTab) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return mRegularModel
                            .getTabCreator()
                            .createNewTab(
                                    new LoadUrlParams(mTestUrl),
                                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                                    parentTab);
                });
    }

    private void verifyPinOrUnpin(Tab changedTab, boolean isPinned, boolean willMove)
            throws Exception {
        CallbackHelper willChangePinStateHelper = new CallbackHelper();
        CallbackHelper didChangePinStateHelper = new CallbackHelper();
        CallbackHelper didMoveTabHelper = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void willChangePinState(Tab tab) {
                        assertEquals(changedTab, tab);
                        willChangePinStateHelper.notifyCalled();
                    }

                    @Override
                    public void didChangePinState(Tab tab) {
                        assertEquals(changedTab, tab);
                        didChangePinStateHelper.notifyCalled();
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int oldIndex) {
                        assertEquals(changedTab, tab);
                        if (willMove) {
                            didMoveTabHelper.notifyCalled();
                        } else {
                            fail("didMoveTab should not be called.");
                        }
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRegularModel.addObserver(observer);
                    if (isPinned) {
                        mRegularModel.pinTab(changedTab.getId());
                    } else {
                        mRegularModel.unpinTab(changedTab.getId());
                    }
                    mRegularModel.removeObserver(observer);
                    assertEquals(isPinned, changedTab.getIsPinned());
                });

        willChangePinStateHelper.waitForOnly();
        didChangePinStateHelper.waitForOnly();
        if (willMove) {
            didMoveTabHelper.waitForOnly();
        }
    }
}
