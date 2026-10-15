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
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

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
