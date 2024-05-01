// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import androidx.test.filters.LargeTest;

import com.google.protobuf.InvalidProtocolBufferException;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SavedTabGroup;
import org.chromium.components.sync.protocol.SavedTabGroup.SavedTabGroupColor;
import org.chromium.components.sync.protocol.SavedTabGroupSpecifics;
import org.chromium.components.sync.protocol.SavedTabGroupTab;
import org.chromium.components.sync.protocol.SyncEntity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * On-device sync integration tests for tab group sync. Designed to test the following:
 *
 * <ul>
 *   <li>Sync updates to local, i.e. sync entities to tab model and tab groups.
 *   <li>Local updates to sync, i.e. tab model to sync entities
 * </ul>
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(b/40743432): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    ChromeFeatureList.ANDROID_TAB_GROUP_STABLE_IDS,
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID
})
public class TabGroupSyncIntegrationTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String SYNC_DATA_TYPE = "Saved Tab Group";

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

    // Create individual tabs
    TabInfo mTab1 = new TabInfo("Physics", "http://physics.com", 1);
    TabInfo mTab2 = new TabInfo("Chemistry", "http://chemistry.com", 2);
    TabInfo mTab3 = new TabInfo("Algebra", "http://algebra.com", 1);
    TabInfo mTab4 = new TabInfo("Calculus", "http://calculus.com", 2);
    GroupInfo mGroup1 =
            new GroupInfo("Science Group", SavedTabGroupColor.SAVED_TAB_GROUP_COLOR_CYAN);

    GroupInfo mGroup2 =
            new GroupInfo("Math Group", SavedTabGroupColor.SAVED_TAB_GROUP_COLOR_ORANGE);

    // We always pick the next GUID from this list.
    private static final List<String> GUIDS =
            Arrays.asList(
                    GUID_1, GUID_2, GUID_3, GUID_4, GUID_5, GUID_6, GUID_7, GUID_8, GUID_9,
                    GUID_10);
    private int mCurrentGuidIndex;

    @Before
    public void setUp() throws Exception {
        ChromeFeatureList.sAndroidTabGroupStableIds.setForTesting(true);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.setSelectedTypes(true, null);
        SyncTestUtil.waitForHistorySyncEnabled();
        assertSyncEntityCount(0);
    }

    @After
    public void tearDown() {
        mCurrentGuidIndex = 0;
        mGroup1.tabs.clear();
        mGroup2.tabs.clear();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testRemoteToLocalCreateNewTabGroup() throws Exception {
        GroupInfo[] groups = createGroupInfos(new GroupInfo[] {mGroup1}, new TabInfo[][] {{mTab1}});
        addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, getTabInfoCount(groups));

        verifyGroupInfos(0, groups);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testOneGroupTwoTabs() throws Exception {
        GroupInfo[] groups =
                createGroupInfos(new GroupInfo[] {mGroup1}, new TabInfo[][] {{mTab1, mTab2}});
        addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, getTabInfoCount(groups));

        verifyGroupInfos(0, groups);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testTwoGroups() throws Exception {
        GroupInfo[] groups =
                createGroupInfos(
                        new GroupInfo[] {mGroup1, mGroup2},
                        new TabInfo[][] {{mTab1, mTab2, mTab3}, {mTab4}});
        addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, getTabInfoCount(groups));
        verifyGroupInfos(0, groups);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testAddTab() throws Exception {
        GroupInfo[] groups = createGroupInfos(new GroupInfo[] {mGroup1}, new TabInfo[][] {{mTab1}});
        addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, getTabInfoCount(groups));

        verifyGroupInfos(0, groups);

        addFakeServerTab(mGroup1.syncId, mTab2);
        waitForLocalTabGroupCountAndTabCount(groups.length, getTabInfoCount(groups));
        verifyGroupInfos(0, groups);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisabledTest(message = "b/337135045")
    public void testRemoveTab() throws Exception {
        GroupInfo[] groups =
                createGroupInfos(new GroupInfo[] {mGroup1}, new TabInfo[][] {{mTab1, mTab2}});
        addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, getTabInfoCount(groups));

        verifyGroupInfos(0, groups);

        // Delete tab 1.
        SyncEntity syncEntity = getSyncEntityWithUuid(mTab1.syncId);
        Assert.assertNotNull(syncEntity);
        mSyncTestRule
                .getFakeServerHelper()
                .deleteEntity(syncEntity.getIdString(), syncEntity.getClientTagHash());
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        mGroup1.tabs.remove(0);
        waitForLocalTabGroupCountAndTabCount(groups.length, getTabInfoCount(groups));
        verifyGroupInfos(0, groups);

        // Delete tab 2. It should delete the group itself locally.
        syncEntity = getSyncEntityWithUuid(mTab2.syncId);
        Assert.assertNotNull(syncEntity);
        mSyncTestRule
                .getFakeServerHelper()
                .deleteEntity(syncEntity.getIdString(), syncEntity.getClientTagHash());
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        waitForLocalTabGroupCountAndTabCount(0, 0);
    }

    private void waitForLocalTabGroupCountAndTabCount(int tabGroupCount, int tabCount) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        int entityCount =
                                SyncTestUtil.getLocalData(
                                                mSyncTestRule.getTargetContext(), SYNC_DATA_TYPE)
                                        .size();
                        Criteria.checkThat(
                                "Sync entity count does not match",
                                entityCount,
                                Matchers.equalTo(tabGroupCount + tabCount));
                        Criteria.checkThat(
                                "Tab group count does not match",
                                getTabGroupFilter().getTabGroupCount(),
                                Matchers.equalTo(tabGroupCount));
                        // Tab count is one extra since we started with an NTP.
                        Criteria.checkThat(
                                "Tab model tab count does not match",
                                getTabModel().getCount(),
                                Matchers.equalTo(1 + tabCount));
                    } catch (Exception ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                },
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
    }

    private void assertSyncEntityCount(int count) throws JSONException {
        Assert.assertEquals(
                "There should be " + count + " saved tab groups.",
                count,
                SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), SYNC_DATA_TYPE).size());
    }

    private SyncEntity getSyncEntityWithUuid(String guid) {
        List<SyncEntity> entities = getSyncEntities();
        for (SyncEntity entity : entities) {
            if (entity.getSpecifics().getSavedTabGroup().getGuid().equals(guid)) {
                return entity;
            }
        }
        return null;
    }

    private List<SyncEntity> getSyncEntities() {
        try {
            List<SyncEntity> entities =
                    mSyncTestRule
                            .getFakeServerHelper()
                            .getSyncEntitiesByModelType(ModelType.SAVED_TAB_GROUP);
            return entities;
        } catch (InvalidProtocolBufferException ex) {
            Assert.fail(ex.toString());
            return new ArrayList<>();
        }
    }

    private int getTabGroupAt(int index) {
        List<Integer> rootIds = getTabGroupRootIds();
        Assert.assertTrue(index < rootIds.size());
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

    private void addFakeServerGroups(GroupInfo[] groupInfos) {
        for (GroupInfo groupInfo : groupInfos) {
            addFakeServerGroup(groupInfo);
        }
    }

    private String addFakeServerGroup(GroupInfo groupInfo) {
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

    private String addFakeServerTab(String groupGuid, TabInfo tabInfo) {
        EntitySpecifics tab = makeTabEntity(groupGuid, tabInfo);
        String guid = tab.getSavedTabGroup().getGuid();
        mSyncTestRule.getFakeServerHelper().injectUniqueClientEntity(guid, guid, tab);
        return guid;
    }

    private EntitySpecifics makeGroupEntity(GroupInfo groupInfo) {
        SavedTabGroup group =
                SavedTabGroup.newBuilder()
                        .setTitle(groupInfo.title)
                        .setColor(groupInfo.color)
                        .build();

        String guid = getNextGuid();
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

    private EntitySpecifics makeTabEntity(String groupGuid, TabInfo tabInfo) {
        SavedTabGroupTab tab =
                SavedTabGroupTab.newBuilder()
                        .setGroupGuid(groupGuid)
                        .setUrl(tabInfo.url)
                        .setTitle(tabInfo.title)
                        .setPosition(tabInfo.position)
                        .build();

        String guid = getNextGuid();
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

    /** Convenient class for setting expectation about a group. Modify as you like. */
    private static class GroupInfo {
        public String title;
        public SavedTabGroupColor color;
        public List<TabInfo> tabs = new ArrayList<>();

        // Required for connecting with tabs. We don't use it for validation.
        public String syncId;

        public GroupInfo(String title, SavedTabGroupColor color) {
            this.title = title;
            this.color = color;
        }

        public void addTab(TabInfo tabInfo) {
            tabs.add(tabInfo);
        }
    }

    private static class TabInfo {
        public String title;
        public String url;
        public int position;

        // Required for connecting with tabs. We don't use it for validation.
        public String syncId;

        public TabInfo(String title, String url, int position) {
            this.title = title;
            this.url = url;
            this.position = position;
        }
    }

    private int getTabInfoCount(GroupInfo[] groupInfos) {
        int count = 0;
        for (GroupInfo groupInfo : groupInfos) {
            count += groupInfo.tabs.size();
        }
        return count;
    }

    private void verifyGroupInfos(int index, GroupInfo[] expectedGroups) {
        for (int i = 0; i < expectedGroups.length; i++) {
            verifyGroupInfo(index + i, expectedGroups[i]);
        }
    }

    private void verifyGroupInfo(int index, GroupInfo expectedGroup) {
        TabGroupModelFilter filter = getTabGroupFilter();
        int groupId = getTabGroupAt(index);
        String actualTitle = filter.getTabGroupTitle(groupId);
        int actualColor = filter.getTabGroupColor(groupId);
        List<Tab> tabs = filter.getRelatedTabList(groupId);

        // Assert group details
        Assert.assertEquals(
                "Group title does not match at index " + index, expectedGroup.title, actualTitle);
        // The actual color starts at index 0 while the proto definition starts at 1.
        Assert.assertEquals(
                "Group color does not match at index " + index,
                expectedGroup.color.getNumber(),
                actualColor + 1);
        Assert.assertEquals(
                "Number of tabs does not match in group at index " + index,
                expectedGroup.tabs.size(),
                tabs.size());

        // Verify each tab in the group
        for (int i = 0; i < tabs.size(); i++) {
            verifyTabInfo(tabs.get(i), expectedGroup.tabs.get(i));
        }
    }

    public void verifyTabInfo(Tab actualTab, TabInfo expectedTab) {
        // TODO(shaktisahu): Tab title will be verified after the lazy loading is fixed.
        // Assert.assertEquals("Tab title does not match", expectedTab.title, actualTab.getTitle());
        // TODO(shaktisahu): Can we turn off network and get the correct URLs? i.e. don't make
        // https://somthing to https://www.something.com
        // Assert.assertEquals(
        //         "Tab URL does not match", new GURL(expectedTab.url), actualTab.getOriginalUrl());
        // TODO(shaktisahu): Verify the tab position as well.
    }

    private GroupInfo[] createGroupInfos(GroupInfo[] groups, TabInfo[][] tabs) {
        Assert.assertEquals(groups.length, tabs.length);
        for (int i = 0; i < groups.length; i++) {
            for (int j = 0; j < tabs[i].length; j++) {
                groups[i].addTab(tabs[i][j]);
            }
        }
        return groups;
    }

    public String getNextGuid() {
        Assert.assertTrue(
                "Exceeded pre-allocated GUIDs, please fix test", mCurrentGuidIndex < GUIDS.size());
        return GUIDS.get(mCurrentGuidIndex++);
    }

    private long getCurrentTimeInMicros() {
        return System.currentTimeMillis() * 1000;
    }

    private TabModel getTabModel() {
        return mSyncTestRule.getActivity().getTabModelSelector().getModel(false);
    }

    private TabGroupModelFilter getTabGroupFilter() {
        return (TabGroupModelFilter)
                mSyncTestRule
                        .getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getTabModelFilter(false);
    }
}
