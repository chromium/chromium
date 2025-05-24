// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doNothing;
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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService.Observer;

/** Unit tests for {@link ArchivedTabCountSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ArchivedTabCountSupplierUnitTest {
    private static final int INITIAL_TAB_COUNT = 1;
    private static final int TAB_MODEL_TAB_COUNT = 2;
    private static final String SYNC_GROUP_ID = "test_sync_group_id1";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabGroupSyncService mTabGroupSyncService;

    @Captor ArgumentCaptor<Observer> mTabGroupSyncServiceObserverCaptor;

    private final ObservableSupplierImpl<Integer> mArchivedTabModelTabCountSupplier =
            new ObservableSupplierImpl<>(INITIAL_TAB_COUNT);
    private ArchivedTabCountSupplier mArchivedTabCountSupplier;

    @Before
    public void setUp() {
        when(mTabModel.getTabCountSupplier()).thenReturn(mArchivedTabModelTabCountSupplier);
        doNothing()
                .when(mTabGroupSyncService)
                .addObserver(mTabGroupSyncServiceObserverCaptor.capture());

        mArchivedTabCountSupplier = new ArchivedTabCountSupplier();
        mArchivedTabCountSupplier.setupInternalObservers(mTabModel, mTabGroupSyncService);
    }

    @Test
    public void testTabModelUpdate() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = SYNC_GROUP_ID;
        savedTabGroup.archivalTimeMs = System.currentTimeMillis();

        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID)).thenReturn(savedTabGroup);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID});
        when(mTabModel.getCount()).thenReturn(TAB_MODEL_TAB_COUNT);

        mArchivedTabModelTabCountSupplier.set(TAB_MODEL_TAB_COUNT);
        assertEquals(TAB_MODEL_TAB_COUNT + 1, mArchivedTabCountSupplier.get().intValue());
    }

    @Test
    public void testTabGroupSyncUpdate() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = SYNC_GROUP_ID;
        savedTabGroup.archivalTimeMs = System.currentTimeMillis();

        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID)).thenReturn(savedTabGroup);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID});
        when(mTabModel.getCount()).thenReturn(TAB_MODEL_TAB_COUNT);

        mTabGroupSyncServiceObserverCaptor.getValue().onInitialized();
        assertEquals(TAB_MODEL_TAB_COUNT + 1, mArchivedTabCountSupplier.get().intValue());
    }

    @Test
    public void testAllObserversUpdate() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = SYNC_GROUP_ID;
        savedTabGroup.archivalTimeMs = System.currentTimeMillis();

        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID)).thenReturn(savedTabGroup);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID});
        when(mTabModel.getCount()).thenReturn(INITIAL_TAB_COUNT);

        mTabGroupSyncServiceObserverCaptor.getValue().onInitialized();
        assertEquals(INITIAL_TAB_COUNT + 1, mArchivedTabCountSupplier.get().intValue());

        when(mTabModel.getCount()).thenReturn(TAB_MODEL_TAB_COUNT);

        mArchivedTabModelTabCountSupplier.set(TAB_MODEL_TAB_COUNT);
        assertEquals(TAB_MODEL_TAB_COUNT + 1, mArchivedTabCountSupplier.get().intValue());
    }
}
