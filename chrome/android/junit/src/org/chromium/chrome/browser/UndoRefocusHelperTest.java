// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.metrics.UmaRecorder;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.TestTabModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.test.util.browser.Features;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Test suite to verify that the TabUsageTracker correctly records the number of tabs used and the
 * percentage of tabs used.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures(ChromeFeatureList.TAB_STRIP_IMPROVEMENTS)
@Config(manifest = Config.NONE)
public class UndoRefocusHelperTest {
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    ObservableSupplier<LayoutManagerImpl> mLayoutManagerObservableSupplier;
    @Mock
    private UmaRecorder mUmaRecorder;

    private static final String UNDO_CLOSE_TAB_USER_ACTION = "TabletTabStrip.UndoCloseTab";
    private final TestTabModel mModel = new TestTabModel();

    private UndoRefocusHelper mUndoRefocusHelper;

    @Before
    public void setUp() throws TimeoutException {
        MockitoAnnotations.initMocks(this);
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorder);
        Mockito.when(mTabModelSelector.getCurrentModel()).thenReturn(mModel);
        Mockito.when(mTabModelSelector.getModel(false)).thenReturn(mModel);

        mUndoRefocusHelper =
                new UndoRefocusHelper(mTabModelSelector, mLayoutManagerObservableSupplier, true);
    }

    private void initializeTabModel(int selectedIndex) {
        for (int i = 0; i < 5; i++) {
            mModel.addTab("Tab" + i);
        }

        mModel.setIndex(selectedIndex);
    }

    @After
    public void tearDown() {}

    @Test
    @Feature("Tab Strip Improvements")
    public void testUndoTabClose_SelectedTab_ReselectsTab() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();

        // Act: Close fourth tab (selected) and undo closed tab.
        Tab tab = getMockedTab(3);
        tabModelSelectorTabModelObserver.willCloseTab(tab, false);
        // When the fourth tab is closed, the third one should be selected.
        mModel.setIndex(2);
        // Undo 4th tab closure.
        tabModelSelectorTabModelObserver.tabClosureUndone(tab);

        // Assert: Fourth tab is selected after undo.
        assertEquals(3, mModel.index());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testUndoTabClose_UnSelectedTab_DoesNotSelectTab() {
        // Arrange: Initialize tabs with third tab selected.
        initializeTabModel(2);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();

        // Act: Close fourth tab (not selected) and undo closed tab.
        Tab tab = getMockedTab(3);
        tabModelSelectorTabModelObserver.willCloseTab(tab, false);
        // When the fourth tab is closed, the third one should be selected.
        mModel.setIndex(2);
        // Undo 4th tab closure.
        tabModelSelectorTabModelObserver.tabClosureUndone(tab);

        // Assert: Fourth tab is not selected after undo.
        assertNotEquals(3, mModel.index());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testUndoAllTabsClose_ReselectsSelectedTab() {
        // Arrange: Start with fourth tab as selected index.
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();

        // Act: Close all tabs and undo closure.
        tabModelSelectorTabModelObserver.willCloseAllTabs(false);
        cancelAllTabClosure(tabModelSelectorTabModelObserver);
        // Finalize closure cancellation completion.
        tabModelSelectorTabModelObserver.allTabsClosureUndone();

        // Assert: Fourth tab is selected after undo.
        assertEquals(3, mModel.index());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testUndoAllTabsClosed_ThenUndoSingleTabClose_ReselectsTab() {
        // Arrange: Start with fourth tab as selected index.
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();
        Tab tab = getMockedTab(3);

        // Act: Close all tabs and undo closure.
        tabModelSelectorTabModelObserver.willCloseAllTabs(false);
        cancelAllTabClosure(tabModelSelectorTabModelObserver);
        // Finalize closure cancellation completion.
        tabModelSelectorTabModelObserver.allTabsClosureUndone();

        // Assert: Fourth tab is selected after undo.
        assertEquals(3, mModel.index());

        // Act 2: Close just the fourth tab and undo.
        tabModelSelectorTabModelObserver.willCloseTab(tab, false);
        // After fourth tab is closed, the third one should be selected.
        mModel.setIndex(2);
        // Undo tab closure.
        tabModelSelectorTabModelObserver.tabClosureUndone(tab);

        // Assert: Third tab is selected after undo.
        assertEquals(3, mModel.index());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testUndoSingleTabClose_ThenUndoAllTabsClosed_ReselectsTab() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();
        Tab tab = getMockedTab(3);

        // Act 1: Close just the fourth tab and undo.
        tabModelSelectorTabModelObserver.willCloseTab(tab, false);
        // After fourth tab is closed, the third one should be selected.
        mModel.setIndex(2);
        // Undo tab closure.
        tabModelSelectorTabModelObserver.tabClosureUndone(tab);

        // Assert: Fourth tab is selected after undo.
        assertEquals(3, mModel.index());

        // Act 2: Close all tabs and undo closure
        tabModelSelectorTabModelObserver.willCloseAllTabs(false);
        cancelAllTabClosure(tabModelSelectorTabModelObserver);
        // Finalize closure cancellation completion.
        tabModelSelectorTabModelObserver.allTabsClosureUndone();

        // Assert: Fourth tab is selected after undo.
        assertEquals(mModel.index(), 3);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testUndoSingleTabClose_AfterManualTabReselection_DoesNotReselectTab() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();
        Tab tab = getMockedTab(3);
        Tab secondTab = getMockedTab(1);

        // Act 1: Close just the fourth tab and undo.
        tabModelSelectorTabModelObserver.willCloseTab(tab, false);
        // After fourth tab is closed, the third one should be selected.
        mModel.setIndex(2);

        // User manually selects the second tab before undoing the tab closure.
        mockClickTab(secondTab, tabModelSelectorTabModelObserver, tab.getId());
        // Undo tab closure.
        tabModelSelectorTabModelObserver.tabClosureUndone(tab);

        // Assert: Second tab is still selected after undo.
        assertEquals(1, mModel.index());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testUndoSingleTabClose_AfterClosingSelectedTabs_ReselectsMostRecentlyClosedTab() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();

        // Act 1: Close the fourth tab.
        Tab fourthTab = getMockedTab(3);
        tabModelSelectorTabModelObserver.willCloseTab(fourthTab, false);
        // After fourth tab is closed, the third one should be selected.
        mModel.setIndex(2);

        // Act 2: Close the third tab after it is selected.
        Tab thirdTab = getMockedTab(2);
        tabModelSelectorTabModelObserver.willCloseTab(thirdTab, false);
        // After third tab is closed, the second one should be selected.
        mModel.setIndex(1);

        // Undo tab closures.
        tabModelSelectorTabModelObserver.tabClosureUndone(thirdTab);
        tabModelSelectorTabModelObserver.tabClosureUndone(fourthTab);

        // Assert: Third tab is still selected after undo instead of the fourth tab.
        assertEquals(2, mModel.index());
    }

    @Test
    public void testUndoTabClose_TabStrip_RecordsUserAction() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();
        Tab tab = getMockedTab(3);

        // Act: Close tab and undo closed tab.
        tabModelSelectorTabModelObserver.willCloseTab(tab, false);
        tabModelSelectorTabModelObserver.tabClosureUndone(tab);

        // Assert: User action is recorded.
        Mockito.verify(mUmaRecorder)
                .recordUserAction(Mockito.eq(UNDO_CLOSE_TAB_USER_ACTION), Mockito.anyLong());
    }

    @Test
    public void testUndoTabClose_TabSwitcher_DoesNotRecordUserAction() {
        // Arrange: Start with fourth tab as selected index and tab switcher showing.
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();
        Tab tab = getMockedTab(3);
        mUndoRefocusHelper.setTabSwitcherVisibilityForTests(true);

        // Act: Close tab and undo closed tab.
        tabModelSelectorTabModelObserver.willCloseTab(tab, false);
        tabModelSelectorTabModelObserver.tabClosureUndone(tab);

        // Assert: User action is not recorded.
        Mockito.verify(mUmaRecorder, Mockito.never())
                .recordUserAction(Mockito.eq(UNDO_CLOSE_TAB_USER_ACTION), Mockito.anyLong());
    }

    @Test
    public void testUndoTabClose_TabSwitcherAndTabStrip_RecordsUserAction() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();
        Tab tab = getMockedTab(3);
        Tab secondTab = getMockedTab(3);

        // Act: Close 2 tabs and undo, one with tab switcher open.
        mUndoRefocusHelper.setTabSwitcherVisibilityForTests(true);
        tabModelSelectorTabModelObserver.willCloseTab(tab, false);
        mUndoRefocusHelper.setTabSwitcherVisibilityForTests(false);
        tabModelSelectorTabModelObserver.willCloseTab(secondTab, false);

        tabModelSelectorTabModelObserver.tabClosureUndone(secondTab);
        tabModelSelectorTabModelObserver.tabClosureUndone(tab);

        // Assert: User action is recorded exactly once.
        Mockito.verify(mUmaRecorder, Mockito.times(1))
                .recordUserAction(Mockito.eq(UNDO_CLOSE_TAB_USER_ACTION), Mockito.anyLong());
    }

    @Test
    public void testUndoAllTabClose_RecordsSingleUserAction() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelSelectorTabModelObserver tabModelSelectorTabModelObserver =
                mUndoRefocusHelper.getTabModelSelectorTabModelObserverForTests();

        // Act: Close all tabs and undo.
        tabModelSelectorTabModelObserver.willCloseAllTabs(false);
        cancelAllTabClosure(tabModelSelectorTabModelObserver);
        tabModelSelectorTabModelObserver.allTabsClosureUndone();

        // Assert: User action is recorded exactly once.
        Mockito.verify(mUmaRecorder, Mockito.times(1))
                .recordUserAction(Mockito.eq(UNDO_CLOSE_TAB_USER_ACTION), Mockito.anyLong());
    }

    @Test
    @SmallTest
    public void testOnDestroy() {
        LayoutManagerImpl layoutManager = Mockito.mock(LayoutManagerImpl.class);
        mUndoRefocusHelper.setLayoutManagerForTesting(layoutManager);

        // Act
        mUndoRefocusHelper.onDestroy();

        // Assert
        Callback<LayoutManagerImpl> supplierCallback =
                mUndoRefocusHelper.getLayoutManagerSupplierCallbackForTests();
        Mockito.verify(mLayoutManagerObservableSupplier).removeObserver(supplierCallback);
        Mockito.verify(mTabModelSelector).removeObserver(Mockito.any());
        Mockito.verify(layoutManager).removeObserver(Mockito.any());
    }

    private Tab getMockedTab(int id) {
        Tab tab1 = Mockito.mock(Tab.class);
        Mockito.when(tab1.isIncognito()).thenReturn(false);
        Mockito.when(tab1.getId()).thenReturn(id);

        return tab1;
    }

    private void cancelAllTabClosure(TabModelSelectorTabModelObserver modelSelectorModelObserver) {
        List<Tab> tabs = mModel.getAllTabs();
        for (int i = 0; i < tabs.size(); i++) {
            modelSelectorModelObserver.tabClosureUndone(tabs.get(i));
        }
    }

    private void mockClickTab(
            Tab tab, TabModelSelectorTabModelObserver modelSelectorModelObserver, int prevId) {
        mModel.setIndex(tab.getId());
        modelSelectorModelObserver.didSelectTab(tab, TabSelectionType.FROM_USER, prevId);
    }
}
