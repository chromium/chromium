// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link DataSharingTabGroupUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DataSharingTabGroupUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String SYNC_SUFFIX = "_sync";
    private static final String COLLABORATION_SUFFIX = "_collaboration";

    private static final int TAB_ID_1 = 123;
    private static final int TAB_ID_2 = 456;
    private static final int TAB_ID_3 = 789;
    private static final int TAB_ID_4 = 433289;
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 =
            new LocalTabGroupId(new Token(1L, 1L));
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_2 =
            new LocalTabGroupId(new Token(2L, 2L));
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_3 =
            new LocalTabGroupId(new Token(3L, 3L));

    @Mock private Profile mRegularProfile;
    @Mock private Profile mOtrProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabCreator mTabCreator;

    @Before
    public void setUp() {
        when(mRegularProfile.isOffTheRecord()).thenReturn(false);
        when(mOtrProfile.isOffTheRecord()).thenReturn(true);
        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mTabCreator);

        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
    }

    @Test
    public void testGetCollaborationsDestroyedByTabRemoval_NullList() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabRemoval(
                        tabModel, /* tabsToRemove= */ null);
        assertTrue(result.isEmpty());
    }

    @Test
    public void testGetCollaborationsDestroyedByTabRemoval_EmptyList() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabRemoval(
                        tabModel, /* tabsToRemove= */ Collections.emptyList());
        assertTrue(result.isEmpty());
    }

    @Test
    public void testGetCollaborationsDestroyedByTabRemoval_IncognitoTabModel() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ true);

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabRemoval(
                        tabModel, List.of(tabModel.getTabById(TAB_ID_1)));
        assertTrue(result.isEmpty());
    }

    @Test
    public void testGetCollaborationsDestroyedByTabRemoval_NoLocalGroup() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        /* localTabGroupId= */ null,
                        List.of(TAB_ID_1),
                        /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabRemoval(
                        tabModel, List.of(tabModel.getTabById(TAB_ID_1)));
        assertTrue(result.isEmpty());
    }

    @Test
    public void testGetCollaborationsDestroyedByTabRemoval_NoCollaboration() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ false));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabRemoval(
                        tabModel, List.of(tabModel.getTabById(TAB_ID_1)));
        assertTrue(result.isEmpty());
    }

    @Test
    public void testGetCollaborationsDestroyedByTabRemoval_NotAllClosing() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1,
                        List.of(TAB_ID_1, TAB_ID_2),
                        /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabRemoval(
                        tabModel, List.of(tabModel.getTabById(TAB_ID_1)));
        assertTrue(result.isEmpty());
    }

    @Test
    public void testGetCollaborationsDestroyedByTabRemoval_AllClosing_1Tab() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabRemoval(
                        tabModel, List.of(tabModel.getTabById(TAB_ID_1)));
        assertEquals(LOCAL_TAB_GROUP_ID_1, result.get(0));
    }

    @Test
    public void testGetCollaborationsDestroyedByTabRemoval_AllClosing_2Tab() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1,
                        List.of(TAB_ID_1, TAB_ID_2),
                        /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabRemoval(
                        tabModel,
                        List.of(tabModel.getTabById(TAB_ID_1), tabModel.getTabById(TAB_ID_2)));
        assertEquals(LOCAL_TAB_GROUP_ID_1, result.get(0));
    }

    @Test
    public void testGetCollaborationsDestroyedByTabClosure_NoTabs() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);
        var params = TabClosureParams.closeTabs(Collections.emptyList()).build();

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabClosure(tabModel, params);

        assertTrue(result.isEmpty());
    }

    @Test
    public void testGetCollaborationsDestroyedByTabClosure_SomeTabs_NotHiding() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);
        var params = TabClosureParams.closeTabs(List.of(tabModel.getTabById(TAB_ID_1))).build();

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabClosure(tabModel, params);

        assertEquals(LOCAL_TAB_GROUP_ID_1, result.get(0));
    }

    @Test
    public void testGetCollaborationsDestroyedByTabClosure_SomeTabs_Hiding() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);
        var params =
                TabClosureParams.closeTabs(List.of(tabModel.getTabById(TAB_ID_1)))
                        .hideTabGroups(true)
                        .build();

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabClosure(tabModel, params);

        assertTrue(result.isEmpty());
    }

    @Test
    public void testGetCollaborationsDestroyedByTabClosure_AllTabs() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1,
                        List.of(TAB_ID_1, TAB_ID_2),
                        /* isCollaboration= */ true));
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_2, List.of(TAB_ID_3), /* isCollaboration= */ false));
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_3, List.of(TAB_ID_4), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);
        var params = TabClosureParams.closeAllTabs().build();

        List<LocalTabGroupId> result =
                DataSharingTabGroupUtils.getCollaborationsDestroyedByTabClosure(tabModel, params);

        assertEquals(LOCAL_TAB_GROUP_ID_1, result.get(0));
        assertEquals(LOCAL_TAB_GROUP_ID_3, result.get(1));
    }

    @Test
    public void testCreatePlaceholderTabInGroups_Incognito() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ true);
        List<Tab> result =
                DataSharingTabGroupUtils.createPlaceholderTabInGroups(
                        tabModel, mTabCreatorManager, List.of(LOCAL_TAB_GROUP_ID_1));
        assertTrue(result.isEmpty());
    }

    @Test
    public void testCreatePlaceholderTabInGroups_EmptyOrNullOrMismatch() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1, List.of(TAB_ID_1), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);
        List<Tab> result =
                DataSharingTabGroupUtils.createPlaceholderTabInGroups(
                        tabModel, mTabCreatorManager, /* localTabGroupIds= */ null);
        assertTrue(result.isEmpty());

        result =
                DataSharingTabGroupUtils.createPlaceholderTabInGroups(
                        tabModel, mTabCreatorManager, Collections.emptyList());
        assertTrue(result.isEmpty());

        result =
                DataSharingTabGroupUtils.createPlaceholderTabInGroups(
                        tabModel, mTabCreatorManager, List.of(LOCAL_TAB_GROUP_ID_2));
        assertTrue(result.isEmpty());
    }

    @Test
    public void testCreatePlaceholderTabInGroups() {
        List<TabGroupData> tabGroups = new ArrayList<>();
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_1,
                        List.of(TAB_ID_1, TAB_ID_2),
                        /* isCollaboration= */ true));
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_2, List.of(TAB_ID_3), /* isCollaboration= */ true));
        tabGroups.add(
                new TabGroupData(
                        LOCAL_TAB_GROUP_ID_3, List.of(TAB_ID_4), /* isCollaboration= */ true));
        var tabModel = createTabGroups(tabGroups, /* isIncognito= */ false);
        List<Tab> result =
                DataSharingTabGroupUtils.createPlaceholderTabInGroups(
                        tabModel,
                        mTabCreatorManager,
                        List.of(LOCAL_TAB_GROUP_ID_1, LOCAL_TAB_GROUP_ID_3));
        assertEquals(2, result.size());

        verify(mTabCreator)
                .createNewTab(
                        any(),
                        eq(TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP),
                        eq(tabModel.getTabById(TAB_ID_2)));
        verify(mTabCreator)
                .createNewTab(
                        any(),
                        eq(TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP),
                        eq(tabModel.getTabById(TAB_ID_4)));
    }

    private static class TabGroupData {
        public final @Nullable LocalTabGroupId localTabGroupId;
        public final List<Integer> tabIds;
        public final boolean isCollaboration;

        TabGroupData(
                @Nullable LocalTabGroupId localTabGroupId,
                List<Integer> tabIds,
                boolean isCollaboration) {
            this.localTabGroupId = localTabGroupId;
            this.tabIds = tabIds;
            this.isCollaboration = isCollaboration;
        }
    }

    private TabModel createTabGroups(List<TabGroupData> groups, boolean isIncognito) {
        MockTabModel mockTabModel =
                new MockTabModel(isIncognito ? mOtrProfile : mRegularProfile, /* delegate= */ null);

        List<SavedTabGroup> savedGroups = new ArrayList<>();
        List<String> savedGroupSyncIds = new ArrayList<>();
        for (TabGroupData group : groups) {
            List<SavedTabGroupTab> savedTabs = new ArrayList<>();
            for (int tabId : group.tabIds) {
                Tab tab = mockTabModel.addTab(tabId);
                if (group.localTabGroupId != null) {
                    tab.setTabGroupId(group.localTabGroupId.tabGroupId);
                }

                SavedTabGroupTab savedTab = new SavedTabGroupTab();
                savedTab.localId = tabId;
                savedTabs.add(savedTab);
            }

            SavedTabGroup savedGroup = new SavedTabGroup();
            // Use hashcode as unique placeholder if non-local group.
            String groupIdString =
                    group.localTabGroupId != null
                            ? group.localTabGroupId.tabGroupId.toString()
                            : String.valueOf(group.hashCode());
            savedGroup.syncId = groupIdString + SYNC_SUFFIX;
            savedGroup.localId = group.localTabGroupId;
            savedGroup.savedTabs = savedTabs;
            if (group.isCollaboration) {
                savedGroup.collaborationId = groupIdString + COLLABORATION_SUFFIX;
            }

            savedGroupSyncIds.add(savedGroup.syncId);
            savedGroups.add(savedGroup);
        }

        if (!isIncognito) {
            when(mTabGroupSyncService.getAllGroupIds())
                    .thenReturn(savedGroupSyncIds.toArray(new String[0]));
            for (int i = 0; i < savedGroupSyncIds.size(); i++) {
                when(mTabGroupSyncService.getGroup(savedGroupSyncIds.get(i)))
                        .thenReturn(savedGroups.get(i));
            }
        } else {
            when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        }

        return mockTabModel;
    }
}
