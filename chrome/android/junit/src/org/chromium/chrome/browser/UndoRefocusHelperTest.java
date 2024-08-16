// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.TestTabModel;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Test suite to verify that the TabUsageTracker correctly records the number of tabs used and the
 * percentage of tabs used.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UndoRefocusHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock TabModelSelector mTabModelSelector;
    @Mock LayoutManagerImpl mLayoutManagerImpl;
    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor ArgumentCaptor<LayoutStateObserver> mLayoutStateObserverCaptor;

    private static final String UNDO_CLOSE_TAB_USER_ACTION = "TabletTabStrip.UndoCloseTab";
    private final ObservableSupplierImpl<LayoutManagerImpl> mLayoutManagerObservableSupplier =
            new ObservableSupplierImpl<>();
    private TestTabModel mTabModel;
    private Tab mTab0;
    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;

    private UserActionTester mUserActionTester;
    private UndoRefocusHelper mUndoRefocusHelper;

    @Before
    public void setUp() throws TimeoutException {
        mTabModel = spy(new TestTabModel());
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModels()).thenReturn(List.of(mTabModel));
        when(mLayoutManagerImpl.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        mLayoutManagerObservableSupplier.set(mLayoutManagerImpl);

        mTab0 = getMockedTab(0);
        mTab1 = getMockedTab(1);
        mTab2 = getMockedTab(2);
        mTab3 = getMockedTab(3);

        mUserActionTester = new UserActionTester();

        mUndoRefocusHelper =
                new UndoRefocusHelper(mTabModelSelector, mLayoutManagerObservableSupplier, true);
        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        ShadowLooper.runUiThreadTasks();
        verify(mLayoutManagerImpl).addObserver(mLayoutStateObserverCaptor.capture());

        when(mLayoutManagerImpl.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        mLayoutStateObserverCaptor.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
    }

    private void initializeTabModel(int selectedIndex) {
        for (int i = 0; i < 5; i++) {
            mTabModel.addTab("Tab" + i);
        }

        mTabModel.setIndex(selectedIndex);
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    public void testUndoSingleTabClose_SelectedTab_ReSelectsTab() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        // Act: Close fourth tab (selected) and undo closed tab.
        Tab tab = getMockedTab(3);
        tabModelObserver.willCloseTab(tab, true);
        // When the fourth tab is closed, the third one should be selected.
        mTabModel.setIndex(2);
        // Undo 4th tab closure.
        tabModelObserver.tabClosureUndone(tab);

        // Assert: Fourth tab is selected after undo.
        assertEquals(3, mTabModel.index());
    }

    @Test
    public void testUndoSingleTabClose_UnSelectedTab_DoesNotSelectTab() {
        // Arrange: Initialize tabs with third tab selected.
        initializeTabModel(2);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        // Act: Close fourth tab (not selected) and undo closed tab.
        Tab tab = getMockedTab(3);
        tabModelObserver.willCloseTab(tab, true);
        // When the fourth tab is closed, the third one should be selected.
        mTabModel.setIndex(2);
        // Undo 4th tab closure.
        tabModelObserver.tabClosureUndone(tab);

        // Assert: Fourth tab is not selected after undo.
        assertNotEquals(3, mTabModel.index());
    }

    @Test
    public void testUndoMultipleSingleTabsClosed_ThenUndoSingleTabClose_ReSelectsTab() {
        // Arrange: Start with fourth tab as selected index.
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        // Act: Close multiple tabs one after the other including selected tab and undo closure
        // once.
        tabModelObserver.willCloseTab(mTab3, true);
        // tab2 is selected after tab3 is closed.
        mTabModel.setIndex(2);
        tabModelObserver.willCloseTab(mTab2, true);

        // Last closure (mTab3) is undone
        tabModelObserver.tabClosureUndone(mTab2);

        // Assert: mTab3 tab is selected after undo.
        assertEquals(mTab2.getId(), mTabModel.getTabAt(mTabModel.index()).getId());
    }

    @Test
    public void testUndoSingleTabClose_ThenUndoMultipleTabsClosed_ReSelectsTab() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();
        Tab tab = getMockedTab(3);

        // Act 1: Close just the fourth tab and undo.
        tabModelObserver.willCloseTab(tab, true);
        // After fourth tab is closed, the third one should be selected.
        mTabModel.setIndex(2);
        // Undo tab closure.
        tabModelObserver.tabClosureUndone(tab);

        // Assert: Fourth tab is selected after undo.
        assertEquals(3, mTabModel.index());

        // Act 2: Close multiple tabs and undo closure
        List<Tab> multipleTabs = Arrays.asList(mTab2, mTab3);
        tabModelObserver.willCloseMultipleTabs(true, multipleTabs);
        cancelTabsClosure(tabModelObserver, multipleTabs);
        // Finalize closure cancellation completion.
        tabModelObserver.allTabsClosureUndone();

        // Assert: Fourth tab is selected after undo.
        assertEquals(mTabModel.index(), 3);
    }

    @Test
    public void testUndoSingleTabClose_AfterManualTabReselection_DoesNotReselectTab() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();
        Tab tab = getMockedTab(3);
        Tab secondTab = getMockedTab(1);

        // Act 1: Close just the fourth tab and undo.
        tabModelObserver.willCloseTab(tab, true);
        // After fourth tab is closed, the third one should be selected.
        mTabModel.setIndex(2);

        // User manually selects the second tab before undoing the tab closure.
        mockClickTab(secondTab, tabModelObserver, tab.getId());
        // Undo tab closure.
        tabModelObserver.tabClosureUndone(tab);

        // Assert: Second tab is still selected after undo.
        assertEquals(1, mTabModel.index());
    }

    @Test
    public void testUndoSingleTabClose_AfterClosingSelectedTabs_ReselectsMostRecentlyClosedTab() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        // Act 1: Close the fourth tab.
        Tab fourthTab = getMockedTab(3);
        tabModelObserver.willCloseTab(fourthTab, true);
        // After fourth tab is closed, the third one should be selected.
        mTabModel.setIndex(2);

        // Act 2: Close the third tab after it is selected.
        Tab thirdTab = getMockedTab(2);
        tabModelObserver.willCloseTab(thirdTab, true);
        // After third tab is closed, the second one should be selected.
        mTabModel.setIndex(1);

        // Undo tab closures.
        tabModelObserver.tabClosureUndone(thirdTab);
        tabModelObserver.tabClosureUndone(fourthTab);

        // Assert: Third tab is still selected after undo instead of the fourth tab.
        assertEquals(2, mTabModel.index());
    }

    @Test
    public void testUndoTabClose_TabStrip_RecordsUserAction() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();
        Tab tab = getMockedTab(3);

        // Act: Close tab and undo closed tab.
        tabModelObserver.willCloseTab(tab, true);
        tabModelObserver.tabClosureUndone(tab);

        // Assert: User action is recorded.
        assertEquals(1, mUserActionTester.getActionCount(UNDO_CLOSE_TAB_USER_ACTION));
    }

    @Test
    public void testUndoTabClose_TabSwitcher_DoesNotRecordUserAction() {
        // Arrange: Start with fourth tab as selected index and tab switcher showing.
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();
        Tab tab = getMockedTab(3);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);

        // Act: Close tab and undo closed tab.
        tabModelObserver.willCloseTab(tab, true);
        tabModelObserver.tabClosureUndone(tab);

        // Assert: User action is not recorded.
        assertEquals(0, mUserActionTester.getActionCount(UNDO_CLOSE_TAB_USER_ACTION));
    }

    @Test
    public void testUndoTabClose_TabSwitcherAndTabStrip_RecordsUserAction() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();
        Tab tab = getMockedTab(3);
        Tab secondTab = getMockedTab(3);

        // Act: Close 2 tabs and undo, one with tab switcher open.
        LayoutStateObserver layoutStateObserver = mLayoutStateObserverCaptor.getValue();
        layoutStateObserver.onFinishedShowing(LayoutType.TAB_SWITCHER);
        tabModelObserver.willCloseTab(tab, true);
        layoutStateObserver.onFinishedHiding(LayoutType.TAB_SWITCHER);
        tabModelObserver.willCloseTab(secondTab, true);

        tabModelObserver.tabClosureUndone(secondTab);
        tabModelObserver.tabClosureUndone(tab);

        // Assert: User action is recorded exactly once.
        assertEquals(1, mUserActionTester.getActionCount(UNDO_CLOSE_TAB_USER_ACTION));
    }

    @Test
    public void testUndoMultipleTabsClosedTogether_ReSelectsSelectedTab() {
        // Arrange: Start with fourth tab as selected index.
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        // Act: Close multiple tabs including selected tab and undo closure.
        List<Tab> tabsToClose = Arrays.asList(mTab2, mTab3);
        tabModelObserver.willCloseMultipleTabs(true, tabsToClose);
        cancelTabsClosure(tabModelObserver, tabsToClose);
        // Finalize closure cancellation completion.
        tabModelObserver.allTabsClosureUndone();

        // Assert: Fourth tab is selected after undo.
        assertEquals(3, mTabModel.index());
    }

    @Test
    public void testUndoManyMultipleTabsClosedTogether_ReSelectsSelectedTab() {
        // Arrange: Start with fourth tab as selected index.
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        // Act: Close first set multiple tabs including selected tabs.
        List<Tab> tabsToClose1 = Arrays.asList(mTab2, mTab3);
        tabModelObserver.willCloseMultipleTabs(true, tabsToClose1);
        // Set mTab1 as newly selected tab.
        mTabModel.setIndex(1);
        // Act: Close second set multiple tabs including selected tabs.
        List<Tab> tabsToClose2 = Arrays.asList(mTab0, mTab1);
        tabModelObserver.willCloseMultipleTabs(true, tabsToClose2);

        cancelTabsClosure(tabModelObserver, tabsToClose2);
        cancelTabsClosure(tabModelObserver, tabsToClose1);

        // Finalize closure cancellation completion.
        tabModelObserver.allTabsClosureUndone();

        // Assert: mTab1 tab is selected after undo.
        assertEquals(1, mTabModel.index());
    }

    @Test
    public void testUndoMultipleTabClose_RecordsUserAction() {
        // Arrange: Start with fourth tab as selected index
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();
        // Act: Close multiple tabs and undo.
        List<Tab> multipleTabs = Arrays.asList(mTab2, mTab3);
        tabModelObserver.willCloseMultipleTabs(true, multipleTabs);
        cancelTabsClosure(tabModelObserver, multipleTabs);
        tabModelObserver.allTabsClosureUndone();

        // Assert: User action is recorded exactly once.
        assertEquals(1, mUserActionTester.getActionCount(UNDO_CLOSE_TAB_USER_ACTION));
    }

    @Test
    public void testUndoAllTabsClosedTogether_ReSelectsSelectedTab() {
        // Arrange: Start with fourth tab as selected index.
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        // Act: Close all tabs and undo closure.
        tabModelObserver.willCloseAllTabs(false);
        cancelTabsClosure(tabModelObserver, Arrays.asList(mTab0, mTab1, mTab2, mTab3));
        // Finalize closure cancellation completion.
        tabModelObserver.allTabsClosureUndone();

        // Assert: Fourth tab is selected after undo.
        assertEquals(3, mTabModel.index());
    }

    @Test
    public void testUndoAllTabsClosedTogether_RecordUserAction() {
        // Arrange: Start with fourth tab as selected index.
        initializeTabModel(3);
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        // Act: Close all tabs and undo closure.
        tabModelObserver.willCloseAllTabs(false);
        cancelTabsClosure(tabModelObserver, Arrays.asList(mTab0, mTab1, mTab2, mTab3));
        // Finalize closure cancellation completion.
        tabModelObserver.allTabsClosureUndone();

        // Assert: User action is recorded exactly once.
        assertEquals(1, mUserActionTester.getActionCount(UNDO_CLOSE_TAB_USER_ACTION));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        // Act
        mUndoRefocusHelper.destroy();

        // Assert
        assertFalse(mLayoutManagerObservableSupplier.hasObservers());
        verify(mTabModel).removeObserver(mTabModelObserverCaptor.getValue());
        verify(mLayoutManagerImpl).removeObserver(mLayoutStateObserverCaptor.getValue());
    }

    private Tab getMockedTab(int id) {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.isIncognito()).thenReturn(false);
        when(tab.getId()).thenReturn(id);
        when(mTabModelSelector.getModelForTabId(id)).thenReturn(mTabModel);

        return tab;
    }

    private void cancelTabsClosure(TabModelObserver tabModelObserver, List<Tab> tabsToUndo) {
        for (int i = 0; i < tabsToUndo.size(); i++) {
            tabModelObserver.tabClosureUndone(tabsToUndo.get(i));
        }
    }

    private void mockClickTab(Tab tab, TabModelObserver tabModelObserver, int prevId) {
        mTabModel.setIndex(tab.getId());
        tabModelObserver.didSelectTab(tab, TabSelectionType.FROM_USER, prevId);
    }
}
