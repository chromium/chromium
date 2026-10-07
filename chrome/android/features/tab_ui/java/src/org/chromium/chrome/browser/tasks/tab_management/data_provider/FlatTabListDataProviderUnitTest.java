// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

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

    private final List<Tab> mModelTabs = new ArrayList<>();
    private final List<Tab> mOtherModelTabs = new ArrayList<>();
    private final Predicate<Tab> mInCurrentGroupFilter = this::isInCurrentGroup;
    private final SettableNullableObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createNullable();

    private @Nullable Token mCurrentTabGroupId = TAB_GROUP_ID;
    private @Nullable Tab mSelectedTab;
    private TabModelObserver mTabModelObserver;
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
    }

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
        when(mTabModel.isTabModelRestored()).thenReturn(false);
        mSelectedTab = mTab1;
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        assertItems();

        when(mTabModel.isTabModelRestored()).thenReturn(true);
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
        assertResetWithItems(item(TAB1_ID), item(TAB2_ID));

        // Switching to mOtherTabModel detaches from mTabModel and waits for the next
        // requestDataReset() before attaching to mOtherTabModel.
        clearInvocations(mObserver);
        mOtherModelTabs.add(mTab3);
        mTabModelSupplier.set(mOtherTabModel);
        verify(mTabModel).removeObserver(firstModelObserver);
        verify(mOtherTabModel, never()).addObserver(any());
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
        verifyNoInteractions(mObserver);
        assertItems();

        mProvider.requestDataReset();
        verify(mOtherTabModel, times(2)).addObserver(mTabModelObserver);
        assertResetWithItems(item(TAB3_ID));
    }

    @Test
    public void testObserversAndDestroy_ManagesNotificationsAndDetaches() {
        setUpProviderWithTabs(/* filter= */ null, mTab1, mTab2);
        // Exercise default no-op TabListDataObserver#onDataReset alongside a second mock observer.
        TabListDataObserver defaultObserver = new TabListDataObserver() {};
        mProvider.addObserver(defaultObserver);
        mProvider.addObserver(mSecondObserver);

        // Repeated requestDataReset() on the same TabModel does not re-register mTabModelObserver.
        mProvider.requestDataReset();
        List<TabListItem> expected = List.of(item(TAB1_ID), item(TAB2_ID));
        verify(mTabModel, times(1)).addObserver(mTabModelObserver);
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
        assertItems();

        mProvider.requestDataReset();
        verifyNoInteractions(mSecondObserver);
    }

    private void stubBackedTabModel(TabModel model, List<Tab> tabs) {
        when(model.isTabModelRestored()).thenReturn(true);
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
    }

    private void setModelTabs(Tab... tabs) {
        mModelTabs.clear();
        mModelTabs.addAll(Arrays.asList(tabs));
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
