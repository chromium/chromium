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

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
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
