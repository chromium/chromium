// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.chrome.browser.tab_group_sync.TabGroupSyncUtils.NEW_TAB_TITLE;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SavedTabGroup;
import org.chromium.components.sync.protocol.SavedTabGroup.SavedTabGroupColor;
import org.chromium.components.sync.protocol.SavedTabGroupSpecifics;
import org.chromium.components.sync.protocol.SavedTabGroupTab;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Helper class for integration tests. */
public class TabGroupSyncIntegrationTestHelper {
    public static final String TAB_GROUP_SYNC_DATA_TYPE = "Saved Tab Group";

    // DO NOT Change these IDs. These must be valid UUIDs to parse as non-empty.
    private static final String GUID_1 = "6f5a2a8f-e1a7-4bca-b0c0-f0f22be21f6d";
    private static final String GUID_2 = "c6e1e2fd-46a6-4f11-8da1-8424c942d210";
    private static final String GUID_3 = "24ed7c34-41a3-47c2-aad4-5ea42a1765d5";
    private static final String GUID_4 = "d8d68781-0465-4b8a-a68f-78d07d474b34";
    private static final String GUID_5 = "e80b5016-b697-4f59-81b0-b4e15e4f3937";
    private static final String GUID_6 = "4c7a6b2a-4139-4e9e-8257-1ee8d1387b90";
    private static final String GUID_7 = "1b687a61-8a17-4f98-bf9d-74d2b50abf3e";
    private static final String GUID_8 = "cf07d904-88d4-4bc9-989d-57a9ab9e17a7";
    private static final String GUID_9 = "8bcbca67-c1b7-40c7-b421-eb7e2db99a9b";
    private static final String GUID_10 = "b453ae62-3568-4d7b-8d18-0be58f43b337";

    // We always pick the next GUID from this list.
    private static final List<String> sGuids =
            List.of(
                    GUID_1, GUID_2, GUID_3, GUID_4, GUID_5, GUID_6, GUID_7, GUID_8, GUID_9,
                    GUID_10);

    private Iterator<String> mGuidIterator;
    private SyncTestRule mSyncTestRule;

    /**
     * Helper for handling or asserting on changes to the fake sync server.
     *
     * @param syncTestRule The {@link SyncTestRule} for the test harness using this helper.
     */
    public TabGroupSyncIntegrationTestHelper(SyncTestRule syncTestRule) {
        mSyncTestRule = syncTestRule;
        resetGuidIterator();
    }

    /** Convenient class for setting expectation about a tab. */
    public static class TabInfo {
        public String title;
        public String url;
        public long position;

        // Required for connecting with tabs. We don't use it for validation.
        public String syncId;

        /**
         * @param title The title of the tab.
         * @param url The url of the tab.
         * @param position The position of the tab in the tab group.
         */
        public TabInfo(String title, String url, long position) {
            this.title = title;
            this.url = url;
            this.position = position;
        }
    }

    /** Convenient class for setting expectation about a group. */
    public static class GroupInfo {
        public String title;
        public SavedTabGroupColor color;
        public List<TabInfo> tabs = new ArrayList<>();

        // Required for connecting with tabs. We don't use it for validation.
        public String syncId;

        /**
         * @param title The title of the tab group.
         * @param color The color of the tab group.
         */
        public GroupInfo(String title, SavedTabGroupColor color) {
            this.title = title;
            this.color = color;
        }

        /** Adds a tab to this tab group. */
        public void addTab(TabInfo tabInfo) {
            tabs.add(tabInfo);
        }
    }

    /**
     * For each group adds the corresponding list of tabs.
     *
     * @param groups The {@link GroupInfo}s to add tabs to.
     * @param tabs The array of arrays of {@link TabInfo}s to add to each group.
     */
    public static GroupInfo[] createGroupInfos(GroupInfo[] groups, TabInfo[][] tabs) {
        assertEquals(groups.length, tabs.length);
        for (int i = 0; i < groups.length; i++) {
            for (int j = 0; j < tabs[i].length; j++) {
                groups[i].addTab(tabs[i][j]);
            }
        }
        return groups;
    }

    /** Resets the GUID iterator for creating groups. */
    public void resetGuidIterator() {
        mGuidIterator = sGuids.iterator();
    }

    /**
     * Creates a fake server tab.
     *
     * @param groupGuid The GUID of the group to add the tab to.
     * @param tabInfo The tab info to add to the group.
     * @return the GUID of the tab.
     */
    public String addFakeServerTab(String groupGuid, TabInfo tabInfo) {
        EntitySpecifics tab = makeTabEntity(groupGuid, tabInfo);
        String guid = tab.getSavedTabGroup().getGuid();
        mSyncTestRule.getFakeServerHelper().injectUniqueClientEntity(guid, guid, tab);
        return guid;
    }

