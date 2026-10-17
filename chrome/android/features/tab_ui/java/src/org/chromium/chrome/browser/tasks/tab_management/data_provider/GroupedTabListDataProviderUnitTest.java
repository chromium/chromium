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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabListDataObserver.PayloadType;

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

        // Flipping state returns a new instance with the updated field.
        TabGroupItem selectedItem = item.withSelected(/* isSelected= */ true);
        assertNotSame(item, selectedItem);
        assertTrue(selectedItem.isSelected());
        assertTrue(selectedItem.isCollapsed());

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
    // Section K: Concurrency, Lifecycle & State Recovery
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
