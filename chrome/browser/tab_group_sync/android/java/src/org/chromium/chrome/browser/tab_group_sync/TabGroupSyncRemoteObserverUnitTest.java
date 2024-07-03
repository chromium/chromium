// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController.TabCreationDelegate;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.OpeningSource;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@link TabGroupSyncRemoteObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH)
public class TabGroupSyncRemoteObserverUnitTest {
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final Token TOKEN_2 = new Token(4, 4);
    private static final int TAB_ID_1 = 1;
    private static final int ROOT_ID_1 = 1;
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 = new LocalTabGroupId(TOKEN_1);
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_2 = new LocalTabGroupId(TOKEN_2);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private PrefService mPrefService;
    private @Mock Supplier<Boolean> mIsActiveWindowSupplier;

    private NavigationTracker mNavigationTracker;
    @Mock private LocalTabGroupMutationHelper mLocalMutationHelper;
    private TabGroupSyncRemoteObserver mRemoteObserver;
    private TestTabCreationDelegate mTabCreationDelegate;

    private boolean mEnabledLocalObservers;

    @Before
    public void setUp() {
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mNavigationTracker = new NavigationTracker();
        mTabCreationDelegate = new TestTabCreationDelegate();
        mRemoteObserver =
                new TabGroupSyncRemoteObserver(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mLocalMutationHelper,
                        enable -> {
                            mEnabledLocalObservers = enable;
                        },
                        mPrefService,
                        mIsActiveWindowSupplier);
        mEnabledLocalObservers = true;

        when(mTabGroupModelFilter.getRootIdFromStableId(any())).thenReturn(Tab.INVALID_TAB_ID);
        when(mTabGroupModelFilter.getRootIdFromStableId(eq(TOKEN_1))).thenReturn(ROOT_ID_1);
        when(mTabGroupModelFilter.getStableIdFromRootId(eq(ROOT_ID_1))).thenReturn(TOKEN_1);
        when(mPrefService.getBoolean(eq(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS))).thenReturn(true);
        when(mIsActiveWindowSupplier.get()).thenReturn(true);
    }

    @After
    public void tearDown() {
        // At the end of every method, the local observer should be reset back to observing.
        Assert.assertTrue(mEnabledLocalObservers);
    }

    private void addOneTab() {
        mTabModel.addTab(TAB_ID_1);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTabModel.getTabAt(0));
        when(mTabGroupModelFilter.getRelatedTabListForRootId(eq(ROOT_ID_1))).thenReturn(tabs);
    }

    @Test
    public void testTabGroupAdded() {
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        mRemoteObserver.onTabGroupAdded(savedTabGroup, TriggerSource.REMOTE);
        verify(mLocalMutationHelper)
                .createNewTabGroup(any(), eq(OpeningSource.AUTO_OPENED_FROM_SYNC));
    }

    @Test
    public void testTabGroupAddedOnNonActiveWindow() {
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        when(mIsActiveWindowSupplier.get()).thenReturn(false);

        mRemoteObserver.onTabGroupAdded(savedTabGroup, TriggerSource.REMOTE);
        verify(mLocalMutationHelper, never()).createNewTabGroup(any(), anyInt());
    }

    @Test
    public void testTabGroupAddedWithAutoOpenOff() {
        when(mPrefService.getBoolean(eq(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS))).thenReturn(false);

        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        mRemoteObserver.onTabGroupAdded(savedTabGroup, TriggerSource.REMOTE);
        verify(mLocalMutationHelper, never()).createNewTabGroup(any(), anyInt());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH)
    public void testAutoOpenKillSwitch() {
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        mRemoteObserver.onTabGroupAdded(savedTabGroup, TriggerSource.REMOTE);
        verify(mLocalMutationHelper, never()).createNewTabGroup(any(), anyInt());
    }

    @Test
    public void testTabGroupVisualsUpdated() {
        addOneTab();
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_1;
        mRemoteObserver.onTabGroupUpdated(savedTabGroup, TriggerSource.REMOTE);
        verify(mLocalMutationHelper).updateTabGroup(any());
    }

    @Test
    public void testTabGroupUpdatedForDifferentWindow() {
        addOneTab();
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_2;
        mRemoteObserver.onTabGroupUpdated(savedTabGroup, TriggerSource.REMOTE);
        verify(mLocalMutationHelper, never()).updateTabGroup(any());
    }

    @Test
    public void testTabGroupRemoved() {
        addOneTab();
        mRemoteObserver.onTabGroupRemoved(LOCAL_TAB_GROUP_ID_1, TriggerSource.REMOTE);
        verify(mLocalMutationHelper).closeTabGroup(any(), eq(ClosingSource.DELETED_FROM_SYNC));
    }

    @Test
    public void testTabGroupRemovedForDifferentWindow() {
        addOneTab();
        mRemoteObserver.onTabGroupRemoved(LOCAL_TAB_GROUP_ID_2, TriggerSource.REMOTE);
        verify(mLocalMutationHelper, never()).closeTabGroup(any(), anyInt());
    }

    @Test
    public void testFilterOutUpdatesForLocal() {
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        mRemoteObserver.onTabGroupAdded(savedTabGroup, TriggerSource.LOCAL);
        verify(mLocalMutationHelper, never()).createNewTabGroup(any(), anyInt());
        mRemoteObserver.onTabGroupUpdated(savedTabGroup, TriggerSource.LOCAL);
        verify(mLocalMutationHelper, never()).updateTabGroup(any());
        mRemoteObserver.onTabGroupRemoved(LOCAL_TAB_GROUP_ID_1, TriggerSource.LOCAL);
        verify(mLocalMutationHelper, never()).closeTabGroup(any(), anyInt());
    }

    private class TestTabCreationDelegate implements TabCreationDelegate {
        private int mNextTabId;

        @Override
        public Tab createBackgroundTab(GURL url, String title, Tab parent, int position) {
            MockTab tab = new MockTab(++mNextTabId, mProfile);
            tab.setIsInitialized(true);
            tab.setUrl(url);
            tab.setRootId(parent == null ? tab.getId() : parent.getRootId());
            tab.setTitle("Tab Title");
            mTabModel.addTab(
                    tab, -1, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
            return tab;
        }

        @Override
        public void navigateToUrl(Tab tab, GURL url, String title, boolean isForegroundTab) {}
    }
}
