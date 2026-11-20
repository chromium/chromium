// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
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

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabListDataObserver.PayloadType;

import java.util.Comparator;
import java.util.List;
import java.util.function.Predicate;

/** Unit tests for {@link FlatTabListDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FlatTabListDataProviderUnitTest extends TabListDataProviderTestBase {
    @Mock private TabListDataObserver mSecondObserver;
    @Mock private Profile mProfile;
    @Mock private Profile mOtherProfile;
    @Mock private TabModel mThirdTabModel;

    private final Predicate<Tab> mInCurrentGroupFilter = this::isInCurrentGroup;

    private @Nullable Token mCurrentTabGroupId = TAB_GROUP_ID;
    private SettableNonNullObservableSupplier<List<TabModel>> mTabModelListSupplier;

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

        TabItem multiSelectedItem = multiSelected(TAB1_ID);
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
        mTab1.setIsPinned(true);
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

        // didMoveTab ignores grouped tabs (handled by TabGroupObserver).
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

        commitCloseTab(mTab2);

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

        commitCloseTab(mTab2);

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

    @Test
    public void testOnTabsSelectionChanged_UpdatesOnlyChangedTabsAndPreservesAcrossSelection() {
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        assertItems(selected(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        setMultiSelectedTabs(TAB2_ID, TAB3_ID);

        verify(mObserver).onItemUpdated(multiSelected(TAB2_ID), PayloadType.MULTI_SELECTION);
        verify(mObserver).onItemUpdated(multiSelected(TAB3_ID), PayloadType.MULTI_SELECTION);
        verifyNoMoreInteractions(mObserver);
        assertItems(selected(TAB1_ID), multiSelected(TAB2_ID), multiSelected(TAB3_ID));
        clearInvocations(mObserver);

        // Selecting mTab2 preserves its multi-selected state.
        TabItem selectedAndMultiTab2 =
                new TabItem(
                        TAB2_ID,
                        /* isSelected= */ true,
                        /* isPinned= */ false,
                        /* isMultiSelected= */ true);
        selectTab(mTab2, TAB1_ID);

        InOrder inOrder = inOrder(mObserver);
        inOrder.verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.SELECTION);
        inOrder.verify(mObserver).onItemUpdated(selectedAndMultiTab2, PayloadType.SELECTION);
        assertItems(item(TAB1_ID), selectedAndMultiTab2, multiSelected(TAB3_ID));
    }

    // ============================================================================================
    // TabModelObserver: didChangePinState
    // ============================================================================================

    @Test
    public void testDidChangePinState_UpdatesPinProperty() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems(item(TAB1_ID), item(TAB2_ID));

        mTab2.setIsPinned(true);
        mTabModelObserver.didChangePinState(mTab2);

        verify(mObserver).onItemUpdated(pinned(TAB2_ID), PayloadType.PIN_STATE);
        assertItems(item(TAB1_ID), pinned(TAB2_ID));
    }

    @Test
    public void testDidChangePinState_Unpinned_UpdatesPinProperty() {
        mTab2.setIsPinned(true);
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems(item(TAB1_ID), pinned(TAB2_ID));

        mTab2.setIsPinned(false);
        mTabModelObserver.didChangePinState(mTab2);

        verify(mObserver).onItemUpdated(item(TAB2_ID), PayloadType.PIN_STATE);
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidChangePinState_FilteredOut_RemovesTab() {
        // Filter that only shows unpinned tabs.
        setUpProviderWithTabs(tab -> !tab.getIsPinned(), mTab1, mTab2);
        assertItems(item(TAB1_ID), item(TAB2_ID));

        mTab1.setIsPinned(true);
        mTabModelObserver.didChangePinState(mTab1);

        verify(mObserver).onItemsRemoved(List.of(item(TAB1_ID)));
        assertItems(item(TAB2_ID));
    }

    @Test
    public void testDidChangePinState_Unpinned_FilteredOut_RemovesTab() {
        // Filter that only shows pinned tabs.
        mTab1.setIsPinned(true);
        setUpProviderWithTabs(Tab::getIsPinned, mTab1, mTab2);
        assertItems(pinned(TAB1_ID));

        mTab1.setIsPinned(false);
        mTabModelObserver.didChangePinState(mTab1);

        verify(mObserver).onItemsRemoved(List.of(pinned(TAB1_ID)));
        assertItems();
    }

    @Test
    public void testDidChangePinState_FilterBecameMatching_AddsTab() {
        // Filter that only shows pinned tabs.
        setUpProviderWithTabs(Tab::getIsPinned, mTab1);
        assertItems();

        mTab1.setIsPinned(true);
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
        mTab2.setIsPinned(true);
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

    @Test
    public void testDidMergeTabToGroup_NullFilter_PresentTabMoves_RepositionsItem() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // didMoveTab skips grouped tabs, so didMergeTabToGroup is what moves mTab3 next to mTab1.
        mergeTabToGroup(mTab3, TAB_GROUP_ID, /* newIndex= */ 1, /* isDestinationTab= */ false);

        verify(mObserver).onItemsMoved(List.of(item(TAB3_ID)), /* after= */ item(TAB1_ID));
        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB3_ID), item(TAB2_ID));
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

        // Ungrouping mTab1 clears its group ID and moves it after the group before firing
        // didMoveTabOutOfGroup; didMoveTab must not emit onItemsMoved before removal.
        ungroupTab(mTab1, TAB_GROUP_ID, /* newIndex= */ 1);

        verify(mObserver).onItemsRemoved(List.of(item(TAB1_ID)));
        verifyNoMoreInteractions(mObserver);
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
    // TabListDataProvider: Scope & Projection Decoupling
    // ============================================================================================

    @Test
    public void testBaseProvider_InScopeTabNotProjected_NotifiesClosureEventsAndNoOpsMove() {
        TabListDataProvider testProvider =
                new TabListDataProvider(createTabModelListSupplier()) {
                    @Override
                    protected void rebuildItems() {}

                    @Override
                    protected boolean shouldShowTab(Tab tab) {
                        return false;
                    }
                };
        testProvider.addObserver(mObserver);

        // moveTabItem no-ops when shouldShowTab is false.
        testProvider.moveTabItem(mTab1);
        verify(mObserver, never()).onItemsMoved(any(), any());

        // notifyTabClosureUndone and notifyTabClosureCommitted notify because isTabInScope defaults
        // to true even when shouldShowTab is false.
        testProvider.notifyTabClosureUndone(mTab1);
        verify(mObserver).onTabClosureUndone(TAB1_ID);

        testProvider.notifyTabClosureCommitted(mTab1);
        verify(mObserver).onTabClosureCommitted(TAB1_ID);
    }

    // ============================================================================================
    // Multiple Models: Setup & Lifecycle
    // ============================================================================================

    @Test
    public void testMultipleModels_NotSupported_Asserts() {
        assertThrows(
                AssertionError.class,
                () ->
                        new FlatTabListDataProvider(
                                ObservableSuppliers.createNonNull(
                                        List.of(mTabModel, mOtherTabModel)),
                                /* filter= */ null,
                                /* comparator= */ null,
                                /* supportsMultipleModels= */ false));
    }

    @Test
    public void testSupportsMultipleModels_WithoutComparator_Asserts() {
        assertThrows(
                AssertionError.class,
                () ->
                        new FlatTabListDataProvider(
                                ObservableSuppliers.createNonNull(List.of(mTabModel)),
                                /* filter= */ null,
                                /* comparator= */ null,
                                /* supportsMultipleModels= */ true));
    }

    @Test
    public void testMultipleModels_DifferentProfiles_Asserts() {
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mOtherTabModel.getProfile()).thenReturn(mOtherProfile);

        assertThrows(AssertionError.class, () -> setUpSortedProvider(mTabModel, mOtherTabModel));
    }

    @Test
    public void testMultipleModels_SameProfile_DoesNotAssert() {
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mOtherTabModel.getProfile()).thenReturn(mProfile);

        setUpSortedProvider(mTabModel, mOtherTabModel);
    }

    @Test
    public void testMultipleModels_OffTheRecordModelWithoutProfile_Asserts() {
        // Like an incognito model before its first tab: off the record, but no Profile yet.
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mOtherTabModel.isOffTheRecord()).thenReturn(true);

        assertThrows(AssertionError.class, () -> setUpSortedProvider(mTabModel, mOtherTabModel));
    }

    @Test
    public void testMultipleModels_DuplicateModels_Asserts() {
        List<TabModel> duplicateModels = List.of(mTabModel, mTabModel);
        mTabModelListSupplier = ObservableSuppliers.createNonNull(duplicateModels);
        assertThrows(
                AssertionError.class,
                () ->
                        new FlatTabListDataProvider(
                                mTabModelListSupplier,
                                /* filter= */ null,
                                (a, b) -> Integer.compare(b.getId(), a.getId()),
                                /* supportsMultipleModels= */ true));

        setUpSortedProvider(mTabModel, mOtherTabModel);
        assertThrows(AssertionError.class, () -> mTabModelListSupplier.set(duplicateModels));
    }

    @Test
    public void testSingleModel_SwitchToModelWithOtherProfile_DoesNotAssert() {
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mOtherTabModel.getProfile()).thenReturn(mOtherProfile);
        mTabModelListSupplier = ObservableSuppliers.createNonNull(List.of(mTabModel));
        initProvider(
                new FlatTabListDataProvider(
                        mTabModelListSupplier,
                        /* filter= */ null,
                        /* comparator= */ null,
                        /* supportsMultipleModels= */ false));
        mProvider.requestDataReset();
        captureObserver(mTabModel);

        // Like a regular to incognito switch: each list has one Profile.
        mTabModelListSupplier.set(List.of(mOtherTabModel));
        verifyDetached(/* times= */ 1, mTabModel);
    }

    @Test
    public void testMultipleModels_StopAndDestroy_DetachFromEveryModel() {
        setUpSortedProvider(mTabModel, mOtherTabModel);

        mProvider.stop();
        verifyDetached(/* times= */ 1, mTabModel, mOtherTabModel);
        assertItems();

        mProvider.requestDataReset();
        mProvider.destroy();
        verifyDetached(/* times= */ 2, mTabModel, mOtherTabModel);
        assertItems();
    }

    @Test
    public void testMultipleModels_SupplierSwitch_DetachesEveryModel() {
        setUpSortedProvider(mTabModel, mOtherTabModel);

        mTabModelListSupplier.set(List.of(mOtherTabModel, mTabModel));
        verifyDetached(/* times= */ 0, mTabModel, mOtherTabModel);

        mTabModelListSupplier.set(List.of());
        verifyDetached(/* times= */ 1, mTabModel, mOtherTabModel);
        assertItems();
    }

    // ============================================================================================
    // Comparator Tests
    // ============================================================================================

    @Test
    public void testComparator_SingleModel_SortsByComparator() {
        mModelTabs.addAll(List.of(mTab1, mTab2, mTab3));
        setUpSortedProvider(mTabModel);

        assertItems(item(TAB3_ID), item(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testComparator_Rebuild_SortsTabsFromAllModels() {
        mModelTabs.addAll(List.of(mTab1, mTab3));
        mOtherModelTabs.addAll(List.of(mTab2, mTab4));
        setUpSortedProvider(mTabModel, mOtherTabModel);

        assertItems(item(TAB4_ID), item(TAB3_ID), item(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testComparator_Rebuild_ShowsRepeatedTabIdOnce() {
        // The second model holds a different Tab object with the same ID.
        MockTab otherTab2 = new MockTab(TAB2_ID, /* profile= */ null);
        otherTab2.setIsPinned(true);
        mModelTabs.addAll(List.of(mTab1, mTab2));
        mOtherModelTabs.add(otherTab2);
        setUpSortedProvider(mTabModel, mOtherTabModel);

        // The first model's copy wins, so the card isn't pinned.
        assertItems(item(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testComparator_Rebuild_FilteredOutFirstCopyHidesRepeatedTabId() {
        // The filter shows pinned tabs. The first model's copy of TAB2_ID isn't pinned, while the
        // second model's copy is.
        MockTab otherTab2 = new MockTab(TAB2_ID, /* profile= */ null);
        otherTab2.setIsPinned(true);
        mTab1.setIsPinned(true);
        mModelTabs.addAll(List.of(mTab1, mTab2));
        mOtherModelTabs.add(otherTab2);
        mTabModelListSupplier =
                ObservableSuppliers.createNonNull(List.of(mTabModel, mOtherTabModel));
        initProvider(
                new FlatTabListDataProvider(
                        mTabModelListSupplier,
                        Tab::getIsPinned,
                        (a, b) -> Integer.compare(b.getId(), a.getId()),
                        /* supportsMultipleModels= */ true));
        mProvider.requestDataReset();

        // The first model's copy is checked before the filter, so TAB2_ID gets no card.
        assertItems(pinned(TAB1_ID));
    }

    @Test
    public void testComparator_Rebuild_SkipsModelThatHasNotLoaded() {
        when(mOtherTabModel.isTabStateInitialized()).thenReturn(false);
        mModelTabs.addAll(List.of(mTab1, mTab3));
        mOtherModelTabs.addAll(List.of(mTab2, mTab4));
        setUpSortedProvider(mTabModel, mOtherTabModel);
        assertItems(item(TAB3_ID), item(TAB1_ID));

        when(mOtherTabModel.isTabStateInitialized()).thenReturn(true);
        mTabModelObserver.restoreCompleted();

        assertItems(item(TAB4_ID), item(TAB3_ID), item(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testComparator_AddTabFromSecondModel_InsertsAtStartEndAndMiddle() {
        mModelTabs.add(mTab2);
        setUpSortedProvider(mTabModel, mOtherTabModel);

        // Sorts first, so it has no previous item.
        addTabToOtherModel(mTab4);
        verify(mObserver).onItemsInserted(List.of(item(TAB4_ID)), null);
        assertItems(item(TAB4_ID), item(TAB2_ID));

        // Sorts last.
        addTabToOtherModel(mTab1);
        verify(mObserver).onItemsInserted(List.of(item(TAB1_ID)), item(TAB2_ID));
        assertItems(item(TAB4_ID), item(TAB2_ID), item(TAB1_ID));

        // Sorts in the middle, though it's at the end of its own model.
        addTabToOtherModel(mTab3);
        verify(mObserver).onItemsInserted(List.of(item(TAB3_ID)), item(TAB4_ID));
        assertItems(item(TAB4_ID), item(TAB3_ID), item(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testComparator_ModelMove_KeepsCardInPlace() {
        // Every tab compares equal, so re-placing the moved card would put it after the others.
        mModelTabs.addAll(List.of(mTab1, mTab2, mTab3));
        setUpProviderWithComparator((a, b) -> 0, mTabModel);

        moveTab(mTab1, /* newIndex= */ 2);

        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));
        verify(mObserver, never()).onItemsMoved(any(), any());
    }

    @Test
    public void testComparator_MoveWithinGroup_KeepsCardInPlace() {
        mTab1.setTabGroupId(TAB_GROUP_ID);
        mTab2.setTabGroupId(TAB_GROUP_ID);
        mModelTabs.addAll(List.of(mTab1, mTab2, mTab3));
        setUpProviderWithComparator((a, b) -> 0, mTabModel);

        moveTabWithinGroup(mTab1, /* newIndex= */ 1);

        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));
        verify(mObserver, never()).onItemsMoved(any(), any());
    }

    @Test
    public void testComparator_GroupJoinAndLeave_KeepsTiedCardsInPlace() {
        mModelTabs.addAll(List.of(mTab1, mTab2));
        setUpProviderWithComparator((a, b) -> 0, mTabModel);

        mergeTabToGroup(mTab1, TAB_GROUP_ID, /* newIndex= */ 0, /* isDestinationTab= */ true);
        ungroupTab(mTab1, TAB_GROUP_ID, /* newIndex= */ 0);

        assertItems(item(TAB1_ID), item(TAB2_ID));
        verify(mObserver, never()).onItemsMoved(any(), any());
    }

    // ============================================================================================
    // Multiple Models: Tab Events
    // ============================================================================================

    @Test
    public void testMultipleModels_AddTabWhileFirstModelLoading_InsertsTab() {
        mModelTabs.add(mTab1);
        mOtherModelTabs.add(mTab2);
        when(mTabModel.isTabStateInitialized()).thenReturn(false);
        setUpSortedProvider(mTabModel, mOtherTabModel);
        assertItems(item(TAB2_ID));

        addTabToOtherModel(mTab3);

        verify(mObserver).onItemsInserted(List.of(item(TAB3_ID)), /* after= */ null);
        assertItems(item(TAB3_ID), item(TAB2_ID));
    }

    @Test
    public void testMultipleModels_PinInSecondModelWhileFirstLoading_UpdatesItem() {
        mModelTabs.add(mTab1);
        mOtherModelTabs.add(mTab2);
        when(mTabModel.isTabStateInitialized()).thenReturn(false);
        setUpSortedProvider(mTabModel, mOtherTabModel);

        mTab2.setIsPinned(true);
        mTabModelObserver.didChangePinState(mTab2);

        verify(mObserver).onItemUpdated(pinned(TAB2_ID), PayloadType.PIN_STATE);
        assertItems(pinned(TAB2_ID));
    }

    @Test
    public void testMultipleModels_MultiSelectInSecondModelWhileFirstLoading_UpdatesItem() {
        mModelTabs.add(mTab1);
        mOtherModelTabs.add(mTab2);
        when(mTabModel.isTabStateInitialized()).thenReturn(false);
        setUpSortedProvider(mTabModel, mOtherTabModel);

        setMultiSelectedTabs(TAB2_ID);

        verify(mObserver).onItemUpdated(multiSelected(TAB2_ID), PayloadType.MULTI_SELECTION);
        assertItems(multiSelected(TAB2_ID));
    }

    @Test
    public void testMultipleModels_RebuildReadsMultiSelectFromTabsOwnModel() {
        // Both models are loaded; only the second holds TAB2_ID, and it has it multi-selected.
        mModelTabs.add(mTab1);
        mOtherModelTabs.add(mTab2);
        mMultiSelectedTabIds.add(TAB2_ID);

        setUpSortedProvider(mTabModel, mOtherTabModel);

        assertItems(multiSelected(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testMultipleModels_RemoveModel_RemovesOnlyItsTabs() {
        mModelTabs.add(mTab1);
        mModelTabs.add(mTab2);
        mOtherModelTabs.add(mTab3);
        setUpSortedProvider(mTabModel, mOtherTabModel);
        assertItems(item(TAB3_ID), item(TAB2_ID), item(TAB1_ID));

        // The second model leaves, so only its tab 3 goes.
        mTabModelListSupplier.set(List.of(mTabModel));

        verifyDetached(/* times= */ 1, mOtherTabModel);
        verifyDetached(/* times= */ 0, mTabModel);
        verify(mObserver).onItemsRemoved(List.of(item(TAB3_ID)));
        verify(mObserver, times(1)).onItemsRemoved(any());
        verify(mObserver, never()).onDataReset(any());
        assertItems(item(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testMultipleModels_AddModel_InsertsItsNewTabsInOrder() {
        mModelTabs.add(mTab1);
        mOtherModelTabs.add(mTab3);
        stubBackedTabModel(mThirdTabModel, List.of(mTab2));
        setUpSortedProvider(mTabModel, mOtherTabModel);
        assertItems(item(TAB3_ID), item(TAB1_ID));

        // The joining model's tab 2 is added in sorted order without a reset.
        mTabModelListSupplier.set(List.of(mTabModel, mOtherTabModel, mThirdTabModel));

        verifyAttached(mThirdTabModel);
        verify(mObserver).onItemsInserted(List.of(item(TAB2_ID)), item(TAB3_ID));
        verify(mObserver, times(1)).onItemsInserted(any(), any());
        verify(mObserver, never()).onDataReset(any());
        assertItems(item(TAB3_ID), item(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testMultipleModels_ReplaceOneModel_UpdatesInPlaceAndResetDoesNotReattach() {
        mModelTabs.add(mTab2);
        mOtherModelTabs.add(mTab3);
        stubBackedTabModel(mThirdTabModel, List.of(mTab1));
        setUpSortedProvider(mTabModel, mOtherTabModel);
        assertItems(item(TAB3_ID), item(TAB2_ID));

        mTabModelListSupplier.set(List.of(mTabModel, mThirdTabModel));

        verifyDetached(/* times= */ 1, mOtherTabModel);
        verifyAttached(mThirdTabModel);
        verifyDetached(/* times= */ 0, mTabModel);
        assertItems(item(TAB2_ID), item(TAB1_ID));

        // A reset after the update rebuilds without attaching the models again.
        clearInvocations(mTabModel, mThirdTabModel, mObserver);
        mProvider.requestDataReset();

        verify(mTabModel, never()).addObserver(any());
        verify(mThirdTabModel, never()).addObserver(any());
        assertResetWithItems(item(TAB2_ID), item(TAB1_ID));
    }

    @Test
    public void testMultipleModels_AddAndRemoveLoadingModel_NoCardChanges() {
        mModelTabs.add(mTab1);
        stubBackedTabModel(mThirdTabModel, List.of(mTab2));
        when(mThirdTabModel.isTabStateInitialized()).thenReturn(false);
        setUpSortedProvider(mTabModel, mOtherTabModel);

        // A model that hasn't loaded adds no cards when it joins and removes none when it leaves.
        mTabModelListSupplier.set(List.of(mTabModel, mOtherTabModel, mThirdTabModel));
        verifyAttached(mThirdTabModel);
        mTabModelListSupplier.set(List.of(mTabModel, mOtherTabModel));
        verifyDetached(/* times= */ 1, mThirdTabModel);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    // ============================================================================================
    // Test Helpers
    // ============================================================================================

    private boolean isInCurrentGroup(Tab tab) {
        return mCurrentTabGroupId != null && mCurrentTabGroupId.equals(tab.getTabGroupId());
    }

    private void createProvider(@Nullable Predicate<Tab> filter) {
        initProvider(
                new FlatTabListDataProvider(
                        createTabModelListSupplier(),
                        filter,
                        /* comparator= */ null,
                        /* supportsMultipleModels= */ false));
    }

    private void setUpProviderWithTabs(@Nullable Predicate<Tab> filter, Tab... tabs) {
        initProviderWithTabs(
                new FlatTabListDataProvider(
                        createTabModelListSupplier(),
                        filter,
                        /* comparator= */ null,
                        /* supportsMultipleModels= */ false),
                tabs);
    }

    /** Sets up a provider over {@code models} that sorts tabs by descending ID. */
    private void setUpSortedProvider(TabModel... models) {
        setUpProviderWithComparator((a, b) -> Integer.compare(b.getId(), a.getId()), models);
    }

    /**
     * Sets up a provider over {@code models} sorted by {@code comparator}, and checks that every
     * model got the provider's observers.
     */
    private void setUpProviderWithComparator(Comparator<Tab> comparator, TabModel... models) {
        mTabModelListSupplier = ObservableSuppliers.createNonNull(List.of(models));
        initProvider(
                new FlatTabListDataProvider(
                        mTabModelListSupplier,
                        /* filter= */ null,
                        comparator,
                        /* supportsMultipleModels= */ models.length > 1));
        mProvider.requestDataReset();
        captureObserver(models[0]);
        verifyAttached(models);
        clearInvocations(mObserver);
    }

    /** Verifies the provider's observers were added to each of {@code models}. */
    private void verifyAttached(TabModel... models) {
        for (TabModel model : models) {
            verify(model).addObserver(mTabModelObserver);
            verify(model).addTabGroupObserver(mTabGroupObserver);
        }
    }

    /** Verifies the provider's observers were removed from each of {@code models}. */
    private void verifyDetached(int times, TabModel... models) {
        for (TabModel model : models) {
            verify(model, times(times)).removeObserver(mTabModelObserver);
            verify(model, times(times)).removeTabGroupObserver(mTabGroupObserver);
        }
    }

    /** Adds {@code tab} to the end of the second model and notifies the provider. */
    private void addTabToOtherModel(Tab tab) {
        mOtherModelTabs.add(tab);
        mTabModelObserver.didAddTab(
                tab,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ false);
    }
}
