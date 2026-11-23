// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupId;

import java.util.ArrayList;
import java.util.function.Supplier;

/** Unit tests for the {@link TabGroupSyncControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupSyncControllerImplUnitTest {
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final int TAB_ID_1 = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock TabModelSelector mTabModelSelector;
    private @Mock TabCreator mTabCreator;
    private @Mock TabGroupSyncService mTabGroupSyncService;
    private @Mock TabStateStorageService mStorageService;
    private @Mock ScopedStorageBatch mStorageBatch;
    private @Mock Profile mProfile;
    private MockTabModel mTabModel;
    private @Mock PrefService mPrefService;
    private @Mock Supplier<Boolean> mIsActiveWindowSupplier;
    private TabGroupSyncController mController;

    private @Captor ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    private @Captor ArgumentCaptor<TabGroupSyncService.Observer> mTabGroupSyncServiceObserverCaptor;
    private @Mock Tab mTab1;
    @Spy private Callback<Boolean> mStartupCallback;
    @Spy private Callback<Boolean> mPostInitCallback;

    @Before
    public void setUp() {
        when(mStorageService.createBatch()).thenReturn(mStorageBatch);
        TabStateStorageServiceFactory.setForTesting(mStorageService);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        doReturn(ObservableSuppliers.createMonotonic(mTabModel))
                .when(mTabModelSelector)
                .getCurrentTabModelSupplier();
        doNothing().when(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        mTabModel.setTabCreatorForTesting(mTabCreator);
        doNothing()
                .when(mTabGroupSyncService)
                .addObserver(mTabGroupSyncServiceObserverCaptor.capture());

        // Prepare mock tab.
        when(mTab1.getTabGroupId()).thenReturn(TOKEN_1);
        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTabModel.containsTabGroup(TabGroupId.fromOptionalToken(TOKEN_1))).thenReturn(true);
        when(mTabCreator.createNewTab(any(), anyString(), anyInt(), any(), anyInt()))
                .thenReturn(mTab1);
    }

    @After
    public void tearDown() {
        if (mController != null) mController.destroy();
        for (TabGroupSyncService.Observer observer :
                mTabGroupSyncServiceObserverCaptor.getAllValues()) {
            verify(mTabGroupSyncService, atLeastOnce()).removeObserver(observer);
        }
    }

    private void createController() {
        mController =
                new TabGroupSyncControllerImpl(
                        mTabModelSelector,
                        mTabGroupSyncService,
                        mPrefService,
                        mIsActiveWindowSupplier);
    }

    @Test
    public void testInitialization() {
        // Set tab model as initialized.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(new ArrayList<>());
        // Create controller.
        createController();
        // Init sync backend.
        mTabGroupSyncServiceObserverCaptor.getValue().onInitialized();

        // Observe calls to the sync backend on controller startup.
        verify(mTabGroupSyncService, times(2)).addObserver(any());
        verify(mTabGroupSyncService, times(1)).getDeletedGroupIds();
    }

    @Test
    public void testOpenTabGroup() {
        // Init both tab model and sync backend.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(new ArrayList<>());
        createController();
        mTabGroupSyncServiceObserverCaptor.getValue().onInitialized();

        // Open a tab group. It should invoke TabModel to create a tab group.
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        when(mTabGroupSyncService.getGroup(savedTabGroup.syncId)).thenReturn(savedTabGroup);
        mController.openTabGroup(savedTabGroup.syncId);
        verify(mTabModel, times(1)).mergeListOfTabsToGroup(any(), any(), anyInt());
    }

    @Test
    public void testOpenTabGroup_beforeSyncBackendInit() {
        // Init tab model but not the sync backend.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(new ArrayList<>());
        createController();

        // Open a tab group. It should not invoke TabModel to create groups.
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        when(mTabGroupSyncService.getGroup(savedTabGroup.syncId)).thenReturn(savedTabGroup);
        mController.openTabGroup(savedTabGroup.syncId);
        verify(mTabModel, never()).mergeListOfTabsToGroup(any(), any(), anyInt());
    }

    @Test
    public void testGetStartupSyncInitializedSupplier_fulfilledAfterSyncBackendInitialized() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(new ArrayList<>());
        createController();

        assertNull(mController.getStartupSyncInitializedSupplier().get());
        mController.getStartupSyncInitializedSupplier().runSyncOrOnAvailable(mStartupCallback);
        verify(mStartupCallback, never()).onResult(any());

        mTabGroupSyncServiceObserverCaptor.getValue().onInitialized();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertTrue(Boolean.TRUE.equals(mController.getStartupSyncInitializedSupplier().get()));
        InOrder inOrder = inOrder(mTabGroupSyncService, mStartupCallback);
        inOrder.verify(mTabGroupSyncService).getDeletedGroupIds();
        inOrder.verify(mStartupCallback).onResult(true);

        mController.getStartupSyncInitializedSupplier().runSyncOrOnAvailable(mPostInitCallback);
        verify(mPostInitCallback, times(1)).onResult(true);
    }

    @Test
    public void testGetStartupSyncInitializedSupplier_unfulfilledOnDestroy() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        createController();

        mController.getStartupSyncInitializedSupplier().runSyncOrOnAvailable(mStartupCallback);

        mController.destroy();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        verify(mTabGroupSyncService).removeObserver(mTabGroupSyncServiceObserverCaptor.getValue());
        assertNull(mController.getStartupSyncInitializedSupplier().get());
        verify(mStartupCallback, never()).onResult(any());
        mController = null;
    }
}
