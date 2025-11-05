// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

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
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
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
    public void testTabStripCollection() {
        assertNotNull(mCollectionModel.getTabStripCollection());
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
    // TODO(crbug.com/454344854): Delete this test as part of feature cleanup as the legacy version
    // will be deleted.
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
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
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
    @RequiresRestart("Removing the last tab has divergent behavior on tablet and phone.")
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
        CallbackHelper didRemoveTabForClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingMultipleTabClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingTabClosureHelper = new CallbackHelper();
        CallbackHelper didSelectTabHelper = new CallbackHelper();

        AtomicReference<Tab> tabInWillClose = new AtomicReference<>();
        AtomicReference<Boolean> isSingleInWillClose = new AtomicReference<>();
        AtomicReference<Tab> tabInDidRemove = new AtomicReference<>();
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
                    public void didRemoveTabForClosure(Tab tab) {
                        tabInDidRemove.set(tab);
                        didRemoveTabForClosureHelper.notifyCalled();
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
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTab(tab1).allowUndo(false).build());
                });

        willCloseTabHelper.waitForOnly();
        didRemoveTabForClosureHelper.waitForOnly();
        onFinishingMultipleTabClosureHelper.waitForOnly();
        onFinishingTabClosureHelper.waitForOnly();
        didSelectTabHelper.waitForOnly();

        assertEquals("Incorrect tab in willCloseTab.", tab1, tabInWillClose.get());
        assertTrue("isSingle should be true.", isSingleInWillClose.get());
        assertEquals("Incorrect tab in didRemoveTabForClosure.", tab1, tabInDidRemove.get());
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
        CallbackHelper didRemoveTabForClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingMultipleTabClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingTabClosureHelper = new CallbackHelper();
        CallbackHelper didSelectTabHelper = new CallbackHelper();

        List<Tab> tabsToClose = Arrays.asList(tab1, tab2);
        AtomicReference<List<Tab>> tabsInWillCloseMultiple = new AtomicReference<>();
        List<Tab> tabsInWillCloseTab = Collections.synchronizedList(new ArrayList<>());
        List<Tab> tabsInDidRemove = Collections.synchronizedList(new ArrayList<>());
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
                    public void didRemoveTabForClosure(Tab tab) {
                        tabsInDidRemove.add(tab);
                        didRemoveTabForClosureHelper.notifyCalled();
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
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTabs(tabsToClose).allowUndo(false).build());
                });

        willCloseMultipleTabsHelper.waitForOnly();
        willCloseTabHelper.waitForCallback(0, 2);
        didRemoveTabForClosureHelper.waitForCallback(0, 2);
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
                "Incorrect number of didRemoveTabForClosure calls.", 2, tabsInDidRemove.size());
        assertTrue(
                "Incorrect tabs in didRemoveTabForClosure.",
                tabsInDidRemove.containsAll(tabsToClose));
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
    @RequiresRestart("Removing the last tab has divergent behavior on tablet and phone.")
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
        CallbackHelper didRemoveTabForClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingMultipleTabClosureHelper = new CallbackHelper();
        CallbackHelper onFinishingTabClosureHelper = new CallbackHelper();

        List<Tab> tabsInWillCloseTab = Collections.synchronizedList(new ArrayList<>());
        List<Tab> tabsInDidRemove = Collections.synchronizedList(new ArrayList<>());
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
                    public void didRemoveTabForClosure(Tab tab) {
                        tabsInDidRemove.add(tab);
                        didRemoveTabForClosureHelper.notifyCalled();
                    }

                    @Override
                    public void willCloseMultipleTabs(boolean allowUndo, List<Tab> tabs) {
                        fail("should not be called for close all tabs operation");
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
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeAllTabs().allowUndo(false).build());
                });

        willCloseAllTabsHelper.waitForOnly();
        willCloseTabHelper.waitForCallback(0, 3);
        didRemoveTabForClosureHelper.waitForCallback(0, 3);
        onFinishingMultipleTabClosureHelper.waitForOnly();
        onFinishingTabClosureHelper.waitForCallback(0, 3);

        assertEquals("Incorrect number of willCloseTab calls.", 3, tabsInWillCloseTab.size());
        assertTrue("Incorrect tabs in willCloseTab.", tabsInWillCloseTab.containsAll(allTabs));
        assertEquals(
                "Incorrect number of didRemoveTabForClosure calls.", 3, tabsInDidRemove.size());
        assertTrue(
                "Incorrect tabs in didRemoveTabForClosure.", tabsInDidRemove.containsAll(allTabs));
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
                    assertFalse(mCollectionModel.detachedTabGroupExists(groupId0));

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
                    assertFalse(mCollectionModel.detachedTabGroupExists(groupId1));
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
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
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
                        assertEquals(1, prevFilterIndex);
                        didMoveOutOfGroup.notifyCalled();
                    }

                    @Override
                    public void didRemoveTabGroup(
                            int tabId, Token tabGroupId, @DidRemoveTabGroupReason int reason) {
                        assertEquals(groupId, tabGroupId);
                        assertEquals(DidRemoveTabGroupReason.UNGROUP, reason);
                        didRemoveGroup.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mRegularModel.pinTab(tab1.getId(), /* showUngroupDialog= */ false);
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
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testPinTabInGroup_ActionListener_Accept() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.createSingleTabGroup(tab1));
        assertNotNull(tab1.getTabGroupId());
        assertTabsInOrderAre(List.of(tab0, tab1));

        TabModelActionListener listener = mock(TabModelActionListener.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.pinTab(tab1.getId(), /* showUngroupDialog= */ true, listener);
                });

        onViewWaiting(withText(R.string.delete_tab_group_action), /* checkRootDialog= */ true)
                .perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verify(listener)
                            .onConfirmationDialogResult(
                                    eq(DialogType.SYNC),
                                    eq(ActionConfirmationResult.CONFIRMATION_POSITIVE));
                    assertTrue(tab1.getIsPinned());
                    assertNull(tab1.getTabGroupId());
                    assertTabsInOrderAre(List.of(tab1, tab0));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testPinTabInGroup_ActionListener_Reject() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.createSingleTabGroup(tab1));
        assertNotNull(tab1.getTabGroupId());
        assertTabsInOrderAre(List.of(tab0, tab1));

        TabModelActionListener listener = mock(TabModelActionListener.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.pinTab(tab1.getId(), /* showUngroupDialog= */ true, listener);
                });

        onViewWaiting(withText(R.string.cancel), /* checkRootDialog= */ true).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verify(listener)
                            .onConfirmationDialogResult(
                                    eq(DialogType.SYNC),
                                    eq(ActionConfirmationResult.CONFIRMATION_NEGATIVE));
                    assertFalse(tab1.getIsPinned());
                    assertNotNull(tab1.getTabGroupId());
                    assertTabsInOrderAre(List.of(tab0, tab1));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
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
                        assertEquals(0, prevFilterIndex);
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

                    mRegularModel.pinTab(tab0.getId(), /* showUngroupDialog= */ false);

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

        // Verify that a suggested color is saved when a group is created.
        int storedColor = TabGroupVisualDataStore.getTabGroupColor(tabGroupId);
        assertNotEquals(TabGroupColorUtils.INVALID_COLOR_ID, storedColor);

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
    }

    @Test
    @MediumTest
    public void testCloseTabGroup_VisualDataRemoved() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        createTab();
        mergeListOfTabsToGroup(List.of(tab0, tab1), tab0);
        Token groupId = tab0.getTabGroupId();
        assertNotNull(groupId);

        CallbackHelper didRemoveTabGroupHelper = new CallbackHelper();
        TabGroupModelFilterObserver observer =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didRemoveTabGroup(
                            int tabId, Token tabGroupId, @DidRemoveTabGroupReason int reason) {
                        assertEquals(groupId, tabGroupId);
                        assertEquals(DidRemoveTabGroupReason.CLOSE, reason);
                        didRemoveTabGroupHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.addTabGroupObserver(observer));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String title = "Test title";
                    mCollectionModel.setTabGroupTitle(groupId, title);
                    mCollectionModel.setTabGroupColor(groupId, TabGroupColorId.BLUE);
                    mCollectionModel.setTabGroupCollapsed(groupId, true, false);

                    assertEquals(title, TabGroupVisualDataStore.getTabGroupTitle(groupId));
                    assertEquals(
                            TabGroupColorId.BLUE,
                            TabGroupVisualDataStore.getTabGroupColor(groupId));
                    assertTrue(TabGroupVisualDataStore.getTabGroupCollapsed(groupId));

                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tab0, tab1))
                                    .allowUndo(false)
                                    .build());

                    assertFalse(mCollectionModel.tabGroupExists(groupId));
                    assertNull(TabGroupVisualDataStore.getTabGroupTitle(groupId));
                    assertEquals(
                            TabGroupColorUtils.INVALID_COLOR_ID,
                            TabGroupVisualDataStore.getTabGroupColor(groupId));
                    assertFalse(TabGroupVisualDataStore.getTabGroupCollapsed(groupId));
                });

        didRemoveTabGroupHelper.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeTabGroupObserver(observer));
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
                            List.of(tab1, tab3),
                            tab1,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                });
        assertTabsInOrderAre(List.of(tab0, tab1, tab3, tab2));
        Token tab1GroupId = tab1.getTabGroupId();
        assertNotNull(tab1GroupId);

        List<Tab> representativeTabs =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mCollectionModel.getRepresentativeTabList());
        assertEquals(3, representativeTabs.size());
        assertEquals(tab0, representativeTabs.get(0));
        assertEquals(tab3, representativeTabs.get(1));
        assertEquals(tab2, representativeTabs.get(2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(3, mCollectionModel.getIndividualTabAndGroupCount());

                    assertEquals(tab0, mCollectionModel.getRepresentativeTabAt(0));
                    assertEquals(tab3, mCollectionModel.getRepresentativeTabAt(1));
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
                            List.of(tab0, tab1),
                            tab0,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
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
                            List.of(tab0, tab1),
                            tab0,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    Token groupId1 = tab0.getTabGroupId();
                    assertNotNull(groupId1);

                    // Create group 2 with tab2, tab3.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2, tab3),
                            tab2,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
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
                            List.of(tab0, tab1),
                            tab0,
                            /* notify= */ MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);
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
                            List.of(tab0, tab1),
                            tab0,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    Token groupId = tab0.getTabGroupId();
                    assertNotNull(groupId);

                    // Merge tab2 into the group.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2), tab0, /* notify= */ MergeNotificationType.DONT_NOTIFY);

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
                            List.of(tab0, tab1),
                            tab0,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    Token groupId1 = tab0.getTabGroupId();
                    assertNotNull(groupId1);

                    // Create group 2 with tab2, tab3.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2, tab3),
                            tab2,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
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
                            List.of(tab0, tab1),
                            tab2,
                            /* notify= */ MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);

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
                            List.of(tab0, tab1),
                            tab0,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);

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
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab1, tab2), tab1, MergeNotificationType.DONT_NOTIFY);
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
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1), tab0, MergeNotificationType.DONT_NOTIFY);
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
                            List.of(tab2, tab3),
                            tab2,
                            MergeNotificationType.DONT_NOTIFY,
                            null,
                            null);
                    Token groupId = tab2.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab1, tab5),
                            tab2,
                            MergeNotificationType.DONT_NOTIFY,
                            /* indexInGroup= */ 1,
                            null);

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
                            List.of(tab2, tab3),
                            tab2,
                            MergeNotificationType.DONT_NOTIFY,
                            null,
                            null);
                    Token groupId = tab2.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab1, tab5),
                            tab2,
                            MergeNotificationType.DONT_NOTIFY,
                            /* indexInGroup= */ 0,
                            null);

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
                            List.of(tab1, tab2, tab3, tab4, tab5),
                            tab1,
                            MergeNotificationType.DONT_NOTIFY,
                            null,
                            null);
                    Token groupId = tab1.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab2, tab3, tab4),
                            tab2,
                            MergeNotificationType.DONT_NOTIFY,
                            /* indexInGroup= */ 2,
                            null);

                    assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));
                });
    }

    @Test
    @MediumTest
    public void testMergeListOfTabsToGroupInternal_CreateGroupAndShowUndoSnackbar()
            throws TimeoutException {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        Tab tab4 = createTab();
        Tab tab5 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));

        AtomicReference<UndoGroupMetadata> undoGroupMetadataRef = new AtomicReference<>();
        CallbackHelper showUndoSnackbarHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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

                    mCollectionModel.mergeListOfTabsToGroupInternal(
                            List.of(tab1, tab2, tab3, tab4, tab5),
                            tab1,
                            MergeNotificationType.NOTIFY_ALWAYS,
                            null,
                            null);
                    Token groupId = tab1.getTabGroupId();
                    assertNotNull(groupId);

                    mCollectionModel.removeTabGroupObserver(observer);
                });

        showUndoSnackbarHelper.waitForOnly();
        assertNotNull(undoGroupMetadataRef.get());
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
        final String group1Title = "Group 1";
        final int group1Color = TabGroupColorId.BLUE;
        final String group2Title = "Group 2";
        final int group2Color = TabGroupColorId.RED;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create group 1 with tab0, tab1.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1),
                            tab0,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    Token groupId1 = tab0.getTabGroupId();
                    assertNotNull(groupId1);
                    mCollectionModel.setTabGroupTitle(groupId1, group1Title);
                    mCollectionModel.setTabGroupColor(groupId1, group1Color);

                    // Create group 2 with tab2, tab3.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2, tab3),
                            tab2,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    Token groupId2 = tab2.getTabGroupId();
                    assertNotNull(groupId2);
                    mCollectionModel.setTabGroupTitle(groupId2, group2Title);
                    mCollectionModel.setTabGroupColor(groupId2, group2Color);

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
                            List.of(tab0, tab1),
                            tab2,
                            /* notify= */ MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);

                    mCollectionModel.removeTabGroupObserver(observer);

                    assertEquals(groupId2, tab0.getTabGroupId());
                    assertEquals(groupId2, tab1.getTabGroupId());
                    assertEquals(4, mCollectionModel.getTabsInGroup(groupId2).size());
                    assertTrue(mCollectionModel.detachedTabGroupExists(groupId1));
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

                    // Visual data should be restored.
                    assertEquals(group1Title, mCollectionModel.getTabGroupTitle(groupId1));
                    assertEquals(group1Color, mCollectionModel.getTabGroupColor(groupId1));
                    assertEquals(group2Title, mCollectionModel.getTabGroupTitle(groupId2));
                    assertEquals(group2Color, mCollectionModel.getTabGroupColor(groupId2));
                });
    }

    @Test
    @MediumTest
    public void testUndoGroupOperation_GroupIntoSingleTab() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        Tab tab4 = createTab();
        Tab tab5 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));

        AtomicReference<UndoGroupMetadata> undoGroupMetadataRef = new AtomicReference<>();
        CallbackHelper showUndoSnackbarHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create group with tab1, tab2.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2, tab3, tab4),
                            tab2,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    Token groupId = tab2.getTabGroupId();
                    assertNotNull(groupId);
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));

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

                    // Merge group into the tab.
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2, tab3, tab4),
                            tab0,
                            /* notify= */ MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);

                    mCollectionModel.removeTabGroupObserver(observer);

                    assertEquals(groupId, tab0.getTabGroupId());
                    assertEquals(4, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab2, tab3, tab4, tab1, tab5));
                });

        showUndoSnackbarHelper.waitForOnly();
        assertNotNull(undoGroupMetadataRef.get());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.performUndoGroupOperation(undoGroupMetadataRef.get());

                    // State should be restored.
                    Token groupId = tab2.getTabGroupId();
                    assertNull(tab0.getTabGroupId());
                    assertNull(tab1.getTabGroupId());
                    assertNull(tab5.getTabGroupId());
                    assertNotNull(groupId);
                    assertEquals(groupId, tab3.getTabGroupId());
                    assertEquals(groupId, tab4.getTabGroupId());
                    assertEquals(3, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3, tab4, tab5));
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
                            List.of(tab1, tab2),
                            tab1,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
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
                            List.of(tab0),
                            tab1,
                            /* notify= */ MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);

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
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0), tab1, MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);
                    mCollectionModel.removeTabGroupObserver(observer);

                    // Group 1 is now detached. Its title should still be available.
                    assertEquals(group1Title, mCollectionModel.getTabGroupTitle(groupId1));
                });

        showUndoSnackbarHelper.waitForOnly();
        assertNotNull(undoGroupMetadataRef.get());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.detachedTabGroupExists(groupId1Ref.get()));

                    mCollectionModel.undoGroupOperationExpired(undoGroupMetadataRef.get());
                    assertFalse(mCollectionModel.detachedTabGroupExists(groupId1Ref.get()));
                });
    }

    @Test
    @MediumTest
    public void testAddTabsToGroup_CreateNewGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Token newGroupId = mCollectionModel.addTabsToGroup(null, List.of(tab0, tab1));
                    assertNotNull(newGroupId);
                    assertEquals(newGroupId, tab0.getTabGroupId());
                    assertEquals(newGroupId, tab1.getTabGroupId());
                    assertNull(tab2.getTabGroupId());
                    assertEquals(2, mCollectionModel.getTabsInGroup(newGroupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                });
    }

    @Test
    @MediumTest
    public void testAddTabsToGroup_AddToExistingGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.createSingleTabGroup(tab0);
                    Token groupId = tab0.getTabGroupId();
                    assertNotNull(groupId);

                    Token returnedGroupId =
                            mCollectionModel.addTabsToGroup(groupId, List.of(tab1, tab2));
                    assertEquals(groupId, returnedGroupId);
                    assertEquals(groupId, tab0.getTabGroupId());
                    assertEquals(groupId, tab1.getTabGroupId());
                    assertEquals(groupId, tab2.getTabGroupId());
                    assertEquals(3, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                });
    }

    @Test
    @MediumTest
    public void testAddTabsToGroup_MoveFromAnotherGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.createSingleTabGroup(tab0);
                    Token groupId1 = tab0.getTabGroupId();
                    assertNotNull(groupId1);

                    mCollectionModel.createSingleTabGroup(tab1);
                    Token groupId2 = tab1.getTabGroupId();
                    assertNotNull(groupId2);
                    assertNotEquals(groupId1, groupId2);

                    Token returnedGroupId =
                            mCollectionModel.addTabsToGroup(groupId1, List.of(tab1));
                    assertEquals(groupId1, returnedGroupId);
                    assertEquals(groupId1, tab0.getTabGroupId());
                    assertEquals(groupId1, tab1.getTabGroupId());
                    assertNull(tab2.getTabGroupId());
                    assertEquals(2, mCollectionModel.getTabsInGroup(groupId1).size());
                    assertFalse(mCollectionModel.tabGroupExists(groupId2));
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                });
    }

    @Test
    @MediumTest
    public void testAddTabsToGroup_EmptyList() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Token newGroupId =
                            mCollectionModel.addTabsToGroup(null, Collections.emptyList());
                    assertNull(newGroupId);

                    mCollectionModel.createSingleTabGroup(tab0);
                    Token groupId = tab0.getTabGroupId();
                    assertNotNull(groupId);

                    Token returnedGroupId =
                            mCollectionModel.addTabsToGroup(groupId, Collections.emptyList());
                    assertNull(returnedGroupId);
                    assertEquals(1, mCollectionModel.getTabsInGroup(groupId).size());
                });
    }

    @Test
    @MediumTest
    public void testAddTabsToGroup_SomeTabsAlreadyInGroup() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.createSingleTabGroup(tab0);
                    Token groupId = tab0.getTabGroupId();
                    assertNotNull(groupId);

                    Token returnedGroupId =
                            mCollectionModel.addTabsToGroup(groupId, List.of(tab0, tab1));
                    assertEquals(groupId, returnedGroupId);
                    assertEquals(groupId, tab0.getTabGroupId());
                    assertEquals(groupId, tab1.getTabGroupId());
                    assertNull(tab2.getTabGroupId());
                    assertEquals(2, mCollectionModel.getTabsInGroup(groupId).size());
                    assertTabsInOrderAre(List.of(tab0, tab1, tab2));
                });
    }

    @Test
    @MediumTest
    public void testAddTabsToGroup_AddToNonExistentGroup() {
        Tab tab0 = getTabAt(0);
        assertTabsInOrderAre(List.of(tab0));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Token nonExistentGroupId = Token.createRandom();
                    Token returnedGroupId =
                            mCollectionModel.addTabsToGroup(nonExistentGroupId, List.of(tab0));
                    assertNull(returnedGroupId);
                    assertNull(tab0.getTabGroupId());
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
                                tabs,
                                destinationTab,
                                /* notify= */ MergeNotificationType.DONT_NOTIFY));
    }

    @Test
    @MediumTest
    public void testMergeActivatedTabToGroup_UpdatesLastShownTabId() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
        assertEquals(tab2, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab0, tab1),
                            tab0,
                            /* notify= */ MergeNotificationType.DONT_NOTIFY);
                    Token groupId = tab0.getTabGroupId();
                    assertNotNull(groupId);

                    assertEquals(tab0.getId(), mCollectionModel.getGroupLastShownTabId(groupId));

                    mCollectionModel.mergeListOfTabsToGroup(
                            List.of(tab2), tab0, /* notify= */ MergeNotificationType.DONT_NOTIFY);

                    assertEquals(tab2.getId(), mCollectionModel.getGroupLastShownTabId(groupId));
                });
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
                        mRegularModel.pinTab(changedTab.getId(), /* showUngroupDialog= */ false);
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

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testCloseTab_Undo() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1));
        assertEquals(tab1, getCurrentTab());

        CallbackHelper onTabPendingClosure = new CallbackHelper();
        CallbackHelper willUndoTabClosure = new CallbackHelper();
        CallbackHelper onTabCloseUndone = new CallbackHelper();
        CallbackHelper didSelectOnCloseHelper = new CallbackHelper();
        CallbackHelper didSelectOnUndoHelper = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs, boolean isAllTabs, @TabClosingSource int source) {
                        assertEquals(TabClosingSource.UNKNOWN, source);
                        assertEquals(1, tabs.size());
                        assertEquals(tab1, tabs.get(0));
                        onTabPendingClosure.notifyCalled();
                    }

                    @Override
                    public void willUndoTabClosure(List<Tab> tabs, boolean isAllTabs) {
                        assertEquals(1, tabs.size());
                        assertEquals(tab1, tabs.get(0));
                        willUndoTabClosure.notifyCalled();
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        assertEquals(tab1, tab);
                        onTabCloseUndone.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (didSelectOnCloseHelper.getCallCount() == 0) {
                            // First event: Caused by closing tab1, selecting tab0.
                            assertEquals(tab0, tab);
                            assertEquals(TabSelectionType.FROM_CLOSE, type);
                            assertEquals(tab1.getId(), lastId);
                            didSelectOnCloseHelper.notifyCalled();
                        } else {
                            // Second event: Caused by UndoRefocusHelper re-selecting the restored
                            // tab1.
                            assertEquals(tab1, tab);
                            assertEquals(tab0.getId(), lastId);
                            didSelectOnUndoHelper.notifyCalled();
                        }
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.closeTabs(TabClosureParams.closeTab(tab1).build());
                });

        onTabPendingClosure.waitForOnly();
        didSelectOnCloseHelper.waitForOnly();

        assertEquals(1, getCount());
        assertTabsInOrderAre(List.of(tab0));
        assertEquals(tab0, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.isClosurePending(tab1.getId()));
                    mCollectionModel.cancelTabClosure(tab1.getId());
                });
        willUndoTabClosure.waitForOnly();
        onTabCloseUndone.waitForOnly();
        didSelectOnUndoHelper.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(
                () -> assertFalse(mCollectionModel.isClosurePending(tab1.getId())));
        assertEquals(2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab1));
        assertEquals(tab1, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testCloseTab_UndoPinnedTab() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        ThreadUtils.runOnUiThreadBlocking(() -> tab0.setIsPinned(true));
        assertTabsInOrderAre(List.of(tab0, tab1));
        assertEquals(tab1, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.closeTabs(TabClosureParams.closeTab(tab0).build());
                });
        assertTabsInOrderAre(List.of(tab1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.isClosurePending(tab0.getId()));
                    mCollectionModel.cancelTabClosure(tab0.getId());
                });
        assertEquals(2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab1));
        assertTrue(getTabAt(0).getIsPinned());
    }

    @Test
    @MediumTest
    public void testCloseTab_UndoGroupedTab() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createChildTab(tab0);
        Token tabGroupId = tab0.getTabGroupId();
        assertNotNull(tabGroupId);
        assertEquals(tabGroupId, tab1.getTabGroupId());
        assertTabsInOrderAre(List.of(tab0, tab1));
        assertEquals(tab1, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.closeTabs(TabClosureParams.closeTab(tab0).build());
                });
        assertTabsInOrderAre(List.of(tab1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.isClosurePending(tab0.getId()));
                    mCollectionModel.cancelTabClosure(tab0.getId());
                });
        assertEquals(2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab1));
        assertEquals(tabGroupId, getTabAt(0).getTabGroupId());
        assertEquals(tabGroupId, getTabAt(1).getTabGroupId());
    }

    @Test
    @MediumTest
    public void testCloseTab_UndoLastTabInGroup_VisualDataRestored() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        mergeListOfTabsToGroup(List.of(tab0, tab1), tab0);
        Token tabGroupId = tab0.getTabGroupId();
        assertNotNull(tabGroupId);
        final String title = "Test Title";
        final int color = TabGroupColorId.BLUE;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.setTabGroupTitle(tabGroupId, title);
                    mCollectionModel.setTabGroupColor(tabGroupId, color);

                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tab0, tab1))
                                    .allowUndo(true)
                                    .build());
                });
        assertEquals(1, getCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.cancelTabClosure(tab0.getId());
                    mCollectionModel.cancelTabClosure(tab1.getId());
                    assertEquals(title, mCollectionModel.getTabGroupTitle(tabGroupId));
                    assertEquals(color, mCollectionModel.getTabGroupColor(tabGroupId));
                });
        assertEquals(3, getCount());
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
        assertEquals(tabGroupId, getTabAt(0).getTabGroupId());
        assertEquals(tabGroupId, getTabAt(1).getTabGroupId());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    @RequiresRestart("Removing the last tab has divergent behavior on tablet and phone.")
    public void testCloseTab_UndoLastTab() throws Exception {
        assertEquals(1, getCount());
        Tab tab0 = getCurrentTab();

        CallbackHelper onTabPendingClosure = new CallbackHelper();
        CallbackHelper willUndoTabClosure = new CallbackHelper();
        CallbackHelper onTabCloseUndone = new CallbackHelper();
        CallbackHelper didSelectTabHelper = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs, boolean isAllTabs, @TabClosingSource int source) {
                        assertEquals(TabClosingSource.UNKNOWN, source);
                        assertEquals(1, tabs.size());
                        assertEquals(tab0, tabs.get(0));
                        onTabPendingClosure.notifyCalled();
                    }

                    @Override
                    public void willUndoTabClosure(List<Tab> tabs, boolean isAllTabs) {
                        assertEquals(1, tabs.size());
                        assertEquals(tab0, tabs.get(0));
                        willUndoTabClosure.notifyCalled();
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        assertEquals(tab0, tab);
                        onTabCloseUndone.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        // Only validate and notify for the first call.
                        if (didSelectTabHelper.getCallCount() > 0) return;

                        assertEquals(tab0, tab);
                        assertEquals(TabSelectionType.FROM_UNDO, type);
                        assertEquals(0, lastId);
                        didSelectTabHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.closeTabs(TabClosureParams.closeTab(tab0).build());
                });

        onTabPendingClosure.waitForOnly();
        assertEquals(0, getCount());
        assertNull(getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.isClosurePending(tab0.getId()));
                    mCollectionModel.cancelTabClosure(tab0.getId());
                });

        willUndoTabClosure.waitForOnly();
        onTabCloseUndone.waitForOnly();
        didSelectTabHelper.waitForOnly();

        ThreadUtils.runOnUiThreadBlocking(
                () -> assertFalse(mCollectionModel.isClosurePending(tab0.getId())));
        assertEquals(1, getCount());
        assertEquals(tab0, getCurrentTab());

        // Exit the tab switcher to fulfill AutoResetCtaTransitTestRule for future tests.
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        LayoutManagerChrome layoutManager = cta.getLayoutManager();
        LayoutTestUtils.waitForLayout(layoutManager, LayoutType.TAB_SWITCHER);
        clickFirstCardFromTabSwitcher(cta);
        LayoutTestUtils.waitForLayout(layoutManager, LayoutType.BROWSING);

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    public void testGetTabsNavigatedInTimeWindow() {
        Tab tab1 = getTabAt(0);
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        Tab tab4 = createTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(tab1, 200);
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(tab2, 50);
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(tab3, 100);
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(tab4, 10);

                    assertEquals(
                            Arrays.asList(tab2, tab4),
                            mCollectionModel.getTabsNavigatedInTimeWindow(10, 100));
                });
    }

    @Test
    @MediumTest
    public void testCloseTabsNavigatedInTimeWindow() {
        Tab tab1 = getTabAt(0);
        Tab tab2 = createTab();
        Tab tab3 = createTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(tab1, 200);
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(tab2, 30);
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(tab3, 20);
                });

        assertEquals(3, getCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.closeTabsNavigatedInTimeWindow(20, 50);
                });

        assertEquals(1, getCount());
        assertEquals(tab1, getTabAt(0));
    }

    @Test
    @MediumTest
    public void testCloseTab_Commit() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.closeTabs(TabClosureParams.closeTab(tab1).build());
                    assertTrue(mCollectionModel.isClosurePending(tab1.getId()));
                });
        assertEquals(1, getCount());

        CallbackHelper onTabClosureCommitted = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        assertEquals(tab1, tab);
                        onTabClosureCommitted.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.commitTabClosure(tab1.getId());
                });

        onTabClosureCommitted.waitForOnly();

        ThreadUtils.runOnUiThreadBlocking(
                () -> assertFalse(mCollectionModel.isClosurePending(tab1.getId())));
        assertTrue(tab1.isDestroyed());

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    public void testCloseTab_UponExitNotUndoable() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1));
        assertEquals(tab1, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTab(tab1).allowUndo(true).uponExit(true).build());
                });

        assertEquals(1, getCount());
        assertTabsInOrderAre(List.of(tab0));
        assertEquals(tab0, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mCollectionModel.isClosurePending(tab1.getId()));
                });
        assertTrue(tab1.isDestroyed());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testCloseTabs_UndoMultiple_ClosureRefactor() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.setIndex(2, TabSelectionType.FROM_USER));
        assertEquals(tab2, getCurrentTab());

        List<Tab> tabsToClose = List.of(tab1, tab2);
        Set<Tab> tabsToCloseSet = new HashSet<>(tabsToClose);
        CallbackHelper pendingClosureHelper = new CallbackHelper();
        CallbackHelper willUndoTabClosure = new CallbackHelper();
        CallbackHelper onTabCloseUndoneHelper = new CallbackHelper();

        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs, boolean isAllTabs, @TabClosingSource int source) {
                        assertEquals(tabsToClose, tabs);
                        assertFalse(isAllTabs);
                        pendingClosureHelper.notifyCalled();
                    }

                    @Override
                    public void willUndoTabClosure(List<Tab> tabs, boolean isAllTabs) {
                        assertEquals(1, tabs.size());
                        assertTrue(tabsToCloseSet.containsAll(tabs));
                        assertFalse(isAllTabs);
                        willUndoTabClosure.notifyCalled();
                    }

                    @Override
                    public void onTabCloseUndone(List<Tab> tabs, boolean isAllTabs) {
                        assertEquals(1, tabs.size());
                        assertTrue(tabsToCloseSet.containsAll(tabs));
                        assertFalse(isAllTabs);
                        onTabCloseUndoneHelper.notifyCalled();
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        fail(
                                "tabClosureUndone should not be called with closure refactor"
                                        + " enabled.");
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.closeTabs(TabClosureParams.closeTabs(tabsToClose).build());
                });

        pendingClosureHelper.waitForOnly();
        assertEquals(2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab3));
        assertEquals(tab0, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.isClosurePending(tab1.getId()));
                    assertTrue(mCollectionModel.isClosurePending(tab2.getId()));
                    for (Tab tabToClose : tabsToClose) {
                        mCollectionModel.cancelTabClosure(tabToClose.getId());
                    }
                });
        willUndoTabClosure.waitForCallback(0, 2);
        onTabCloseUndoneHelper.waitForCallback(0, 2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mCollectionModel.isClosurePending(tab1.getId()));
                    assertFalse(mCollectionModel.isClosurePending(tab2.getId()));
                });
        assertEquals(4, getCount());
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));

        assertEquals(tab0, getCurrentTab());
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testCloseTabs_UndoMultiple() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.setIndex(2, TabSelectionType.FROM_USER));
        assertEquals(tab2, getCurrentTab());

        List<Tab> tabsToClose = List.of(tab1, tab2);
        Set<Tab> tabsToCloseSet = new HashSet<>(tabsToClose);
        CallbackHelper pendingClosureHelper = new CallbackHelper();
        CallbackHelper willUndoTabClosure = new CallbackHelper();
        CallbackHelper tabClosureUndoneHelper = new CallbackHelper();

        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs, boolean isAllTabs, @TabClosingSource int source) {
                        assertEquals(tabsToClose, tabs);
                        assertFalse(isAllTabs);
                        pendingClosureHelper.notifyCalled();
                    }

                    @Override
                    public void willUndoTabClosure(List<Tab> tabs, boolean isAllTabs) {
                        assertTrue(tabsToCloseSet.containsAll(tabs));
                        assertFalse(isAllTabs);
                        willUndoTabClosure.notifyCalled();
                    }

                    @Override
                    public void onTabCloseUndone(List<Tab> tabs, boolean isAllTabs) {
                        fail(
                                "onTabCloseUndone should not be called with closure refactor"
                                        + " disabled.");
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        assertTrue(tabsToCloseSet.contains(tab));
                        tabClosureUndoneHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.closeTabs(TabClosureParams.closeTabs(tabsToClose).build());
                });

        pendingClosureHelper.waitForOnly();
        assertEquals(2, getCount());
        assertTabsInOrderAre(List.of(tab0, tab3));
        assertEquals(tab0, getCurrentTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.isClosurePending(tab1.getId()));
                    assertTrue(mCollectionModel.isClosurePending(tab2.getId()));
                    for (Tab tabToClose : tabsToClose) {
                        mCollectionModel.cancelTabClosure(tabToClose.getId());
                    }
                });
        willUndoTabClosure.waitForCallback(0, 2);
        tabClosureUndoneHelper.waitForCallback(0, 2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mCollectionModel.isClosurePending(tab1.getId()));
                    assertFalse(mCollectionModel.isClosurePending(tab2.getId()));
                });
        assertEquals(4, getCount());
        assertTabsInOrderAre(List.of(tab0, tab1, tab2, tab3));

        assertEquals(tab2, getCurrentTab());
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    public void testCloseTabs_CommitMultiple() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        List<Tab> tabsToClose = List.of(tab0, tab1);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.closeTabs(TabClosureParams.closeTabs(tabsToClose).allowUndo(true).build());
                    assertTrue(mCollectionModel.isClosurePending(tab0.getId()));
                    assertTrue(mCollectionModel.isClosurePending(tab1.getId()));
                });
        assertTabsInOrderAre(List.of(tab2));

        CallbackHelper onTabClosureCommitted = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        // This may be called for either tab0 or tab1.
                        assertTrue(tabsToClose.contains(tab));
                        onTabClosureCommitted.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addObserver(observer);
                    mCollectionModel.commitAllTabClosures();
                });
        onTabClosureCommitted.waitForCallback(0, 2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mCollectionModel.isClosurePending(tab0.getId()));
                    assertFalse(mCollectionModel.isClosurePending(tab1.getId()));
                });
        assertTrue(tab0.isDestroyed());
        assertTrue(tab1.isDestroyed());
        assertEquals(1, getCount());
        assertTabsInOrderAre(List.of(tab2));

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    @RequiresRestart("Removing the last tab has divergent behavior on tablet and phone.")
    public void testCloseAllTabs_Undo() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        List<Tab> allTabs = List.of(tab0, tab1, tab2);
        Set<Tab> allTabSet = new HashSet<>(allTabs);
        assertTabsInOrderAre(allTabs);
        assertEquals(3, getCount());

        CallbackHelper willCloseAllTabsHelper = new CallbackHelper();
        CallbackHelper willUndoTabClosure = new CallbackHelper();
        CallbackHelper tabClosureUndoneHelper = new CallbackHelper();
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void willCloseAllTabs(boolean isIncognito) {
                        willCloseAllTabsHelper.notifyCalled();
                    }

                    @Override
                    public void willCloseMultipleTabs(boolean allowUndo, List<Tab> tabs) {
                        fail("should not be called for close all tabs operation");
                    }

                    @Override
                    public void willUndoTabClosure(List<Tab> tabs, boolean isAllTabs) {
                        assertTrue(allTabSet.containsAll(tabs));
                        willUndoTabClosure.notifyCalled();
                    }

                    @Override
                    public void onTabCloseUndone(List<Tab> tabs, boolean isAllTabs) {
                        fail(
                                "onTabCloseUndone should not be called with closure refactor"
                                        + " disabled.");
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        assertTrue(allTabSet.contains(tab));
                        tabClosureUndoneHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.addObserver(observer));

        TabUiTestHelper.enterTabSwitcher(cta);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.closeTabs(TabClosureParams.closeAllTabs().build());
                });

        willCloseAllTabsHelper.waitForOnly();

        assertEquals(0, getCount());
        assertNull(getCurrentTab());
        assertFalse(tab0.isDestroyed());
        assertFalse(tab1.isDestroyed());
        assertFalse(tab2.isDestroyed());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.getComprehensiveModel().getCount() > 0);
                    for (Tab tabToClose : allTabs) {
                        mCollectionModel.cancelTabClosure(tabToClose.getId());
                    }
                });
        willUndoTabClosure.waitForCallback(0, 3);
        tabClosureUndoneHelper.waitForCallback(0, 3);

        assertNotNull(getCurrentTab());
        assertEquals(3, getCount());

        // Exit the tab switcher to fulfill AutoResetCtaTransitTestRule for future tests.
        TabUiTestHelper.clickNthCardFromTabSwitcher(mActivityTestRule.getActivity(), 0);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);

        ThreadUtils.runOnUiThreadBlocking(() -> mCollectionModel.removeObserver(observer));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR})
    public void testCloseTabGroup_UndoableHiding() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        List<Tab> groupTabs = List.of(tab0, tab1);
        mergeListOfTabsToGroup(groupTabs, tab0);
        Token tabGroupId = tab0.getTabGroupId();
        assertNotNull(tabGroupId);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
        // Select a tab outside the group to be closed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mCollectionModel.setIndex(2, TabSelectionType.FROM_USER));
        assertEquals(tab2, getCurrentTab());

        CallbackHelper willCloseTabGroupHelper = new CallbackHelper();
        CallbackHelper onTabPendingClosureHelper = new CallbackHelper();
        CallbackHelper onTabCloseUndoneHelper = new CallbackHelper();
        AtomicBoolean hidingInWillClose = new AtomicBoolean();

        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willCloseTabGroup(Token id, boolean hiding) {
                        assertEquals(tabGroupId, id);
                        hidingInWillClose.set(hiding);
                        willCloseTabGroupHelper.notifyCalled();
                    }
                };

        TabModelObserver modelObserver =
                new TabModelObserver() {
                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs,
                            boolean isAllTabs,
                            @TabClosingSource int closingSource) {
                        assertEquals(2, tabs.size());
                        assertTrue(new HashSet<>(tabs).equals(new HashSet<>(groupTabs)));
                        onTabPendingClosureHelper.notifyCalled();
                    }

                    @Override
                    public void onTabCloseUndone(List<Tab> tabs, boolean isAllTabs) {
                        assertEquals(1, tabs.size()); // It's called for each tab.
                        onTabCloseUndoneHelper.notifyCalled();
                    }
                };

        String groupTitle = "Test Group";
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mCollectionModel.addObserver(modelObserver);
                    mCollectionModel.setTabGroupTitle(tabGroupId, groupTitle);
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTabs(groupTabs)
                                    .allowUndo(true)
                                    .hideTabGroups(true)
                                    .build());
                });

        willCloseTabGroupHelper.waitForOnly();
        onTabPendingClosureHelper.waitForOnly();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(hidingInWillClose.get());
                    assertTrue(mCollectionModel.isTabGroupHiding(tabGroupId));
                    assertTrue(mCollectionModel.detachedTabGroupExists(tabGroupId));
                    assertFalse(mCollectionModel.tabGroupExists(tabGroupId));
                    assertTrue(mCollectionModel.isClosurePending(tab0.getId()));
                    assertTrue(mCollectionModel.isClosurePending(tab1.getId()));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.cancelTabClosure(tab1.getId());
                    mCollectionModel.cancelTabClosure(tab0.getId());
                });
        onTabCloseUndoneHelper.waitForCallback(0, 2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.tabGroupExists(tabGroupId));
                    assertEquals(tab1.getId(), mCollectionModel.getGroupLastShownTabId(tabGroupId));
                    assertFalse(mCollectionModel.isTabGroupHiding(tabGroupId));
                    assertFalse(mCollectionModel.detachedTabGroupExists(tabGroupId));
                    assertFalse(mCollectionModel.isClosurePending(tab0.getId()));
                    assertFalse(mCollectionModel.isClosurePending(tab1.getId()));
                    assertEquals(tabGroupId, tab0.getTabGroupId());
                    assertEquals(tabGroupId, tab1.getTabGroupId());
                    assertEquals(groupTitle, mCollectionModel.getTabGroupTitle(tabGroupId));

                    mCollectionModel.removeTabGroupObserver(groupObserver);
                    mCollectionModel.removeObserver(modelObserver);
                });
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));
    }

    @Test
    @MediumTest
    public void testCloseTabGroup_CommitHiding() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        List<Tab> groupTabs = List.of(tab0, tab1);
        mergeListOfTabsToGroup(groupTabs, tab0);
        Token tabGroupId = tab0.getTabGroupId();
        assertNotNull(tabGroupId);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        CallbackHelper willCloseTabGroupHelper = new CallbackHelper();
        CallbackHelper committedTabGroupClosureHelper = new CallbackHelper();
        AtomicBoolean hidingInWillClose = new AtomicBoolean();
        AtomicBoolean hidingInCommitted = new AtomicBoolean();

        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willCloseTabGroup(Token id, boolean hiding) {
                        assertEquals(tabGroupId, id);
                        hidingInWillClose.set(hiding);
                        willCloseTabGroupHelper.notifyCalled();
                    }

                    @Override
                    public void committedTabGroupClosure(Token id, boolean hiding) {
                        assertEquals(tabGroupId, id);
                        hidingInCommitted.set(hiding);
                        committedTabGroupClosureHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTabs(groupTabs)
                                    .allowUndo(true)
                                    .hideTabGroups(true)
                                    .build());
                });

        willCloseTabGroupHelper.waitForOnly();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(hidingInWillClose.get());
                    assertTrue(mCollectionModel.isTabGroupHiding(tabGroupId));
                    assertTrue(mCollectionModel.detachedTabGroupExists(tabGroupId));
                });

        // Commit the closure
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.commitTabClosure(tab0.getId());
                    mCollectionModel.commitTabClosure(tab1.getId());
                });
        committedTabGroupClosureHelper.waitForOnly();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mCollectionModel.isTabGroupHiding(tabGroupId));
                    assertFalse(mCollectionModel.detachedTabGroupExists(tabGroupId));
                    assertFalse(mCollectionModel.isClosurePending(tab0.getId()));
                    assertFalse(mCollectionModel.isClosurePending(tab1.getId()));
                    assertTrue(tab0.isDestroyed());
                    assertTrue(tab1.isDestroyed());

                    mCollectionModel.removeTabGroupObserver(groupObserver);
                });
        assertEquals(1, getCount());
        assertTabsInOrderAre(List.of(tab2));
    }

    @Test
    @MediumTest
    public void testCloseTabGroup_NotUndoableHiding() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        List<Tab> groupTabs = List.of(tab0, tab1);
        mergeListOfTabsToGroup(groupTabs, tab0);
        Token tabGroupId = tab0.getTabGroupId();
        assertNotNull(tabGroupId);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        CallbackHelper willCloseTabGroupHelper = new CallbackHelper();
        CallbackHelper committedTabGroupClosureHelper = new CallbackHelper();
        AtomicBoolean hidingInWillClose = new AtomicBoolean();
        AtomicBoolean hidingInCommitted = new AtomicBoolean();

        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willCloseTabGroup(Token id, boolean hiding) {
                        assertEquals(tabGroupId, id);
                        hidingInWillClose.set(hiding);
                        willCloseTabGroupHelper.notifyCalled();
                    }

                    @Override
                    public void committedTabGroupClosure(Token id, boolean hiding) {
                        assertEquals(tabGroupId, id);
                        hidingInCommitted.set(hiding);
                        committedTabGroupClosureHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTabs(groupTabs)
                                    .allowUndo(false)
                                    .hideTabGroups(true)
                                    .build());
                });

        willCloseTabGroupHelper.waitForOnly();
        committedTabGroupClosureHelper.waitForOnly();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(hidingInWillClose.get());
                    assertTrue(hidingInCommitted.get());
                    assertFalse(mCollectionModel.isTabGroupHiding(tabGroupId));
                    assertFalse(mCollectionModel.detachedTabGroupExists(tabGroupId));
                    assertTrue(tab0.isDestroyed());
                    assertTrue(tab1.isDestroyed());

                    mCollectionModel.removeTabGroupObserver(groupObserver);
                });
        assertEquals(1, getCount());
        assertTabsInOrderAre(List.of(tab2));
    }

    @Test
    @MediumTest
    public void testCloseTabGroup_Partial() {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        List<Tab> groupTabs = List.of(tab0, tab1, tab2);
        mergeListOfTabsToGroup(groupTabs, tab0);
        Token tabGroupId = tab0.getTabGroupId();
        assertNotNull(tabGroupId);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willCloseTabGroup(Token id, boolean hiding) {
                        fail("willCloseTabGroup should not be called for partial closure.");
                    }

                    @Override
                    public void committedTabGroupClosure(Token id, boolean hiding) {
                        fail("committedTabGroupClosure should not be called for partial closure.");
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTabs(List.of(tab0, tab1))
                                    .allowUndo(true)
                                    .hideTabGroups(true)
                                    .build());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mCollectionModel.isClosurePending(tab0.getId()));
                    assertTrue(mCollectionModel.isClosurePending(tab1.getId()));
                    assertFalse(mCollectionModel.isTabGroupHiding(tabGroupId));
                    assertTrue(mCollectionModel.tabGroupExists(tabGroupId));
                    assertFalse(mCollectionModel.detachedTabGroupExists(tabGroupId));

                    mCollectionModel.removeTabGroupObserver(groupObserver);
                });
        assertEquals(1, getCount());
        assertTabsInOrderAre(List.of(tab2));
        assertEquals(tabGroupId, tab2.getTabGroupId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.commitTabClosure(tab0.getId());
                    mCollectionModel.commitTabClosure(tab1.getId());
                });
    }

    @Test
    @MediumTest
    public void testCloseTabGroup_HidingDisabled() throws Exception {
        Tab tab0 = getTabAt(0);
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        List<Tab> groupTabs = List.of(tab0, tab1);
        mergeListOfTabsToGroup(groupTabs, tab0);
        Token tabGroupId = tab0.getTabGroupId();
        assertNotNull(tabGroupId);
        assertTabsInOrderAre(List.of(tab0, tab1, tab2));

        CallbackHelper willCloseTabGroupHelper = new CallbackHelper();
        // Should get reset to false.
        AtomicBoolean hidingInWillClose = new AtomicBoolean(true);

        TabGroupModelFilterObserver groupObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willCloseTabGroup(Token id, boolean hiding) {
                        assertEquals(tabGroupId, id);
                        hidingInWillClose.set(hiding);
                        willCloseTabGroupHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCollectionModel.addTabGroupObserver(groupObserver);
                    mCollectionModel.closeTabs(
                            TabClosureParams.closeTabs(groupTabs).hideTabGroups(false).build());
                });

        willCloseTabGroupHelper.waitForOnly();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(hidingInWillClose.get());
                    assertFalse(mCollectionModel.isTabGroupHiding(tabGroupId));
                    // A detached group is still created for undo.
                    assertTrue(mCollectionModel.detachedTabGroupExists(tabGroupId));

                    mCollectionModel.commitTabClosure(tab0.getId());
                    mCollectionModel.commitTabClosure(tab1.getId());

                    assertFalse(mCollectionModel.detachedTabGroupExists(tabGroupId));

                    mCollectionModel.removeTabGroupObserver(groupObserver);
                });
        assertEquals(1, getCount());
        assertTabsInOrderAre(List.of(tab2));
    }
}
