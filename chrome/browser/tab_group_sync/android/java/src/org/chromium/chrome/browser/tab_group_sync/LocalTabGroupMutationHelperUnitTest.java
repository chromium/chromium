// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController.TabCreationDelegate;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.EventDetails;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.OpeningSource;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@link LocalTabGroupMutationHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocalTabGroupMutationHelperUnitTest {
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final Token TOKEN_2 = new Token(4, 4);
    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int ROOT_ID_1 = 1;
    private static final int ROOT_ID_2 = 2;
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 = new LocalTabGroupId(TOKEN_1);
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_2 = new LocalTabGroupId(TOKEN_2);
    private static final String TAB_TITLE_1 = "Tab Title 1";
    private static final GURL TAB_URL_1 = new GURL("https://url1.com");
    private static final GURL TAB_URL_2 = new GURL("https://url2.com");
    private static final GURL UNSYNCABLE_URL_1 = new GURL("chrome://flags");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupSyncUtilsJni mTabGroupSyncUtilsJni;
    private LocalTabGroupMutationHelper mLocalMutationHelper;
    private TestTabCreationDelegate mTabCreationDelegate;

    private Tab mTab1;
    private Tab mTab2;
    private @Captor ArgumentCaptor<EventDetails> mEventDetailsCaptor;

    @Before
    public void setUp() {
        mJniMocker.mock(TabGroupSyncUtilsJni.TEST_HOOKS, mTabGroupSyncUtilsJni);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mTabCreationDelegate = spy(new TestTabCreationDelegate());
        mLocalMutationHelper =
                new LocalTabGroupMutationHelper(
                        mTabGroupModelFilter, mTabGroupSyncService, mTabCreationDelegate);

        when(mTabGroupModelFilter.getRootIdFromStableId(any())).thenReturn(Tab.INVALID_TAB_ID);
        when(mTabGroupModelFilter.getRootIdFromStableId(eq(TOKEN_1))).thenReturn(ROOT_ID_1);
        when(mTabGroupModelFilter.getStableIdFromRootId(eq(ROOT_ID_1))).thenReturn(TOKEN_1);

        Mockito.doNothing()
                .when(mTabGroupSyncService)
                .recordTabGroupEvent(mEventDetailsCaptor.capture());

        mTab1 = prepareTab(TAB_ID_1, ROOT_ID_1);
        mTab2 = prepareTab(TAB_ID_2, ROOT_ID_2);
        when(mTab1.getUrl()).thenReturn(TAB_URL_1);
        when(mTab1.getTitle()).thenReturn(TAB_TITLE_1);
        when(mTab2.getUrl()).thenReturn(TAB_URL_2);
    }

    @After
    public void tearDown() {}

    private void addOneTab() {
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        Mockito.doReturn(TOKEN_1).when(mTab1).getTabGroupId();
        when(mTabGroupModelFilter.getRelatedTabListForRootId(eq(ROOT_ID_1))).thenReturn(tabs);
    }

    private Tab prepareTab(int tabId, int rootId) {
        Tab tab = Mockito.mock(Tab.class);
        Mockito.doReturn(tabId).when(tab).getId();
        Mockito.doReturn(rootId).when(tab).getRootId();
        Mockito.doReturn(tab).when(mTabModel).getTabById(tabId);
        return tab;
    }

    private SavedTabGroup createOneSavedTabGroup(
            LocalTabGroupId localTabGroupId, Integer[] tabIds) {
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        savedTabGroup.localId = localTabGroupId;
        for (int i = 0; i < tabIds.length; i++) {
            savedTabGroup.savedTabs.get(i).localId = tabIds[i];
        }

        // The final group should match tabIds.
        savedTabGroup.savedTabs.subList(tabIds.length, savedTabGroup.savedTabs.size()).clear();
        Assert.assertEquals(savedTabGroup.savedTabs.size(), tabIds.length);
        return savedTabGroup;
    }

    @Test
    public void testCreateNewTabGroup() {
        SavedTabGroup savedTabGroup = createOneSavedTabGroup(null, new Integer[] {null, null});
        mLocalMutationHelper.createNewTabGroup(savedTabGroup, OpeningSource.AUTO_OPENED_FROM_SYNC);

        // Verify calls to create local tab group, and update ID mappings for group and tabs.
        verify(mTabGroupModelFilter).mergeListOfTabsToGroup(anyList(), any(), eq(false));
        verify(mTabGroupModelFilter).setTabGroupColor(anyInt(), anyInt());
        verify(mTabGroupModelFilter).setTabGroupTitle(anyInt(), any());
        verify(mTabGroupModelFilter).setTabGroupCollapsed(anyInt(), eq(true));
        verify(mTabGroupSyncService)
                .updateLocalTabGroupMapping(any(), any(), eq(OpeningSource.AUTO_OPENED_FROM_SYNC));
        verify(mTabGroupSyncService, times(2)).updateLocalTabId(any(), any(), anyInt());
    }

    @Test
    public void testCreateNewTabGroup_SingleTab() {
        SavedTabGroup savedTabGroup = createOneSavedTabGroup(null, new Integer[] {null});
        mLocalMutationHelper.createNewTabGroup(savedTabGroup, OpeningSource.OPENED_FROM_REVISIT_UI);

        // Verify calls to create local tab group, and update ID mappings for group and tabs.
        verify(mTabGroupModelFilter).createSingleTabGroup(any(), eq(false));
        verify(mTabGroupModelFilter).setTabGroupColor(anyInt(), anyInt());
        verify(mTabGroupModelFilter).setTabGroupTitle(anyInt(), any());
        verify(mTabGroupSyncService)
                .updateLocalTabGroupMapping(any(), any(), eq(OpeningSource.OPENED_FROM_REVISIT_UI));
        verify(mTabGroupSyncService, times(1)).updateLocalTabId(any(), any(), anyInt());
    }

    @Test
    public void testUpdateTabGroupUpdatesVisuals() {
        addOneTab();
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {null, null});
        savedTabGroup.title = "Updated group";
        mLocalMutationHelper.updateTabGroup(savedTabGroup);

        verify(mTabGroupModelFilter).setTabGroupTitle(eq(ROOT_ID_1), eq(savedTabGroup.title));
        verify(mTabGroupModelFilter).setTabGroupColor(eq(ROOT_ID_1), anyInt());
    }

    @Test
    public void testUpdateTabGroup_CloseLocalTabsThatDoNotExistInSync() {
        // One local group with one tab syncing.
        addOneTab();

        // One saved group with two tabs: both with no local mapping.
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {null, null});
        mLocalMutationHelper.updateTabGroup(savedTabGroup);

        verify(mTabModel).closeTabs(argThat(params -> params.tabs.size() == 1));
    }

    @Test
    public void testUpdateTabGroup_AddTabsFromSync() {
        // One local group with one tab syncing.
        addOneTab();
        when(mTabGroupModelFilter.getTabGroupCollapsed(ROOT_ID_1)).thenReturn(true);

        // One saved group with two tabs: both with no local mapping.
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {null, null});
        mLocalMutationHelper.updateTabGroup(savedTabGroup);

        // Collapsed must be re-set after the merge.
        InOrder inOrder = inOrder(mTabGroupModelFilter, mTabModel, mTabGroupSyncService);
        verify(mTabCreationDelegate, times(2))
                .createBackgroundTab(any(), anyString(), any(), anyInt());
        inOrder.verify(mTabGroupModelFilter, times(2))
                .mergeListOfTabsToGroup(
                        anyList(), argThat(tab -> tab.getId() == ROOT_ID_1), eq(false));
        verify(mTabGroupSyncService, times(1))
                .updateLocalTabId(eq(LOCAL_TAB_GROUP_ID_1), any(), eq(TAB_ID_1));
        inOrder.verify(mTabModel).closeTabs(argThat(params -> params.tabs.size() == 1));
        inOrder.verify(mTabGroupModelFilter).setTabGroupCollapsed(ROOT_ID_1, true);
    }

    @Test
    public void testUpdateTabGroup_UpdateExistingTab_Navigate() {
        // One local group with one tab syncing.
        addOneTab();

        // One saved group with one tabs mapped to the local tab.
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {TAB_ID_1});
        SavedTabGroupTab savedTab = savedTabGroup.savedTabs.get(0);
        savedTab.url = TAB_URL_2;
        savedTab.title = TAB_TITLE_1;

        when(mTabGroupSyncUtilsJni.isUrlInTabRedirectChain(
                        any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), eq(TAB_URL_2)))
                .thenReturn(false);
        mLocalMutationHelper.updateTabGroup(savedTabGroup);

        verify(mTabCreationDelegate, never())
                .createBackgroundTab(any(), anyString(), any(), anyInt());
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyBoolean());
        verify(mTabGroupSyncService, never()).updateLocalTabId(any(), any(), anyInt());
        verify(mTabModel, never()).closeTabs(any());
        verify(mTabCreationDelegate, times(1))
                .navigateToUrl(any(), eq(TAB_URL_2), eq(TAB_TITLE_1), eq(false));
    }

    @Test
    public void testUpdateTabGroup_UpdateExistingTab_SkipNavigateSameUrl() {
        // One local group with one tab syncing.
        addOneTab();

        // One saved group with one tabs mapped to the local tab. It has same URl as existing.
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {TAB_ID_1});
        SavedTabGroupTab savedTab = savedTabGroup.savedTabs.get(0);
        savedTab.url = TAB_URL_1;

        mLocalMutationHelper.updateTabGroup(savedTabGroup);

        verify(mTabCreationDelegate, never())
                .createBackgroundTab(any(), anyString(), any(), anyInt());
        verify(mTabCreationDelegate, never())
                .navigateToUrl(any(), any(), anyString(), anyBoolean());
    }

    @Test
    public void testUpdateTabGroup_UpdateExistingTab_SkipNavigateUrlInTabRedirectChain() {
        // One local group with one tab syncing.
        addOneTab();

        // One saved group with one tabs mapped to the local tab. The URL is in the current
        // tab's redirect chain.
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {TAB_ID_1});
        SavedTabGroupTab savedTab = savedTabGroup.savedTabs.get(0);
        savedTab.url = TAB_URL_2;
        savedTab.title = TAB_TITLE_1;

        when(mTabGroupSyncUtilsJni.isUrlInTabRedirectChain(
                        any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), eq(TAB_URL_2)))
                .thenReturn(true);
        mLocalMutationHelper.updateTabGroup(savedTabGroup);

        verify(mTabCreationDelegate, never())
                .createBackgroundTab(any(), anyString(), any(), anyInt());
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyBoolean());
        verify(mTabGroupSyncService, never()).updateLocalTabId(any(), any(), anyInt());
        verify(mTabModel, never()).closeTabs(any());
        verify(mTabCreationDelegate, never())
                .navigateToUrl(any(), any(), anyString(), anyBoolean());
    }

    @Test
    public void testUpdateTabGroup_UpdateExistingTab_UnsyncableUrlAreNotClobberedWithNTPUrl() {
        // One local group with one tab syncing.
        addOneTab();
        when(mTab1.getUrl()).thenReturn(UNSYNCABLE_URL_1);

        // One saved group with one tabs mapped to the local tab.
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {TAB_ID_1});
        SavedTabGroupTab savedTab = savedTabGroup.savedTabs.get(0);
        savedTab.url = TabGroupSyncUtils.UNSAVEABLE_URL_OVERRIDE;

        mLocalMutationHelper.updateTabGroup(savedTabGroup);
        verify(mTabCreationDelegate, never())
                .navigateToUrl(any(), any(), anyString(), anyBoolean());
    }

    @Test
    public void
            testUpdateTabGroup_UpdateExistingTab_UnsyncableUrlAreOverwrittenWithValidNonNTPUrl() {
        // One local group with one tab syncing.
        addOneTab();
        when(mTab1.getUrl()).thenReturn(UNSYNCABLE_URL_1);

        // One saved group with one tabs mapped to the local tab.
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {TAB_ID_1});
        SavedTabGroupTab savedTab = savedTabGroup.savedTabs.get(0);
        savedTab.url = TAB_URL_2;

        mLocalMutationHelper.updateTabGroup(savedTabGroup);
        verify(mTabCreationDelegate, times(1))
                .navigateToUrl(any(), eq(TAB_URL_2), anyString(), eq(false));
    }

    @Test
    public void testUpdateTabGroup_UpdateExistingTabInWrongGroup() {
        // One local group with one tab syncing.
        addOneTab();
        // One saved group with one tabs mapped to the tab but in wrong group.
        SavedTabGroup savedTabGroup =
                createOneSavedTabGroup(LOCAL_TAB_GROUP_ID_1, new Integer[] {TAB_ID_1});
        when(mTab1.getRootId()).thenReturn(ROOT_ID_2);
        mLocalMutationHelper.updateTabGroup(savedTabGroup);

        verify(mTabCreationDelegate, times(1))
                .createBackgroundTab(any(), anyString(), any(), anyInt());
        verify(mTabGroupModelFilter, times(1))
                .mergeListOfTabsToGroup(anyList(), any(), anyBoolean());
        verify(mTabGroupSyncService, times(1))
                .updateLocalTabId(eq(LOCAL_TAB_GROUP_ID_1), any(), eq(TAB_ID_1));
    }

    @Test
    public void testCloseTabGroup() {
        mTabModel.addTab(TAB_ID_1);
        mLocalMutationHelper.closeTabGroup(LOCAL_TAB_GROUP_ID_1, ClosingSource.CLOSED_BY_USER);
        verify(mTabModel).closeTabs(any());
        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(
                        eq(LOCAL_TAB_GROUP_ID_1), eq(ClosingSource.CLOSED_BY_USER));
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