    /**
     * Creates a fake server group.
     *
     * @param groupInfo The data for the group to create.
     * @return the GUID of the group.
     */
    public String addFakeServerGroup(GroupInfo groupInfo) {
        EntitySpecifics group = makeGroupEntity(groupInfo);
        String guid = group.getSavedTabGroup().getGuid();
        mSyncTestRule.getFakeServerHelper().injectUniqueClientEntity(guid, guid, group);
        for (int i = 0; i < groupInfo.tabs.size(); i++) {
            TabInfo tabInfo = groupInfo.tabs.get(i);
            tabInfo.position = i;
            addFakeServerTab(guid, tabInfo);
        }
        return guid;
    }

    /**
     * Creates multiple fake server groups.
     *
     * @param groupInfos The list of tab groups to create.
     */
    public void addFakeServerGroups(GroupInfo[] groupInfos) {
        for (GroupInfo groupInfo : groupInfos) {
            addFakeServerGroup(groupInfo);
        }
    }

    /**
     * Constructs a representation of a list of tab groups from sync entities.
     *
     * @param syncEntities The {@link SyncEntity} objects representing a tab group.
     * @return a list of {@link GroupInfo} representing the synced groups.
     */
    public List<GroupInfo> constructGroupInfoFromSyncEntities(List<SyncEntity> syncEntities) {
        Map<String, GroupInfo> groupInfos = new HashMap();

        // Group specifics.
        for (SyncEntity entity : syncEntities) {
            SavedTabGroupSpecifics specifics = entity.getSpecifics().getSavedTabGroup();
            if (specifics.hasGroup()) {
                GroupInfo groupInfo =
                        new GroupInfo(
                                specifics.getGroup().getTitle(), specifics.getGroup().getColor());
                groupInfos.put(specifics.getGuid(), groupInfo);
            }
        }

        // Tab specifics.
        for (SyncEntity entity : syncEntities) {
            SavedTabGroupSpecifics specifics = entity.getSpecifics().getSavedTabGroup();
            if (specifics.hasGroup()) continue;

            String groupGuid = specifics.getTab().getGroupGuid();
            TabInfo tabInfo =
                    new TabInfo(
                            specifics.getTab().getTitle(),
                            specifics.getTab().getUrl(),
                            specifics.getTab().getPosition());
            groupInfos.get(groupGuid).addTab(tabInfo);
        }

        return new ArrayList<>(groupInfos.values());
    }

    private EntitySpecifics makeTabEntity(String groupGuid, TabInfo tabInfo) {
        SavedTabGroupTab tab =
                SavedTabGroupTab.newBuilder()
                        .setGroupGuid(groupGuid)
                        .setUrl(tabInfo.url)
                        .setTitle(tabInfo.title)
                        .setPosition(tabInfo.position)
                        .build();

        String guid = mGuidIterator.next();
        tabInfo.syncId = guid;
        SavedTabGroupSpecifics specificsTab =
                SavedTabGroupSpecifics.newBuilder()
                        .setGuid(guid)
                        .setCreationTimeWindowsEpochMicros(getCurrentTimeInMicros())
                        .setUpdateTimeWindowsEpochMicros(getCurrentTimeInMicros())
                        .setTab(tab)
                        .build();

        return EntitySpecifics.newBuilder().setSavedTabGroup(specificsTab).build();
    }

    private EntitySpecifics makeGroupEntity(GroupInfo groupInfo) {
        SavedTabGroup group =
                SavedTabGroup.newBuilder()
                        .setTitle(groupInfo.title)
                        .setColor(groupInfo.color)
                        .build();

        String guid = mGuidIterator.next();
        groupInfo.syncId = guid;
        SavedTabGroupSpecifics specificsGroup =
                SavedTabGroupSpecifics.newBuilder()
                        .setGuid(guid)
                        .setCreationTimeWindowsEpochMicros(getCurrentTimeInMicros())
                        .setUpdateTimeWindowsEpochMicros(getCurrentTimeInMicros())
                        .setGroup(group)
                        .build();

        return EntitySpecifics.newBuilder().setSavedTabGroup(specificsGroup).build();
    }

    /** Returns the total number of tabs in the supplied groups. */
    public int getTabInfoCount(GroupInfo[] groupInfos) {
        int count = 0;
        for (GroupInfo groupInfo : groupInfos) {
            count += groupInfo.tabs.size();
        }
        return count;
    }

    /** Returns the regular tab model. */
    public TabModel getTabModel() {
        return mSyncTestRule.getActivity().getTabModelSelector().getModel(false);
    }

    /** Returns the regular tab model filter. */
    public TabGroupModelFilter getTabGroupFilter() {
        return (TabGroupModelFilter)
                mSyncTestRule
                        .getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getTabModelFilter(false);
    }

    /** Gets the {@link SyncEntity} for a particular sync GUID. */
    public SyncEntity getSyncEntityWithUuid(String guid) {
        List<SyncEntity> entities = getSyncEntities();
        for (SyncEntity entity : entities) {
            if (entity.getSpecifics().getSavedTabGroup().getGuid().equals(guid)) {
                return entity;
            }
        }
        return null;
    }

    /** Returns all the synced tab group related entities. */
    public List<SyncEntity> getSyncEntities() {
        try {
            List<SyncEntity> entities =
                    mSyncTestRule
                            .getFakeServerHelper()
                            .getSyncEntitiesByDataType(DataType.SAVED_TAB_GROUP);
            return entities;
        } catch (InvalidProtocolBufferException ex) {
            fail(ex.toString());
            return new ArrayList<>();
        }
    }

