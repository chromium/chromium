// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;

/** Unit tests for the {@link TabGroupSyncController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncControllerUnitTest {
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final int TAB_ID_1 = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock TabCreatorManager mTabCreatorManager;
    private @Mock TabCreator mTabCreator;
    private @Mock TabGroupSyncService mTabGroupSyncService;
    private @Mock Profile mProfile;
    private MockTabModel mTabModel;
    private @Mock TabGroupModelFilter mTabGroupModelFilter;
    private @Mock PrefService mPrefService;
    private @Mock Supplier<Boolean> mIsActiveWindowSupplier;
    private TabGroupSyncController mController;

    private @Captor ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    private @Captor ArgumentCaptor<TabGroupSyncService.Observer> mTabGroupSyncServiceObserverCaptor;
    private @Mock Tab mTab1;

    @Before
    public void setUp() {
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        TabGroupModelFilterProvider tabGroupModelFilterProvider =
                mock(TabGroupModelFilterProvider.class);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(tabGroupModelFilterProvider);
        when(tabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mTabGroupModelFilter);
        doNothing().when(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mTabCreator);
        doNothing()
                .when(mTabGroupSyncService)
                .addObserver(mTabGroupSyncServiceObserverCaptor.capture());

        // Prepare mock tab.
        Mockito.doReturn(TAB_ID_1).when(mTab1).getId();
        Mockito.doReturn(TAB_ID_1).when(mTab1).getRootId();
        when(mTabGroupModelFilter.getStableIdFromRootId(TAB_ID_1)).thenReturn(TOKEN_1);
        when(mTabCreator.createNewTab(any(), anyString(), anyInt(), any(), anyInt()))
                .thenReturn(mTab1);
    }

    @After
    public void tearDown() {
        mController.destroy();
    }

    private void createController() {
        mController =
                new TabGroupSyncController(
                        mTabModelSelector,
                        mTabCreatorManager,
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

        // Open a tab group. It should invoke TabGroupModelFilter to create a tab group.
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        when(mTabGroupSyncService.getGroup(savedTabGroup.syncId)).thenReturn(savedTabGroup);
        mController.openTabGroup(savedTabGroup.syncId);
        verify(mTabGroupModelFilter, times(1)).mergeListOfTabsToGroup(any(), any(), anyBoolean());
    }

    @Test
    public void testOpenTabGroup_beforeSyncBackendInit() {
        // Init tab model but not the sync backend.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(new ArrayList<>());
        createController();

        // Open a tab group. It should not invoke TabGroupModelFilter to create groups.
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        when(mTabGroupSyncService.getGroup(savedTabGroup.syncId)).thenReturn(savedTabGroup);
        mController.openTabGroup(savedTabGroup.syncId);
        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(any(), any(), anyBoolean());
    }
}
