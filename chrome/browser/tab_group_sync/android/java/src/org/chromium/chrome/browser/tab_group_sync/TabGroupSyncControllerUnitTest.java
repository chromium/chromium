// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;

/** Unit tests for the {@link TabGroupSyncController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncControllerUnitTest {
    private static final Token TOKEN_1 = new Token(2, 3);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock TabCreatorManager mTabCreatorManager;
    private @Mock TabGroupSyncService mTabGroupSyncService;
    private @Mock Profile mProfile;
    private MockTabModel mTabModel;
    private @Mock TabGroupModelFilter mTabGroupModelFilter;
    private @Mock PrefService mPrefService;
    private @Mock Supplier<Boolean> mIsActiveWindowSupplier;
    private TabGroupSyncController mController;

    private @Captor ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    private @Captor ArgumentCaptor<TabGroupSyncService.Observer> mTabGroupSyncServiceObserverCaptor;

    @Before
    public void setUp() {
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        TabModelFilterProvider tabModelFilterProvider = mock(TabModelFilterProvider.class);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(tabModelFilterProvider);
        when(tabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabGroupModelFilter);
        doNothing().when(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        doNothing()
                .when(mTabGroupSyncService)
                .addObserver(mTabGroupSyncServiceObserverCaptor.capture());
        mController =
                new TabGroupSyncController(
                        mTabModelSelector,
                        mTabCreatorManager,
                        mTabGroupSyncService,
                        mPrefService,
                        mIsActiveWindowSupplier);
    }

    @After
    public void tearDown() {
        mController.destroy();
    }

    @Test
    public void testInitialization() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(new ArrayList<>());
        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabGroupSyncServiceObserverCaptor.getValue().onInitialized();
        verify(mTabGroupSyncService, times(2)).addObserver(any());
        verify(mTabGroupSyncService, times(1)).getDeletedGroupIds();
    }
}