    /** Asserts that the number of sync tab group entities is the same as {@code count}. */
    public void assertSyncEntityCount(int count) {
        int entityCount = 0;
        try {
            entityCount =
                    SyncTestUtil.getLocalData(
                                    mSyncTestRule.getTargetContext(), TAB_GROUP_SYNC_DATA_TYPE)
                            .size();
        } catch (Exception e) {
            fail("Getting local data for TAB_GROUP_SYNC_DATA_TYPE failed " + e);
        }
        assertEquals("There should be " + count + " saved tab groups.", count, entityCount);
    }

    /** Verifies that the entities in sync match the provided tab groups. */
    public void verifySyncEntities(GroupInfo[] expectedGroups) {
        List<GroupInfo> retrievedGroups = constructGroupInfoFromSyncEntities(getSyncEntities());
        assertEquals(expectedGroups.length, retrievedGroups.size());

        for (int i = 0; i < expectedGroups.length; i++) {
            GroupInfo expectedGroup = expectedGroups[i];
            GroupInfo actualGroup = retrievedGroups.get(i);
            assertEquals(expectedGroup.title, actualGroup.title);
            assertEquals(expectedGroup.color, actualGroup.color);
            assertEquals(expectedGroup.tabs.size(), actualGroup.tabs.size());
            for (int j = 0; j < expectedGroup.tabs.size(); j++) {
                verifyTitleAndUrlForTab(expectedGroup.tabs.get(j), actualGroup.tabs.get(j));
            }
        }
    }

    private static void verifyTitleAndUrlForTab(TabInfo expectedTab, TabInfo actualTab) {
        boolean isNtpUrl = TabGroupSyncUtils.isNtpOrAboutBlankUrl(new GURL(expectedTab.url));
        if (isNtpUrl) {
            assertTrue(
                    "URL is not NTP",
                    TabGroupSyncUtils.isNtpOrAboutBlankUrl(new GURL(actualTab.url)));
            assertTrue(
                    "Title is not new tab",
                    NEW_TAB_TITLE.equals(actualTab.title) || "about:blank".equals(actualTab.title));
        } else {
            assertEquals(expectedTab.url, actualTab.url);
            assertEquals(expectedTab.title, actualTab.title);
        }
    }

    /**
     * Verifies the synced tab group data matches the local data.
     *
     * @param indices The index for each tab group.
     * @param expectedGroups The list of expected groups.
     */
    public void verifyGroupInfosMatchesLocalData(int[] indices, GroupInfo[] expectedGroups) {
        assertEquals(indices.length, expectedGroups.length);
        for (int i = 0; i < expectedGroups.length; i++) {
            verifyGroupInfoMatchesLocalData(indices[i], expectedGroups[i]);
        }
    }

    /**
     * Verifies the synced tab group matches the local data.
     *
     * @param index The index of the tab group.
     * @param expectedGroup The expected tab group.
     */
    public void verifyGroupInfoMatchesLocalData(int index, GroupInfo expectedGroup) {
        TabGroupModelFilter filter = getTabGroupFilter();
        int rootId = getTabGroupRootIdAt(index);
        String actualTitle = filter.getTabGroupTitle(rootId);
        int actualColor = filter.getTabGroupColorWithFallback(rootId);
        List<Tab> tabs = filter.getRelatedTabList(rootId);

        // group details
        assertEquals(
                "Group title does not match at index " + index, expectedGroup.title, actualTitle);
        // The actual color starts at index 0 while the proto definition starts at 1.
        assertEquals(
                "Group color does not match at index " + index,
                expectedGroup.color.getNumber(),
                actualColor + 1);
        assertEquals(
                "Number of tabs does not match in group at index " + index,
                expectedGroup.tabs.size(),
                tabs.size());

        // Verify each tab in the group
        for (int i = 0; i < tabs.size(); i++) {
            verifyTabInfo(tabs.get(i), expectedGroup.tabs.get(i));
        }
    }

    private int getTabGroupRootIdAt(int index) {
        List<Integer> rootIds = getTabGroupRootIds();
        assertTrue(index < rootIds.size());
        return rootIds.get(index);
    }

    private List<Integer> getTabGroupRootIds() {
        Set<Integer> rootIds = new HashSet<>();
        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            if (tab.getTabGroupId() == null) continue;
            rootIds.add(tab.getRootId());
        }
        return new ArrayList<>(rootIds);
    }

    /**
     * Verifies the tab and the synced tab match.
     *
     * @param actualTab The tab in the local model.
     * @param expectedTab The synced tab.
     */
    public void verifyTabInfo(Tab actualTab, TabInfo expectedTab) {
        assertEquals("Tab title does not match", expectedTab.title, actualTab.getTitle());
        assertEquals(
                "Tab URL does not match", new GURL(expectedTab.url), actualTab.getOriginalUrl());
    }

    private static long getCurrentTimeInMicros() {
        return System.currentTimeMillis() * 1000;
    }
}
