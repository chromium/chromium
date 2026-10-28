// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabListDataObserver.PayloadType;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;

/**
 * Unit tests for {@link GroupedTabListDataProvider}.
 *
 * <p>Tests both {@code GROUPED} mode (collapsed-only Grid Tab Switcher) and {@code NESTED} mode
 * (expandable Vertical Tabs) using realistic {@link TabModel} event sequences.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupedTabListDataProviderUnitTest extends TabListDataProviderTestBase {
    // ============================================================================================
    // Value Semantics Tests
    // ============================================================================================

    @Test
    public void testTabGroupItem_ValueSemantics() {
        TabGroupItem item =
                new TabGroupItem(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true);
        assertEquals(TAB_GROUP_ID, item.getTabGroupId());
        assertFalse(item.isSelected());
        assertTrue(item.isCollapsed());

        // Calling withSelected with the current value returns the same instance.
        assertSame(item, item.withSelected(/* isSelected= */ false));

        // Flipping selection state returns a new instance with the updated field.
        TabGroupItem selectedItem = item.withSelected(/* isSelected= */ true);
        assertNotSame(item, selectedItem);
        assertTrue(selectedItem.isSelected());
        assertTrue(selectedItem.isCollapsed());

        // Calling withCollapsed with the current value returns the same instance.
        assertSame(item, item.withCollapsed(/* isCollapsed= */ true));

        // Flipping collapse state returns a new instance with the updated field.
        TabGroupItem expandedItem = item.withCollapsed(/* isCollapsed= */ false);
        assertNotSame(item, expandedItem);
        assertFalse(expandedItem.isSelected());
        assertFalse(expandedItem.isCollapsed());

        // Value equality and hashCode across fields.
        TabGroupItem duplicate =
                new TabGroupItem(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true);
        assertEquals(item, duplicate);
        assertEquals(item.hashCode(), duplicate.hashCode());
        assertNotEquals(item, selectedItem);
        assertNotEquals(
                item,
                new TabGroupItem(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false));
        assertNotEquals(
                item,
                new TabGroupItem(TAB_GROUP_ID_2, /* isSelected= */ false, /* isCollapsed= */ true));
        assertNotEquals(item, new Object());
    }

    // ============================================================================================
    // Section A: Construction & Initial Projection
    // ============================================================================================

    @Test
    public void testInitialState_NoTabModel_ItemsEmpty() {
        createProvider(/* supportsGroupExpansion= */ false);

        assertItems();
        assertFalse(mProvider.shouldShowTab(mTab1));
        verifyNoInteractions(mObserver);
    }

    @Test
    public void testInitialState_TabModelRestoring_ItemsEmpty() {
        when(mTabModel.isTabStateInitialized()).thenReturn(false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        assertItems();
    }

    @Test
    public void testInitialState_GroupedMode_CollapsedCardsOnly() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3, mTab4);

        // In GROUPED mode, group cards are always collapsed and individual member tabs are not
        // projected.
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB4_ID));
        assertTrue(mProvider.shouldShowTab(mTab1));
        assertFalse(mProvider.shouldShowTab(mTab2));
    }

    @Test
    public void testInitialState_NestedMode_ExpandedIncludesChildRows() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mCollapsed.put(TAB_GROUP_ID, false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3, mTab4);

        // In NESTED mode with an expanded group, header is followed immediately by child TabItems.
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                item(TAB3_ID),
                item(TAB4_ID));
        assertTrue(mProvider.shouldShowTab(mTab1));
        assertTrue(mProvider.shouldShowTab(mTab2));
    }

    @Test
    public void testInitialState_NestedMode_CollapsedExcludesChildRows() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mCollapsed.put(TAB_GROUP_ID, true);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3, mTab4);

        // When collapsed, child items are omitted.
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB4_ID));
        assertTrue(mProvider.shouldShowTab(mTab1));
        assertFalse(mProvider.shouldShowTab(mTab2));
    }

    @Test
    public void testInitialState_SelectionOnGroupInGroupedMode() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mSelectedTab = mTab2;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);

        // In GROUPED mode, selection of a grouped tab resolves to selecting the parent
        // TabGroupItem.
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true));
    }

    @Test
    public void testInitialState_SelectionOnChildInNestedMode() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mCollapsed.put(TAB_GROUP_ID, false);
        mSelectedTab = mTab2;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);

        // In NESTED mode, TabGroupItem is never selected; selection highlights the child TabItem
        // row.
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                selected(TAB2_ID),
                item(TAB3_ID));
    }

    // ============================================================================================
    // Section B: Tab Additions & Session Restore
    // ============================================================================================

    @Test
    public void testAddTab_StandaloneTab_NormalLaunch() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        addTab(mTab3, /* modelIndex= */ 2);

        verify(mObserver)
                .onItemsInserted(
                        List.of(item(TAB3_ID)),
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));
    }

    @Test
    public void testAddTab_GroupedMode_GroupWithParent_SecondTabSuppressed() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        // Adding a child tab via groupWithParent: didAddTab fires with non-null tabGroupId and
        // normal launch type, and early-returns because didMergeTabToGroup will follow.
        groupTabs(TAB_GROUP_ID, mTab2);
        addTabWithLaunchType(mTab2, /* modelIndex= */ 1, TabLaunchType.FROM_LINK);

        verifyNoInteractions(mObserver);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testAddTab_GroupedMode_FromRestore_NewAndExistingGroup() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        // 1st pre-grouped tab from restore inserts the TabGroupItem header.
        groupTabs(TAB_GROUP_ID, mTab2);
        addTabWithLaunchType(mTab2, /* modelIndex= */ 1, TabLaunchType.FROM_RESTORE);

        verify(mObserver)
                .onItemsInserted(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ true)),
                        item(TAB1_ID));
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
        clearInvocations(mObserver);

        // 2nd pre-grouped tab from restore updates the existing TabGroupItem with GROUP_MEMBERSHIP.
        groupTabs(TAB_GROUP_ID, mTab3);
        mSelectedTab = mTab3;
        addTabWithLaunchType(mTab3, /* modelIndex= */ 2, TabLaunchType.FROM_RESTORE);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        verify(mObserver, never()).onItemsInserted(any(), any());
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true));
    }

    @Test
    public void testAddTab_GroupedMode_FromReparenting_PreGroupedTabUpdatesGroup() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        groupTabs(TAB_GROUP_ID, mTab2);
        addTabWithLaunchType(mTab2, /* modelIndex= */ 1, TabLaunchType.FROM_REPARENTING);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        clearInvocations(mObserver);

        groupTabs(TAB_GROUP_ID, mTab3);
        addTabWithLaunchType(mTab3, /* modelIndex= */ 2, TabLaunchType.FROM_REPARENTING_BACKGROUND);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testAddTab_NestedMode_ExpandedGroup_InsertsHeaderAndChildRows() {
        mCollapsed.put(TAB_GROUP_ID, false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1);

        // 1st tab of an expanded group inserts the TabGroupItem header and then the child TabItem
        // directly after the header.
        groupTabs(TAB_GROUP_ID, mTab2);
        addTabWithLaunchType(mTab2, /* modelIndex= */ 1, TabLaunchType.FROM_RESTORE);

        InOrder order = inOrder(mObserver);
        order.verify(mObserver)
                .onItemsInserted(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ false)),
                        item(TAB1_ID));
        order.verify(mObserver)
                .onItemsInserted(
                        List.of(item(TAB2_ID)),
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false));
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID));
        clearInvocations(mObserver);

        // 2nd tab of the expanded group updates the header and appends the child TabItem.
        groupTabs(TAB_GROUP_ID, mTab3);
        addTabWithLaunchType(mTab3, /* modelIndex= */ 2, TabLaunchType.FROM_RESTORE);

        order = inOrder(mObserver);
        order.verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.GROUP_MEMBERSHIP);
        order.verify(mObserver).onItemsInserted(List.of(item(TAB3_ID)), item(TAB2_ID));
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                item(TAB3_ID));
    }

    @Test
    public void testAddTab_NestedMode_CollapsedGroup_SuppressesChildRow() {
        groupTabs(TAB_GROUP_ID, mTab1);
        mCollapsed.put(TAB_GROUP_ID, true);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1);

        groupTabs(TAB_GROUP_ID, mTab2);
        addTabWithLaunchType(mTab2, /* modelIndex= */ 1, TabLaunchType.FROM_RESTORE);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        verify(mObserver, never()).onItemsInserted(any(), any());
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    // ============================================================================================
    // Section C: Tab & Group Removals and Closure Callbacks
    // ============================================================================================

    @Test
    public void testCloseTab_StandaloneTab_RemovesItem() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        closeTab(mTab1);

        verify(mObserver).onItemsRemoved(List.of(item(TAB1_ID)));
        assertItems(item(TAB2_ID));
    }

    @Test
    public void testCloseTab_GroupedMode_NonLastTab_UpdatesGroupMembership() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        closeTab(mTab1);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        verify(mObserver, never()).onItemsRemoved(any());
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testCloseTab_GroupedMode_LastTab_SuppressesGroupUpdate() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        // Closing the last tab in a group detaches it from TabModel (!containsTabGroup), so
        // didRemoveTabForClosure suppresses the intermediate GROUP_MEMBERSHIP update before
        // didRemoveTabGroup removes the group card.
        closeTab(mTab1);

        verifyNoInteractions(mObserver);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testCloseTab_NestedMode_Expanded_RemovesChildRowAndUpdatesHeader() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2);

        closeTab(mTab2);

        verify(mObserver).onItemsRemoved(List.of(item(TAB2_ID)));
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.GROUP_MEMBERSHIP);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID));
    }

    @Test
    public void testCloseTab_NestedMode_Collapsed_UpdatesHeaderOnly() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, true);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2);

        closeTab(mTab2);

        verify(mObserver, never()).onItemsRemoved(any());
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testTabRemoved_StandaloneAndExpandedGroupChild() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mCollapsed.put(TAB_GROUP_ID, false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);

        // Removing a standalone tab via tabRemoved (e.g. reparenting to another window).
        mModelTabs.remove(mTab1);
        mTabModelObserver.tabRemoved(mTab1);
        verify(mObserver).onItemsRemoved(List.of(item(TAB1_ID)));
        clearInvocations(mObserver);

        // Removing a child tab from an expanded group removes the child row and updates the header.
        mModelTabs.remove(mTab2);
        mTabModelObserver.tabRemoved(mTab2);
        verify(mObserver).onItemsRemoved(List.of(item(TAB2_ID)));
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.GROUP_MEMBERSHIP);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB3_ID));
    }

    @Test
    public void testDidRemoveTabGroup_GroupedMode_RemovesGroupCard() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        closeTab(mTab1);
        removeTabGroup(TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);

        verify(mObserver)
                .onItemsRemoved(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ true)));
        assertItems();
    }

    @Test
    public void testDidRemoveTabGroup_NestedMode_RemovesGroupHeader() {
        groupTabs(TAB_GROUP_ID, mTab1);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1);

        closeTab(mTab1);
        removeTabGroup(TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);

        verify(mObserver).onItemsRemoved(List.of(item(TAB1_ID)));
        verify(mObserver)
                .onItemsRemoved(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ false)));
        assertItems();
    }

    @Test
    public void testDidRemoveTabGroup_UnknownGroup_NoOps() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        removeTabGroup(TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);

        verifyNoInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    @Test
    public void testOnTabGroupRemoving_SuppressesIntermediateGroupUpdates() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        removingTabGroup(TAB_GROUP_ID);

        // Closing the first tab in a group being removed suppresses the intermediate
        // GROUP_MEMBERSHIP update.
        closeTab(mTab1);
        verifyNoInteractions(mObserver);

        // Closing the second tab also suppresses the update.
        closeTab(mTab2);
        verifyNoInteractions(mObserver);

        // didRemoveTabGroup removes the group card and clears the removing state.
        removeTabGroup(TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);
        verify(mObserver)
                .onItemsRemoved(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ true)));
        assertItems();
    }

    @Test
    public void testTabClosureUndone_StandaloneTab() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        closeTab(mTab2);
        clearInvocations(mObserver);

        undoCloseTab(mTab2, /* modelIndex= */ 1);

        verify(mObserver).onItemsInserted(List.of(item(TAB2_ID)), item(TAB1_ID));
        verify(mObserver).onTabClosureUndone(TAB2_ID);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));
    }

    @Test
    public void testTabClosureUndone_GroupedMode_RestoresGroupCard() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);
        closeTab(mTab1);
        removeTabGroup(TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);
        clearInvocations(mObserver);

        undoCloseTab(mTab1, /* modelIndex= */ 0);

        verify(mObserver)
                .onItemsInserted(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ true)),
                        /* after= */ null);
        verify(mObserver).onTabClosureUndone(TAB1_ID);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testTabClosureUndone_GroupedMode_ExistingGroup_UpdatesGroupMembership() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);
        closeTab(mTab2);
        clearInvocations(mObserver);

        undoCloseTab(mTab2, /* modelIndex= */ 1);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        verify(mObserver).onTabClosureUndone(TAB2_ID);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testTabClosureUndone_NestedMode_Expanded_RestoresChildRowAndUpdatesHeader() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2);
        closeTab(mTab2);
        clearInvocations(mObserver);

        undoCloseTab(mTab2, /* modelIndex= */ 1);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.GROUP_MEMBERSHIP);
        verify(mObserver).onItemsInserted(List.of(item(TAB2_ID)), item(TAB1_ID));
        verify(mObserver).onTabClosureUndone(TAB2_ID);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB2_ID));
    }

    @Test
    public void testTabClosureCommitted_ForwardsWithoutItemChanges() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);
        closeTab(mTab2);
        clearInvocations(mObserver);

        commitCloseTab(mTab2);

        verify(mObserver).onTabClosureCommitted(TAB2_ID);
        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    // ============================================================================================
    // Section D: Group Creation & Additive Merges
    // ============================================================================================

    @Test
    public void testDidMergeTabToGroup_GroupedMode_SingleTabGroup_ReplacesTabWithGroupCard() {
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        mergeTabToGroup(mTab1, TAB_GROUP_ID, /* newIndex= */ 0, /* isDestinationTab= */ true);

        verify(mObserver).onItemsRemoved(List.of(selected(TAB1_ID)));
        verify(mObserver)
                .onItemsInserted(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ true,
                                        /* isCollapsed= */ true)),
                        /* after= */ null);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true),
                item(TAB2_ID));
    }

    @Test
    public void testDidMergeTabToGroup_GroupedMode_ExistingGroup_RemovesTabAndUpdatesGroup() {
        groupTabs(TAB_GROUP_ID, mTab1);
        mSelectedTab = mTab2;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        mergeTabToGroup(mTab2, TAB_GROUP_ID, /* newIndex= */ 1, /* isDestinationTab= */ false);

        verify(mObserver).onItemsRemoved(List.of(selected(TAB2_ID)));
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true));
    }

    @Test
    public void testDidMergeTabToGroup_NestedMode_Expanded_InsertsHeaderAndMovesChild() {
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);

        // Destination tab mTab1 forms the expanded group header and updates in place.
        mergeTabToGroup(mTab1, TAB_GROUP_ID, /* newIndex= */ 0, /* isDestinationTab= */ true);

        verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.GROUP_MEMBERSHIP);
        verify(mObserver)
                .onItemsInserted(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ false)),
                        /* after= */ null);
        verify(mObserver, never()).onItemsMoved(any(), any());
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB2_ID),
                item(TAB3_ID));
        clearInvocations(mObserver);

        // Source tab mTab3 moves into the expanded group after mTab1 and updates group membership.
        mergeTabToGroup(mTab3, TAB_GROUP_ID, /* newIndex= */ 1, /* isDestinationTab= */ false);

        verify(mObserver).onItemsMoved(List.of(item(TAB3_ID)), item(TAB1_ID));
        verify(mObserver).onItemUpdated(item(TAB3_ID), PayloadType.GROUP_MEMBERSHIP);
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.GROUP_MEMBERSHIP);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB3_ID),
                item(TAB2_ID));
    }

    @Test
    public void testDidMergeTabToGroup_NestedMode_Expanded_GroupWithParent_InsertsChildTab() {
        groupTabs(TAB_GROUP_ID, mTab1);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1);

        // Adding a tab via groupWithParent suppresses didAddTab and inserts the child row on
        // didMergeTabToGroup without firing a redundant onItemUpdated on the new TabItem.
        groupTabs(TAB_GROUP_ID, mTab2);
        addTabWithLaunchType(mTab2, /* modelIndex= */ 1, TabLaunchType.FROM_LINK);
        mTabGroupObserver.didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        verify(mObserver).onItemsInserted(List.of(item(TAB2_ID)), item(TAB1_ID));
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.GROUP_MEMBERSHIP);
        verifyNoMoreInteractions(mObserver);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB2_ID));
    }

    // ============================================================================================
    // Section E: Ungrouping & Group Dissolution
    // ============================================================================================

    @Test
    public void testDidMoveTabOutOfGroup_GroupedMode_SurvivingGroup_InsertsTabAndUpdatesGroup() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);

        // Ungrouping mTab1 with trailing=true moves it to index 1 after mTab2.
        ungroupTab(mTab1, TAB_GROUP_ID, /* newIndex= */ 1);

        verify(mObserver)
                .onItemsInserted(
                        List.of(selected(TAB1_ID)),
                        group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true));
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                selected(TAB1_ID),
                item(TAB3_ID));
    }

    @Test
    public void testDidMoveTabOutOfGroup_GroupedMode_LastTabDissolvesGroup_ReplacesGroupWithTab() {
        groupTabs(TAB_GROUP_ID, mTab1);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        ungroupTab(mTab1, TAB_GROUP_ID, /* newIndex= */ 0);
        removeTabGroup(TAB_GROUP_ID, DidRemoveTabGroupReason.UNGROUP);

        verify(mObserver).onItemsInserted(List.of(item(TAB1_ID)), /* after= */ null);
        verify(mObserver)
                .onItemsRemoved(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ true)));
        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidMoveTabOutOfGroup_NestedMode_Expanded_MovesAndUpdatesTabAndGroup() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2);

        // Ungrouping mTab1 with trailing=true moves it after mTab2 and clears its group styling.
        ungroupTab(mTab1, TAB_GROUP_ID, /* newIndex= */ 1);

        verify(mObserver).onItemsMoved(List.of(item(TAB1_ID)), item(TAB2_ID));
        verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.GROUP_MEMBERSHIP);
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.GROUP_MEMBERSHIP);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                item(TAB1_ID));
    }

    @Test
    public void testDidMoveTabOutOfGroup_GroupToGroupMerge_SkipsTabSyncAndUpdatesOldGroup() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        groupTabs(TAB_GROUP_ID_2, mTab3);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);

        // Moving mTab2 directly into TAB_GROUP_ID_2 fires didMoveTabOutOfGroup with non-null
        // tabGroupId before didMergeTabToGroup.
        mTab2.setTabGroupId(TAB_GROUP_ID_2);
        mTabGroupObserver.didMoveTabOutOfGroup(mTab2, TAB_GROUP_ID);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.GROUP_MEMBERSHIP);
        verifyNoMoreInteractions(mObserver);
    }

    // ============================================================================================
    // Section F: Group Metadata & Visuals
    // ============================================================================================

    @Test
    public void testDidChangeTabGroupTitle_UpdatesGroupItem() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        mTabGroupObserver.didChangeTabGroupTitle(TAB_GROUP_ID, "New Title");

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.TITLE);
        verifyNoMoreInteractions(mObserver);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testDidChangeTabGroupTitle_UnknownGroup_NoOps() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        mTabGroupObserver.didChangeTabGroupTitle(TAB_GROUP_ID, "New Title");

        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    @Test
    public void testDidChangeTabGroupColor_UpdatesGroupItem() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        mTabGroupObserver.didChangeTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.COLOR);
        verifyNoMoreInteractions(mObserver);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testDidChangeTabGroupColor_UnknownGroup_NoOps() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        mTabGroupObserver.didChangeTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);

        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_GroupedMode_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        mTabGroupObserver.didChangeTabGroupCollapsed(
                TAB_GROUP_ID, /* isCollapsed= */ false, /* animate= */ false);

        verifyNoMoreInteractions(mObserver);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_NestedMode_Collapse_RemovesChildItems() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB2_ID));

        mCollapsed.put(TAB_GROUP_ID, /* value= */ true);
        mTabGroupObserver.didChangeTabGroupCollapsed(
                TAB_GROUP_ID, /* isCollapsed= */ true, /* animate= */ false);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.COLLAPSED);
        verify(mObserver).onItemsRemoved(List.of(item(TAB1_ID), item(TAB2_ID)));
        verifyNoMoreInteractions(mObserver);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_NestedMode_Expand_InsertsChildItems() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ true);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));

        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        mTabGroupObserver.didChangeTabGroupCollapsed(
                TAB_GROUP_ID, /* isCollapsed= */ false, /* animate= */ false);

        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.COLLAPSED);
        verify(mObserver)
                .onItemsInserted(
                        List.of(item(TAB1_ID), item(TAB2_ID)),
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false));
        verifyNoMoreInteractions(mObserver);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB2_ID));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_NestedMode_AlreadyMatchesState_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ true);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2);

        mTabGroupObserver.didChangeTabGroupCollapsed(
                TAB_GROUP_ID, /* isCollapsed= */ true, /* animate= */ false);

        verifyNoMoreInteractions(mObserver);
        assertItems(group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_NestedMode_UnknownGroup_NoOps() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1);

        mTabGroupObserver.didChangeTabGroupCollapsed(
                TAB_GROUP_ID, /* isCollapsed= */ true, /* animate= */ false);

        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID));
    }

    // ============================================================================================
    // Section J: Reordering & Spatial Moves
    // ============================================================================================

    @Test
    public void testDidMoveTab_StandaloneTab_MovesDown() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // Moving mTab1 down after mTab3 emits onItemsMoved with after=mTab3.
        moveTab(mTab1, /* newIndex= */ 2);

        verify(mObserver).onItemsMoved(List.of(item(TAB1_ID)), /* after= */ item(TAB3_ID));
        assertItems(item(TAB2_ID), item(TAB3_ID), item(TAB1_ID));
    }

    @Test
    public void testDidMoveTab_StandaloneTab_MovesUp() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        // Moving mTab3 up to the start emits onItemsMoved with after=null.
        moveTab(mTab3, /* newIndex= */ 0);

        verify(mObserver).onItemsMoved(List.of(item(TAB3_ID)), /* after= */ null);
        assertItems(item(TAB3_ID), item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidMoveTab_StandaloneTab_MovesPastCollapsedGroup() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));

        // Moving mTab1 down after the group emits onItemsMoved with after=group.
        moveTab(mTab1, /* newIndex= */ 2);

        verify(mObserver)
                .onItemsMoved(
                        List.of(item(TAB1_ID)),
                        /* after= */ group(
                                TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB1_ID));
    }

    @Test
    public void testDidMoveTab_StandaloneTab_MovesPastExpandedGroup() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                item(TAB3_ID));

        // Moving mTab1 down after the group emits onItemsMoved with after=last member tab.
        moveTab(mTab1, /* newIndex= */ 2);

        verify(mObserver).onItemsMoved(List.of(item(TAB1_ID)), /* after= */ item(TAB3_ID));
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                item(TAB3_ID),
                item(TAB1_ID));
    }

    @Test
    public void testDidMoveTab_GroupedTab_Ignored() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));

        // didMoveTab ignores grouped tabs (whole group moves defer to onTabGroupMoved;
        // intra-group moves defer to didMoveWithinGroup).
        moveTab(mTab1, /* newIndex= */ 1);

        verify(mObserver, never()).onItemsMoved(any(), any());
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));
    }

    @Test
    public void testOnTabGroupMoved_CollapsedMode_MovesDown() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));

        // Moving group after mTab3 emits onItemsMoved with after=mTab3.
        moveTabGroup(TAB_GROUP_ID, /* newIndex= */ 1);

        verify(mObserver)
                .onItemsMoved(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ true)),
                        /* after= */ item(TAB3_ID));
        assertItems(
                item(TAB3_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testOnTabGroupMoved_CollapsedMode_MovesUp() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab3, mTab1, mTab2);
        assertItems(
                item(TAB3_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));

        // Moving group to the start emits onItemsMoved with after=null.
        moveTabGroup(TAB_GROUP_ID, /* newIndex= */ 0);

        verify(mObserver)
                .onItemsMoved(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ true)),
                        /* after= */ null);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));
    }

    @Test
    public void testOnTabGroupMoved_NestedMode_MovesContiguousBlock() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB2_ID),
                item(TAB3_ID));

        // Moving expanded group after mTab3 moves header and child tabs as a contiguous block.
        moveTabGroup(TAB_GROUP_ID, /* newIndex= */ 1);

        verify(mObserver)
                .onItemsMoved(
                        List.of(
                                group(
                                        TAB_GROUP_ID,
                                        /* isSelected= */ false,
                                        /* isCollapsed= */ false),
                                item(TAB1_ID),
                                item(TAB2_ID)),
                        /* after= */ item(TAB3_ID));
        assertItems(
                item(TAB3_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB2_ID));
    }

    @Test
    public void testOnTabGroupMoved_IdenticalIndex_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);

        mTabGroupObserver.onTabGroupMoved(
                TAB_GROUP_ID, /* tabModelOldIndex= */ 0, /* tabModelNewIndex= */ 0);

        verify(mObserver, never()).onItemsMoved(any(), any());
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));
    }

    @Test
    public void testDidMoveWithinGroup_CollapsedMode_UpdatesGroupPayloadOnly() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));

        // Moving mTab2 before mTab1 within the group emits TAB_GROUP_ORDER on the group card.
        moveTabWithinGroup(mTab2, /* newIndex= */ 0);

        verify(mObserver, never()).onItemsMoved(any(), any());
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                        PayloadType.TAB_GROUP_ORDER);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));
    }

    @Test
    public void testDidMoveWithinGroup_NestedMode_MovesChildTabAndUpdatesGroupPayload() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB1_ID),
                item(TAB2_ID),
                item(TAB3_ID));

        // Moving mTab2 before mTab1 within the group repositions the child item and emits
        // TAB_GROUP_ORDER on the group header.
        moveTabWithinGroup(mTab2, /* newIndex= */ 0);

        verify(mObserver)
                .onItemsMoved(
                        List.of(item(TAB2_ID)),
                        /* after= */ group(
                                TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false));
        verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                        PayloadType.TAB_GROUP_ORDER);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                item(TAB1_ID),
                item(TAB3_ID));
    }

    @Test
    public void testDidMoveWithinGroup_IdenticalIndex_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab1, mTab2);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);

        mTabGroupObserver.didMoveWithinGroup(
                mTab1, /* tabModelOldIndex= */ 0, /* tabModelNewIndex= */ 0);

        verifyNoMoreInteractions(mObserver);
        assertItems(
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true),
                item(TAB3_ID));
    }

    // ============================================================================================
    // Section K: Selection
    // ============================================================================================

    @Test
    public void testDidSelectTab_CollapsedMode_StandaloneTabToGroup_SelectsGroup() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(
                selected(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));

        selectTab(mTab2, TAB1_ID);

        InOrder inOrder = inOrder(mObserver);
        inOrder.verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.SELECTION);
        inOrder.verify(mObserver)
                .onItemUpdated(
                        group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true),
                        PayloadType.SELECTION);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true));
    }

    @Test
    public void testDidSelectTab_CollapsedMode_SameGroup_NoOps() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mSelectedTab = mTab2;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true));

        // Selecting another tab within the same group drops the update to avoid redundant churn.
        selectTab(mTab3, TAB2_ID);

        verifyNoMoreInteractions(mObserver);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ true, /* isCollapsed= */ true));
    }

    @Test
    public void testDidSelectTab_NestedMode_SelectsChildTab_NeverSelectsGroupHeader() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);
        assertItems(
                selected(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                item(TAB3_ID));

        selectTab(mTab2, TAB1_ID);

        InOrder inOrder = inOrder(mObserver);
        inOrder.verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.SELECTION);
        inOrder.verify(mObserver).onItemUpdated(selected(TAB2_ID), PayloadType.SELECTION);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                selected(TAB2_ID),
                item(TAB3_ID));
    }

    @Test
    public void testDidSelectTab_NestedMode_SwitchBetweenChildren() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        mSelectedTab = mTab2;
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                selected(TAB2_ID),
                item(TAB3_ID));

        selectTab(mTab3, TAB2_ID);

        InOrder inOrder = inOrder(mObserver);
        inOrder.verify(mObserver).onItemUpdated(item(TAB2_ID), PayloadType.SELECTION);
        inOrder.verify(mObserver).onItemUpdated(selected(TAB3_ID), PayloadType.SELECTION);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                selected(TAB3_ID));
    }

    // ============================================================================================
    // Section L: Pin State
    // ============================================================================================

    @Test
    public void testDidChangePinState_StandaloneTab_PinsTab() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);
        assertItems(item(TAB1_ID), item(TAB2_ID));

        mTab2.setIsPinned(true);
        mTabModelObserver.didChangePinState(mTab2);

        verify(mObserver).onItemUpdated(pinned(TAB2_ID), PayloadType.PIN_STATE);
        assertItems(item(TAB1_ID), pinned(TAB2_ID));
    }

    @Test
    public void testDidChangePinState_StandaloneTab_UnpinsTab() {
        mTab1.setIsPinned(true);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);
        assertItems(pinned(TAB1_ID), item(TAB2_ID));

        mTab1.setIsPinned(false);
        mTabModelObserver.didChangePinState(mTab1);

        verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.PIN_STATE);
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    @Test
    public void testDidChangePinState_NoChange_NoOps() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        // Calling didChangePinState when isPinned has not changed no-ops.
        mTabModelObserver.didChangePinState(mTab1);

        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    // ============================================================================================
    // Section M: Multi-Selection
    // ============================================================================================

    @Test
    public void testOnTabsSelectionChanged_StandaloneTabs_UpdatesMultiSelectedTabs() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2, mTab3);
        assertItems(item(TAB1_ID), item(TAB2_ID), item(TAB3_ID));

        setMultiSelectedTabs(TAB1_ID, TAB3_ID);

        verify(mObserver).onItemUpdated(multiSelected(TAB1_ID), PayloadType.MULTI_SELECTION);
        verify(mObserver).onItemUpdated(multiSelected(TAB3_ID), PayloadType.MULTI_SELECTION);
        assertItems(multiSelected(TAB1_ID), item(TAB2_ID), multiSelected(TAB3_ID));

        setMultiSelectedTabs(TAB2_ID);

        verify(mObserver).onItemUpdated(item(TAB1_ID), PayloadType.MULTI_SELECTION);
        verify(mObserver).onItemUpdated(multiSelected(TAB2_ID), PayloadType.MULTI_SELECTION);
        verify(mObserver).onItemUpdated(item(TAB3_ID), PayloadType.MULTI_SELECTION);
        assertItems(item(TAB1_ID), multiSelected(TAB2_ID), item(TAB3_ID));
    }

    @Test
    public void testOnTabsSelectionChanged_NestedMode_UpdatesChildTabs_SkipsGroupHeader() {
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        mCollapsed.put(TAB_GROUP_ID, /* value= */ false);
        setUpProviderWithTabs(/* supportsGroupExpansion= */ true, mTab1, mTab2, mTab3);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                item(TAB2_ID),
                item(TAB3_ID));

        setMultiSelectedTabs(TAB2_ID);

        verify(mObserver).onItemUpdated(multiSelected(TAB2_ID), PayloadType.MULTI_SELECTION);
        verifyNoMoreInteractions(mObserver);
        assertItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ false),
                multiSelected(TAB2_ID),
                item(TAB3_ID));
    }

    @Test
    public void testOnTabsSelectionChanged_NoChange_NoOps() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1, mTab2);

        // Invoking onTabsSelectionChanged when selection set is unchanged emits nothing.
        mTabModelObserver.onTabsSelectionChanged();

        verifyNoMoreInteractions(mObserver);
        assertItems(item(TAB1_ID), item(TAB2_ID));
    }

    // ============================================================================================
    // Section N: Concurrency, Lifecycle & State Recovery
    // ============================================================================================

    @Test
    public void testRestoreCompleted_RequestsDataReset() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);
        groupTabs(TAB_GROUP_ID, mTab2, mTab3);
        setModelTabs(mTab1, mTab2, mTab3);

        mTabModelObserver.restoreCompleted();

        assertResetWithItems(
                item(TAB1_ID),
                group(TAB_GROUP_ID, /* isSelected= */ false, /* isCollapsed= */ true));
    }

    @Test
    public void testTabModelSwitch_DetachesAndRebuildsOnReset() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        // Switch TabModel.
        mOtherModelTabs.add(mTab2);
        mTabModelSupplier.set(mOtherTabModel);

        verify(mTabModel).removeObserver(mTabModelObserver);
        verify(mTabModel).removeTabGroupObserver(mTabGroupObserver);
        verifyNoMoreInteractions(mObserver);
        assertItems();

        mProvider.requestDataReset();
        captureObserver(mOtherTabModel);
        assertResetWithItems(item(TAB2_ID));
    }

    @Test
    public void testStopAndDestroy_DetachesAndClearsState() {
        setUpProviderWithTabs(/* supportsGroupExpansion= */ false, mTab1);

        mProvider.stop();
        verify(mTabModel).removeObserver(mTabModelObserver);
        verify(mTabModel).removeTabGroupObserver(mTabGroupObserver);
        assertItems();

        mProvider.requestDataReset();
        assertResetWithItems(item(TAB1_ID));

        mProvider.destroy();
        assertItems();
    }

    // ============================================================================================
    // Test Helpers
    // ============================================================================================

    private void createProvider(boolean supportsGroupExpansion) {
        initProvider(new GroupedTabListDataProvider(mTabModelSupplier, supportsGroupExpansion));
    }

    private void setUpProviderWithTabs(boolean supportsGroupExpansion, Tab... tabs) {
        initProviderWithTabs(
                new GroupedTabListDataProvider(mTabModelSupplier, supportsGroupExpansion), tabs);
    }
}
