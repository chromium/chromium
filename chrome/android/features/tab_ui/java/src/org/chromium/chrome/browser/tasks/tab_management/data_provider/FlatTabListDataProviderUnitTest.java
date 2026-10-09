// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabListDataObserver.PayloadType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Predicate;

/** Unit tests for {@link FlatTabListDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FlatTabListDataProviderUnitTest {
    private static final @TabId int TAB1_ID = 1;
    private static final @TabId int TAB2_ID = 2;
    private static final @TabId int TAB3_ID = 3;
    private static final @TabId int TAB4_ID = 4;
    private static final Token TAB_GROUP_ID = new Token(/* high= */ 1L, /* low= */ 2L);
    private static final Token TAB_GROUP_ID_2 = new Token(/* high= */ 3L, /* low= */ 4L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabModel mOtherTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mTab4;
    @Mock private TabListDataObserver mObserver;
    @Mock private TabListDataObserver mSecondObserver;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<TabGroupObserver> mTabGroupObserverCaptor;

    private final List<Tab> mModelTabs = new ArrayList<>();
    private final List<Tab> mOtherModelTabs = new ArrayList<>();
    private final Predicate<Tab> mInCurrentGroupFilter = this::isInCurrentGroup;
    private final SettableNullableObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createNullable();

    private @Nullable Token mCurrentTabGroupId = TAB_GROUP_ID;
    private @Nullable Tab mSelectedTab;
    private TabModelObserver mTabModelObserver;
    private TabGroupObserver mTabGroupObserver;
    private FlatTabListDataProvider mProvider;

    @Before
    public void setUp() {
        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab2.getId()).thenReturn(TAB2_ID);
        when(mTab3.getId()).thenReturn(TAB3_ID);
        when(mTab4.getId()).thenReturn(TAB4_ID);

        stubBackedTabModel(mTabModel, mModelTabs);
        stubBackedTabModel(mOtherTabModel, mOtherModelTabs);
    }

    // ============================================================================================
    // Value Semantics Tests
    // ============================================================================================

    @Test
    public void testTabItem_ValueSemantics() {
        TabItem base = item(TAB1_ID);
        assertEquals(TAB1_ID, base.getTabId());
        assertFalse(base.isSelected());
        assertFalse(base.isPinned());
        assertFalse(base.isMultiSelected());

        TabItem selectedItem = selected(TAB1_ID);
        assertTrue(selectedItem.isSelected());

        TabItem pinnedItem = pinned(TAB1_ID);
        assertTrue(pinnedItem.isPinned());

        TabItem multiSelectedItem =
                new TabItem(
                        TAB1_ID,
                        /* isSelected= */ false,
                        /* isPinned= */ false,
                        /* isMultiSelected= */ true);
        assertTrue(multiSelectedItem.isMultiSelected());

        TabItem duplicate = item(TAB1_ID);
        assertEquals(base, duplicate);
        assertEquals(base.hashCode(), duplicate.hashCode());
        assertNotEquals(base, item(TAB2_ID));
        assertNotEquals(base, selectedItem);
        assertNotEquals(base, pinnedItem);
        assertNotEquals(base, multiSelectedItem);

        TabItem updatedSelected = base.withSelected(/* isSelected= */ true);
        assertTrue(updatedSelected.isSelected());
        assertEquals(selectedItem, updatedSelected);
        assertSame(base, base.withSelected(/* isSelected= */ false));
    }

    // ============================================================================================
    // Lifecycle & Reset Tests
    // ============================================================================================

    @Test
    public void testRequestDataReset_NoTabModel_EmitsEmptyReset() {
        createProvider(/* filter= */ null);
        mProvider.requestDataReset();

        assertResetWithItems();
    }

    @Test
    public void testInitialState_NullFilter_IncludesAllTabsInModelOrderWithState() {
        when(mTab1.getIsPinned()).thenReturn(true);
        groupTabs(TAB_GROUP_ID, mTab2);
        mSelectedTab = mTab2;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);

        assertItems(pinned(TAB1_ID), selected(TAB2_ID), item(TAB3_ID));
    }

    @Test
    public void testInitialState_WithFilter_IncludesOnlyMatchingTabs() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        groupTabs(TAB_GROUP_ID_2, mTab4);
        // Selected tab is outside the filter, so no projected item is marked selected.
        mSelectedTab = mTab1;
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3, mTab4);

        assertItems(item(TAB2_ID), item(TAB3_ID));
    }

    @Test
    public void testRestoreCompleted_PopulatesItemsAfterRestore() {
        when(mTabModel.isTabStateInitialized()).thenReturn(false);
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems();

        when(mTabModel.isTabStateInitialized()).thenReturn(true);
        mTabModelObserver.restoreCompleted();

        assertResetWithItems(selected(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testRequestDataReset_CurrentGroupChanges_ReprojectsForNewGroup() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        groupTabs(TAB_GROUP_ID_2, mTab4);
        mSelectedTab = mTab2;
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3, mTab4);
        assertItems(selected(TAB2_ID), item(TAB3_ID));

        // Switch active group to TAB_GROUP_ID_2 and reset.
        mCurrentTabGroupId = TAB_GROUP_ID_2;
        mSelectedTab = mTab4;
        mProvider.requestDataReset();

        assertResetWithItems(selected(TAB4_ID));
    }

    @Test
    public void testSupplierSwitchingAndStop_DetachesOnSwitchOrStopAndReattachesOnReset() {
        setModelTabs(mTab1, mTab2);
        mTabModelSupplier.set(mTabModel);
        createProvider(/* filter= */ null);
        // Provider does not attach to TabModel until requestDataReset() is called.
        verify(mTabModel, never()).addObserver(any());
        assertItems();

        // First requestDataReset() attaches to mTabModel and populates items.
        mProvider.requestDataReset();
        captureObserver(mTabModel);
        TabModelObserver firstModelObserver = mTabModelObserver;
        TabGroupObserver firstGroupObserver = mTabGroupObserver;
        assertResetWithItems(item(TAB1_ID), item(TAB2_ID));

        // Switching to mOtherTabModel detaches from mTabModel and waits for the next
        // requestDataReset() before attaching to mOtherTabModel.
        clearInvocations(mObserver);
        mOtherModelTabs.add(mTab3);
        mTabModelSupplier.set(mOtherTabModel);
        verify(mTabModel).removeObserver(firstModelObserver);
        verify(mTabModel).removeTabGroupObserver(firstGroupObserver);
        verify(mOtherTabModel, never()).addObserver(any());
        verify(mOtherTabModel, never()).addTabGroupObserver(any());
        verifyNoInteractions(mObserver);
        assertItems();

        // Next requestDataReset() attaches to mOtherTabModel and populates its items.
        mProvider.requestDataReset();
        captureObserver(mOtherTabModel);
        assertResetWithItems(item(TAB3_ID));

        // stop() detaches from mOtherTabModel and clears items while hidden, and a subsequent
        // requestDataReset() re-attaches to the same model when the surface re-opens.
        clearInvocations(mObserver);
        mProvider.stop();
        verify(mOtherTabModel).removeObserver(mTabModelObserver);
        verify(mOtherTabModel).removeTabGroupObserver(mTabGroupObserver);
        verifyNoInteractions(mObserver);
        assertItems();

        mProvider.requestDataReset();
        verify(mOtherTabModel, times(2)).addObserver(mTabModelObserver);
        verify(mOtherTabModel, times(2)).addTabGroupObserver(mTabGroupObserver);
        assertResetWithItems(item(TAB3_ID));
    }

    @Test
    public void testObserversAndDestroy_ManagesNotificationsAndDetaches() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        // Exercise default no-op TabListDataObserver methods alongside a second mock observer.
        TabListDataObserver defaultObserver = new TabListDataObserver() {};
        mProvider.addObserver(defaultObserver);
        mProvider.addObserver(mSecondObserver);

        // Repeated requestDataReset() on the same TabModel does not re-register observers.
        mProvider.requestDataReset();
        List<TabListItem> expected = List.of(item(TAB1_ID), item(TAB2_ID));
        verify(mTabModel, times(1)).addObserver(mTabModelObserver);
        verify(mTabModel, times(1)).addTabGroupObserver(mTabGroupObserver);
        verify(mObserver).onDataReset(expected);
        verify(mSecondObserver).onDataReset(expected);

        // Removing an observer stops further notifications to that observer only.
        clearInvocations(mObserver, mSecondObserver);
        mProvider.removeObserver(mObserver);
        mProvider.requestDataReset();
        verifyNoInteractions(mObserver);
        verify(mSecondObserver).onDataReset(expected);

        // Destroy detaches from TabModel, clears items, and stops notifying observers.
        clearInvocations(mSecondObserver);
        mProvider.destroy();
        verify(mTabModel).removeObserver(mTabModelObserver);
        verify(mTabModel).removeTabGroupObserver(mTabGroupObserver);
        assertItems();

        mProvider.requestDataReset();
        verifyNoInteractions(mSecondObserver);
    }

    // ============================================================================================
    // TabModelObserver: didAddTab
    // ============================================================================================

    @Test
    public void testDidAddTab_InsertsAtHeadMiddleAndEnd_EmitsPredecessorAnchor() {
        setUpProviderWithTabs(/* filter= */ null, mTab2);

        // Insert at head (index 0) -> after is null.
        addTab(mTab1, /* modelIndex= */ 0);
        verify(mObserver).onItemsInserted(List.of(item(TAB1_ID)), /* after= */ null);
        assertItems(item(TAB1_ID), item(TAB2_ID));

        // Append at end (index 2) -> after is mTab2.
        addTab(mTab4, /* modelIndex= */ 2);
        verify(mObserver).onItemsInserted(List.of(item(TAB4_ID)), item(TAB2_ID));
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB4_ID));

        // Insert in middle (index 2, between mTab2 and mTab4) -> after is mTab2.
        addTab(mTab3, /* modelIndex= */ 2);
        verify(mObserver).onItemsInserted(List.of(item(TAB3_ID)), item(TAB2_ID));
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID), item(TAB4_ID));
    }

    @Test
    public void testDidAddTab_WithFilter_SkipsFilteredOutTabsAndComputesVisibleAnchor() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab4);
        // mTab1 is ungrouped (filtered out), so only mTab2 is initially projected.
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        assertItems(item(TAB2_ID));

        // Adding ungrouped mTab3 is ignored because it does not match the filter.
        addTab(mTab3, /* modelIndex= */ 2);
        verifyNoInteractions(mObserver);
        assertItems(item(TAB2_ID));

        // Adding grouped mTab4 after filtered-out mTab3 skips mTab1 and mTab3 and anchors after
        // mTab2.
        mSelectedTab = mTab4;
        addTab(mTab4, /* modelIndex= */ 3);
        verify(mObserver).onItemsInserted(List.of(selected(TAB4_ID)), item(TAB2_ID));
        assertItems(item(TAB2_ID), selected(TAB4_ID));
    }

    @Test
    public void testDidAddTab_IgnoresUnrestoredDuplicateOrAbsentTab() {
        when(mTabModel.isTabStateInitialized()).thenReturn(false);
        setUpProviderWithTabs(/* filter= */ null);

        // Ignored while TabModel is not yet restored.
        addTab(mTab1, /* modelIndex= */ 0);
        verifyNoInteractions(mObserver);
        assertItems();

        when(mTabModel.isTabStateInitialized()).thenReturn(true);
        mTabModelObserver.restoreCompleted();
        assertResetWithItems(item(TAB1_ID));
        clearInvocations(mObserver);

        // Ignored if the tab is already present in mItems.
        mTabModelObserver.didAddTab(
                mTab1,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ false);
        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));

        // Ignored if the tab is not present in TabModel.
        mTabModelObserver.didAddTab(
                mTab2,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ false);
        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    // ============================================================================================
    // TabModelObserver: didMoveTab
    // ============================================================================================

    @Test
    public void testDidMoveTab_StandaloneTab_MovesDown() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // Moving mTab1 down after mTab3 emits onItemsMoved with after=mTab3.
        moveTab(mTab1, /* newIndex= */ 2);

        verify(mObserver).onItemsMoved(List.of(item(TAB1_ID)), /* after= */ item(TAB3_ID));
        assertItems(item(TAB2_ID), item(TAB3_ID), item(TAB1_ID));
    }

    @Test
    public void testDidMoveTab_StandaloneTab_MovesUp() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // Moving mTab3 up to the start emits onItemsMoved with after=null.
        moveTab(mTab3, /* newIndex= */ 0);

        verify(mObserver).onItemsMoved(List.of(item(TAB3_ID)), /* after= */ null);
        assertItems(item(TAB3_ID), item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidMoveTab_GroupedTab_Ignored() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // didMoveTab ignores grouped tabs (handled by didMoveWithinGroup).
        moveTab(mTab1, /* newIndex= */ 2);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));
    }

    @Test
    public void testDidMoveTab_FilteredOutTab_NoOps() {
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3);
        assertItems();

        // Moving a tab that does not satisfy the filter is ignored.
        moveTab(mTab1, /* newIndex= */ 2);

        verifyNoInteractions(mObserver);
        assertItems();
    }

    @Test
    public void testDidMoveTab_SamePosition_NoOps() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // Moving a tab to its current position no-ops.
        mTabModelObserver.didMoveTab(mTab1, /* newIndex= */ 0, /* curIndex= */ 0);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));
    }

    // ============================================================================================
    // TabModelObserver: didRemoveTabForClosure / tabRemoved
    // ============================================================================================

    @Test
    public void testDidRemoveTabForClosure_RemovesItem() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);

        closeTab(mTab2);

        verify(mObserver).onItemsRemoved(List.of(item(TAB2_ID)));
        assertItems(item(TAB1_ID), item(TAB3_ID));
    }

    @Test
    public void testTabRemoved_RemovesItem() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);

        mModelTabs.remove(mTab3);
        mTabModelObserver.tabRemoved(mTab3);

        verify(mObserver).onItemsRemoved(List.of(item(TAB3_ID)));
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidRemoveTabForClosure_UnprojectedTab_NoEvents() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);

        closeTab(mTab2);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    // ============================================================================================
    // TabModelObserver: tabClosureUndone / tabClosureCommitted
    // ============================================================================================

    @Test
    public void testTabClosureUndone_ReinsertsAtTabModelPositionAndNotifiesUndo() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        closeTab(mTab2);
        clearInvocations(mObserver);

        undoCloseTab(mTab2, /* modelIndex= */ 1);

        verify(mObserver).onItemsInserted(List.of(item(TAB2_ID)), item(TAB1_ID));
        verify(mObserver).onTabClosureUndone(TAB2_ID);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));
    }

    @Test
    public void testTabClosureUndone_SelectedTab_ItemIsSelected() {
        mSelectedTab = mTab2;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        selectTab(mTab3, TAB2_ID);
        closeTab(mTab2);
        clearInvocations(mObserver);

        // Backend restores the tab and re-selects it.
        mSelectedTab = mTab2;
        undoCloseTab(mTab2, /* modelIndex= */ 1);
        selectTab(mTab2, TAB3_ID);

        verify(mObserver).onItemsInserted(List.of(selected(TAB2_ID)), item(TAB1_ID));
        verify(mObserver).onTabClosureUndone(TAB2_ID);
        // Tab 2 was inserted already selected, so only the deselect of tab 3 is emitted.
        verify(mObserver).onItemUpdated(item(TAB3_ID), PayloadType.SELECTION);
        assertItems(item(TAB1_ID), selected(TAB2_ID), item(TAB3_ID));
    }

    @Test
    public void testTabClosureUndone_SelectedTab_RestoredAtIndexZero_PrevSelectedTabIdInvalid() {
        mSelectedTab = mTab2;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        closeTab(mTab1);
        clearInvocations(mObserver);

        // Backend restores tab 1 at index 0 and re-selects it with prevSelectedTabId =
        // INVALID_TAB_ID.
        mSelectedTab = mTab1;
        undoCloseTab(mTab1, /* modelIndex= */ 0);
        selectTab(mTab1, Tab.INVALID_TAB_ID);

        verify(mObserver).onItemsInserted(List.of(selected(TAB1_ID)), /* after= */ null);
        verify(mObserver).onTabClosureUndone(TAB1_ID);
        // Tab 1 was inserted already selected, so the previously selected tab 2 is deselected.
        verify(mObserver).onItemUpdated(item(TAB2_ID), PayloadType.SELECTION);
        assertItems(selected(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testTabClosureUndone_FilteredOut_NoInsert() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        closeTab(mTab2);
        clearInvocations(mObserver);

        undoCloseTab(mTab2, /* modelIndex= */ 1);

        verify(mObserver, never()).onItemsInserted(any(), any());
        verify(mObserver, never()).onTabClosureUndone(anyInt());
        assertItems(item(TAB1_ID));
    }

    @Test
    public void testTabClosureCommitted_ForwardsWithoutItemChanges() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        closeTab(mTab2);
        clearInvocations(mObserver);

        mTabModelObserver.tabClosureCommitted(mTab2);

        verify(mObserver).onTabClosureCommitted(TAB2_ID);
        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB3_ID));
    }

    @Test
    public void testTabClosureCommitted_FilteredOut_NoNotify() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        closeTab(mTab2);
        clearInvocations(mObserver);

        mTabModelObserver.tabClosureCommitted(mTab2);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    // ============================================================================================
    // TabModelObserver: didSelectTab
    // ============================================================================================

    @Test
    public void testDidSelectTab_SelectsNewTab_DeselectsPreviousTab() {
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems(selected(TAB1_ID), item(TAB2_ID));

        selectTab(mTab2, TAB1_ID);

        // Intentional two-step dispatch: deselected tab first, newly selected tab second.
        InOrder inOrder = inOrder(mObserver);
        inOrder.verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.SELECTION);
        inOrder.verify(mObserver).onItemUpdated(selected(TAB2_ID), PayloadType.SELECTION);
        assertItems(item(TAB1_ID), selected(TAB2_ID));
    }

    @Test
    public void testDidSelectTab_PreviousInvalidTabId_DeselectsCurrentlySelectedTab() {
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems(selected(TAB1_ID), item(TAB2_ID));

        // When a new tab is created, TabModel passes Tab.INVALID_TAB_ID as prevSelectedTabId.
        selectTab(mTab2, Tab.INVALID_TAB_ID);

        InOrder inOrder = inOrder(mObserver);
        inOrder.verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.SELECTION);
        inOrder.verify(mObserver).onItemUpdated(selected(TAB2_ID), PayloadType.SELECTION);
        assertItems(item(TAB1_ID), selected(TAB2_ID));
    }

    @Test
    public void testDidSelectTab_SelectingUnprojectedTab_DeselectsPreviousProjectedTab() {
        mSelectedTab = mTab1;
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        assertItems(selected(TAB1_ID));

        // Selecting unprojected mTab2 deselects mTab1 without emitting selection for mTab2.
        selectTab(mTab2, TAB1_ID);

        verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.SELECTION);
        verify(mObserver, never()).onItemUpdated(selected(TAB2_ID), PayloadType.SELECTION);
        assertItems(item(TAB1_ID));
    }

    @Test
    public void testDidSelectTab_SelectingProjectedTab_PreviousUnprojected_OnlySelectsNewTab() {
        mSelectedTab = mTab2;
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        assertItems(item(TAB1_ID));

        // Selecting projected mTab1 when previous was unprojected only updates mTab1.
        selectTab(mTab1, TAB2_ID);

        verify(mObserver).onItemUpdated(selected(TAB1_ID), PayloadType.SELECTION);
        verify(mObserver, times(1)).onItemUpdated(any(), anyInt());
        assertItems(selected(TAB1_ID));
    }

    @Test
    public void testDidSelectTab_SameTab_NoOps() {
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems(selected(TAB1_ID), item(TAB2_ID));

        selectTab(mTab1, TAB1_ID);

        verifyNoInteractions(mObserver);
        assertItems(selected(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidSelectTab_BothUnprojected_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID));

        // Selecting between two unprojected tabs emits no events and does not alter items.
        selectTab(mTab3, TAB2_ID);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    // ============================================================================================
    // TabModelObserver: didChangePinState
    // ============================================================================================

    @Test
    public void testDidChangePinState_UpdatesPinProperty() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems(item(TAB1_ID), item(TAB2_ID));

        when(mTab2.getIsPinned()).thenReturn(true);
        mTabModelObserver.didChangePinState(mTab2);

        verify(mObserver).onItemUpdated(pinned(TAB2_ID), PayloadType.PIN_STATE);
        assertItems(item(TAB1_ID), pinned(TAB2_ID));
    }

    @Test
    public void testDidChangePinState_Unpinned_UpdatesPinProperty() {
        when(mTab2.getIsPinned()).thenReturn(true);
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems(item(TAB1_ID), pinned(TAB2_ID));

        when(mTab2.getIsPinned()).thenReturn(false);
        mTabModelObserver.didChangePinState(mTab2);

        verify(mObserver).onItemUpdated(item(TAB2_ID), PayloadType.PIN_STATE);
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidChangePinState_FilteredOut_RemovesTab() {
        // Filter that only shows unpinned tabs.
        setUpProviderWithTabs(tab -> !tab.getIsPinned(), mTab1, mTab2);
        assertItems(item(TAB1_ID), item(TAB2_ID));

        when(mTab1.getIsPinned()).thenReturn(true);
        mTabModelObserver.didChangePinState(mTab1);

        verify(mObserver).onItemsRemoved(List.of(item(TAB1_ID)));
        assertItems(item(TAB2_ID));
    }

    @Test
    public void testDidChangePinState_Unpinned_FilteredOut_RemovesTab() {
        // Filter that only shows pinned tabs.
        when(mTab1.getIsPinned()).thenReturn(true);
        setUpProviderWithTabs(Tab::getIsPinned, mTab1, mTab2);
        assertItems(pinned(TAB1_ID));

        when(mTab1.getIsPinned()).thenReturn(false);
        mTabModelObserver.didChangePinState(mTab1);

        verify(mObserver).onItemsRemoved(List.of(pinned(TAB1_ID)));
        assertItems();
    }

    @Test
    public void testDidChangePinState_FilterBecameMatching_AddsTab() {
        // Filter that only shows pinned tabs.
        setUpProviderWithTabs(Tab::getIsPinned, mTab1);
        assertItems();

        when(mTab1.getIsPinned()).thenReturn(true);
        mTabModelObserver.didChangePinState(mTab1);

        verify(mObserver).onItemsInserted(List.of(pinned(TAB1_ID)), /* after= */ null);
        assertItems(pinned(TAB1_ID));
    }

    @Test
    public void testDidChangePinState_UnprojectedTab_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        assertItems(item(TAB1_ID));

        // Pin state change on unprojected mTab2 does nothing.
        when(mTab2.getIsPinned()).thenReturn(true);
        mTabModelObserver.didChangePinState(mTab2);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    // ============================================================================================
    // TabGroupObserver: didMergeTabToGroup
    // ============================================================================================

    @Test
    public void testDidMergeTabToGroup_MatchingGroup_InsertsTab() {
        groupTabs(TAB_GROUP_ID, mTab2);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        assertItems(item(TAB2_ID));

        // Merging mTab1 into the active group satisfies the filter and inserts the item.
        groupTabs(TAB_GROUP_ID, mTab1);
        mTabGroupObserver.didMergeTabToGroup(mTab1, /* isDestinationTab= */ false);

        verify(mObserver).onItemsInserted(List.of(item(TAB1_ID)), /* after= */ null);
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidMergeTabToGroup_DifferentGroup_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab2);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        assertItems(item(TAB2_ID));

        // Merging mTab1 into a different group does not satisfy the filter and emits no events.
        groupTabs(TAB_GROUP_ID_2, mTab1);
        mTabGroupObserver.didMergeTabToGroup(mTab1, /* isDestinationTab= */ false);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB2_ID));
    }

    @Test
    public void testDidMergeTabToGroup_AlreadyPresent_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        assertItems(item(TAB1_ID), item(TAB2_ID));

        // Re-merging an already-present tab no-ops.
        mTabGroupObserver.didMergeTabToGroup(mTab1, /* isDestinationTab= */ false);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    // ============================================================================================
    // TabGroupObserver: didMoveWithinGroup
    // ============================================================================================

    @Test
    public void testDidMoveWithinGroup_MovesDown() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2, mTab3);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // Moving mTab1 down after mTab3 emits onItemsMoved with after=mTab3.
        moveTabWithinGroup(mTab1, /* newIndex= */ 2);

        verify(mObserver).onItemsMoved(List.of(item(TAB1_ID)), /* after= */ item(TAB3_ID));
        assertItems(item(TAB2_ID), item(TAB3_ID), item(TAB1_ID));
    }

    @Test
    public void testDidMoveWithinGroup_MovesUp() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2, mTab3);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // Moving mTab3 up to the start emits onItemsMoved with after=null.
        moveTabWithinGroup(mTab3, /* newIndex= */ 0);

        verify(mObserver).onItemsMoved(List.of(item(TAB3_ID)), /* after= */ null);
        assertItems(item(TAB3_ID), item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidMoveWithinGroup_UnprojectedGroup_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1);
        groupTabs(TAB_GROUP_ID_2, mTab2, mTab3);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID));

        // Moving a tab in another group is ignored by this provider.
        moveTabWithinGroup(mTab2, /* newIndex= */ 2);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    @Test
    public void testDidMoveWithinGroup_SamePosition_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2, mTab3);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // Moving a tab to its current position no-ops.
        mTabGroupObserver.didMoveWithinGroup(
                mTab1, /* tabModelOldIndex= */ 0, /* tabModelNewIndex= */ 0);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));
    }

    // ============================================================================================
    // TabGroupObserver: didMoveTabOutOfGroup
    // ============================================================================================

    @Test
    public void testDidMoveTabOutOfGroup_FromMatchingGroup_RemovesTab() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2);
        assertItems(item(TAB1_ID), item(TAB2_ID));

        // Moving mTab1 out of the group makes it fail the filter and removes it.
        groupTabs(/* groupId= */ null, mTab1);
        mTabGroupObserver.didMoveTabOutOfGroup(mTab1, TAB_GROUP_ID);

        verify(mObserver).onItemsRemoved(List.of(item(TAB1_ID)));
        assertItems(item(TAB2_ID));
    }

    @Test
    public void testDidMoveTabOutOfGroup_FromDifferentGroup_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab2);
        groupTabs(TAB_GROUP_ID_2, mTab3);
        setUpProviderWithTabs(mInCurrentGroupFilter, mTab1, mTab2, mTab3);
        assertItems(item(TAB2_ID));

        // Moving mTab3 out of an unrelated group is ignored.
        groupTabs(/* groupId= */ null, mTab3);
        mTabGroupObserver.didMoveTabOutOfGroup(mTab3, TAB_GROUP_ID_2);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB2_ID));
    }

    // ============================================================================================
    // Test Helpers
    // ============================================================================================

    private void stubBackedTabModel(TabModel model, List<Tab> tabs) {
        when(model.isTabStateInitialized()).thenReturn(true);
        when(model.iterator()).thenAnswer(invocation -> tabs.iterator());
        when(model.getTabAt(anyInt()))
                .thenAnswer(
                        invocation -> {
                            int index = invocation.getArgument(0);
                            return index >= 0 && index < tabs.size() ? tabs.get(index) : null;
                        });
        when(model.index())
                .thenAnswer(
                        invocation -> {
                            if (mSelectedTab == null) return TabList.INVALID_TAB_INDEX;
                            int index = tabs.indexOf(mSelectedTab);
                            return index < 0 ? TabList.INVALID_TAB_INDEX : index;
                        });
    }

    private void captureObserver(TabModel model) {
        verify(model).addObserver(mTabModelObserverCaptor.capture());
        mTabModelObserver = mTabModelObserverCaptor.getValue();
        verify(model).addTabGroupObserver(mTabGroupObserverCaptor.capture());
        mTabGroupObserver = mTabGroupObserverCaptor.getValue();
    }

    private void setModelTabs(Tab... tabs) {
        mModelTabs.clear();
        mModelTabs.addAll(Arrays.asList(tabs));
    }

    private void addTab(Tab tab, int modelIndex) {
        mModelTabs.add(modelIndex, tab);
        mTabModelObserver.didAddTab(
                tab,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ false);
    }

    private void moveTab(Tab tab, int newIndex) {
        int curIndex = mModelTabs.indexOf(tab);
        mModelTabs.remove(curIndex);
        mModelTabs.add(newIndex, tab);
        mTabModelObserver.didMoveTab(tab, newIndex, curIndex);
    }

    private void moveTabWithinGroup(Tab tab, int newIndex) {
        int curIndex = mModelTabs.indexOf(tab);
        mModelTabs.remove(curIndex);
        mModelTabs.add(newIndex, tab);
        mTabGroupObserver.didMoveWithinGroup(tab, curIndex, newIndex);
    }

    private void closeTab(Tab tab) {
        mModelTabs.remove(tab);
        mTabModelObserver.didRemoveTabForClosure(tab);
    }

    private void undoCloseTab(Tab tab, int modelIndex) {
        mModelTabs.add(modelIndex, tab);
        mTabModelObserver.tabClosureUndone(tab);
    }

    private void selectTab(Tab tab, @TabId int prevSelectedTabId) {
        mSelectedTab = tab;
        mTabModelObserver.didSelectTab(tab, TabSelectionType.FROM_USER, prevSelectedTabId);
    }

    private static void groupTabs(@Nullable Token groupId, Tab... tabs) {
        for (Tab tab : tabs) {
            when(tab.getTabGroupId()).thenReturn(groupId);
        }
    }

    private boolean isInCurrentGroup(Tab tab) {
        return mCurrentTabGroupId != null && mCurrentTabGroupId.equals(tab.getTabGroupId());
    }

    private void createProvider(@Nullable Predicate<Tab> filter) {
        mProvider = new FlatTabListDataProvider(mTabModelSupplier, filter);
        mProvider.addObserver(mObserver);
    }

    private void setUpProviderWithTabs(@Nullable Predicate<Tab> filter, Tab... tabs) {
        setModelTabs(tabs);
        mTabModelSupplier.set(mTabModel);
        createProvider(filter);
        mProvider.requestDataReset();
        captureObserver(mTabModel);
        clearInvocations(mObserver);
    }

    private void assertItems(TabItem... expected) {
        assertEquals(List.of(expected), mProvider.getItemsForTesting());
    }

    private void assertResetWithItems(TabItem... expected) {
        List<TabListItem> expectedList = List.of(expected);
        verify(mObserver).onDataReset(expectedList);
        assertEquals(expectedList, mProvider.getItemsForTesting());
    }

    private static TabItem item(@TabId int tabId) {
        return new TabItem(
                tabId,
                /* isSelected= */ false,
                /* isPinned= */ false,
                /* isMultiSelected= */ false);
    }

    private static TabItem selected(@TabId int tabId) {
        return new TabItem(
                tabId, /* isSelected= */ true, /* isPinned= */ false, /* isMultiSelected= */ false);
    }

    private static TabItem pinned(@TabId int tabId) {
        return new TabItem(
                tabId, /* isSelected= */ false, /* isPinned= */ true, /* isMultiSelected= */ false);
    }
}
