// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.util.Criteria.checkThat;
import static org.chromium.chrome.browser.tab_group_sync.TabGroupSyncIntegrationTestHelper.TAB_GROUP_SYNC_DATA_TYPE;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncIntegrationTestHelper.GroupInfo;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncIntegrationTestHelper.TabInfo;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.protocol.SavedTabGroup.SavedTabGroupColor;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.ui.base.DeviceFormFactor;

/** On-device sync integration tests for tab group sync from remote to local. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(b/40743432): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
    ChromeFeatureList.TAB_GROUP_PANE_ANDROID
})
@Restriction({DeviceFormFactor.PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
public class TabGroupSyncRemoteToLocalTest {
    private static final String TEST_URL1 = "/chrome/test/data/simple.html";
    private static final String TEST_URL2 = "/chrome/test/data/title2.html";
    private static final String TEST_URL3 = "/chrome/test/data/title3.html";
    private static final String TEST_URL4 = "/chrome/test/data/iframe.html";
    private static final String TAB_TITLE_1 = "OK";
    private static final String TAB_TITLE_2 = "Title Of Awesomeness";
    private static final String TAB_TITLE_3 = "Title Of More Awesomeness";
    private static final String TAB_TITLE_4 = "iframe test";

    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    // Create individual tabs
    private TabInfo mTab1;
    private TabInfo mTab2;
    private TabInfo mTab3;
    private TabInfo mTab4;
    GroupInfo mGroup1 =
            new GroupInfo("Science Group", SavedTabGroupColor.SAVED_TAB_GROUP_COLOR_CYAN);

    GroupInfo mGroup2 =
            new GroupInfo("Math Group", SavedTabGroupColor.SAVED_TAB_GROUP_COLOR_ORANGE);

    private TabGroupSyncIntegrationTestHelper mHelper;

    @Before
    public void setUp() {
        setUpUrlConstants();
        mHelper = new TabGroupSyncIntegrationTestHelper(mSyncTestRule);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForHistorySyncEnabled();
        mHelper.assertSyncEntityCount(0);

        CriteriaHelper.pollUiThread(
                mSyncTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        mGroup1.tabs.clear();
        mGroup2.tabs.clear();
    }

    private void setUpUrlConstants() {
        mTab1 = new TabInfo(TAB_TITLE_1, getUrl(TEST_URL1), 1);
        mTab2 = new TabInfo(TAB_TITLE_2, getUrl(TEST_URL2), 2);
        mTab3 = new TabInfo(TAB_TITLE_3, getUrl(TEST_URL3), 1);
        mTab4 = new TabInfo(TAB_TITLE_4, getUrl(TEST_URL4), 2);
    }

    private String getUrl(String url) {
        return mSyncTestRule.getTestServer().getURL(url);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testRemoteToLocalCreateNewTabGroup() {
        GroupInfo[] groups =
                TabGroupSyncIntegrationTestHelper.createGroupInfos(
                        new GroupInfo[] {mGroup1}, new TabInfo[][] {{mTab1}});
        mHelper.addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, mHelper.getTabInfoCount(groups));

        mHelper.verifyGroupInfosMatchesLocalData(new int[] {0}, groups);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testOneGroupTwoTabs() {
        GroupInfo[] groups =
                TabGroupSyncIntegrationTestHelper.createGroupInfos(
                        new GroupInfo[] {mGroup1}, new TabInfo[][] {{mTab1, mTab2}});
        mHelper.addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, mHelper.getTabInfoCount(groups));

        mHelper.verifyGroupInfosMatchesLocalData(new int[] {0}, groups);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testTwoGroups() {
        GroupInfo[] groups =
                TabGroupSyncIntegrationTestHelper.createGroupInfos(
                        new GroupInfo[] {mGroup1, mGroup2},
                        new TabInfo[][] {{mTab1, mTab2, mTab3}, {mTab4}});
        mHelper.addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, mHelper.getTabInfoCount(groups));
        mHelper.verifyGroupInfosMatchesLocalData(new int[] {0, 1}, groups);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testAddTab() {
        GroupInfo[] groups =
                TabGroupSyncIntegrationTestHelper.createGroupInfos(
                        new GroupInfo[] {mGroup1}, new TabInfo[][] {{mTab1}});
        mHelper.addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, mHelper.getTabInfoCount(groups));

        mHelper.verifyGroupInfosMatchesLocalData(new int[] {0}, groups);

        mHelper.addFakeServerTab(mGroup1.syncId, mTab2);
        waitForLocalTabGroupCountAndTabCount(groups.length, mHelper.getTabInfoCount(groups));
        mHelper.verifyGroupInfosMatchesLocalData(new int[] {0}, groups);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisabledTest(message = "b/337135045")
    public void testRemoveTab() {
        GroupInfo[] groups =
                TabGroupSyncIntegrationTestHelper.createGroupInfos(
                        new GroupInfo[] {mGroup1}, new TabInfo[][] {{mTab1, mTab2}});
        mHelper.addFakeServerGroups(groups);
        SyncTestUtil.triggerSyncAndWaitForCompletion();

        waitForLocalTabGroupCountAndTabCount(groups.length, mHelper.getTabInfoCount(groups));

        mHelper.verifyGroupInfosMatchesLocalData(new int[] {0}, groups);

        // Delete tab 1.
        SyncEntity syncEntity = mHelper.getSyncEntityWithUuid(mTab1.syncId);
        assertNotNull(syncEntity);
        mSyncTestRule
                .getFakeServerHelper()
                .deleteEntity(syncEntity.getIdString(), syncEntity.getClientTagHash());
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        mGroup1.tabs.remove(0);
        waitForLocalTabGroupCountAndTabCount(groups.length, mHelper.getTabInfoCount(groups));
        mHelper.verifyGroupInfosMatchesLocalData(new int[] {0}, groups);

        // Delete tab 2. It should delete the group itself locally.
        syncEntity = mHelper.getSyncEntityWithUuid(mTab2.syncId);
        assertNotNull(syncEntity);
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
                                                mSyncTestRule.getTargetContext(),
                                                TAB_GROUP_SYNC_DATA_TYPE)
                                        .size();
                        checkThat(
                                "Sync entity count does not match",
                                entityCount,
                                Matchers.equalTo(tabGroupCount + tabCount));
                        checkThat(
                                "Tab group count does not match",
                                mHelper.getTabGroupFilter().getTabGroupCount(),
                                Matchers.equalTo(tabGroupCount));
                        // Tab count is one extra since we started with an NTP.
                        checkThat(
                                "Tab model tab count does not match",
                                mHelper.getTabModel().getCount(),
                                Matchers.equalTo(1 + tabCount));
                    } catch (Exception ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                },
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
    }
}
