// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SavedTabGroup;
import org.chromium.components.sync.protocol.SavedTabGroup.SavedTabGroupColor;
import org.chromium.components.sync.protocol.SavedTabGroupSpecifics;
import org.chromium.components.sync.protocol.SavedTabGroupTab;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
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
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
    ChromeFeatureList.TAB_GROUP_PANE_ANDROID
})
@Restriction({
    UiRestriction.RESTRICTION_TYPE_PHONE,
    Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE
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

    private static final String TEST_URL1 = "/chrome/test/data/simple.html";
    private static final String TEST_URL2 = "/chrome/test/data/title2.html";
    private static final String TEST_URL3 = "/chrome/test/data/title3.html";
    private static final String TEST_URL4 = "/chrome/test/data/iframe.html";
    private static final String TAB_TITLE_1 = "OK";
    private static final String TAB_TITLE_2 = "Title Of Awesomeness";
    private static final String TAB_TITLE_3 = "Title Of More Awesomeness";
    private static final String TAB_TITLE_4 = "iframe test";

    private static final String NEW_TAB_TITLE = "New tab";
    public static final String NEW_TAB_URL = UrlConstants.NTP_NON_NATIVE_URL;

    // Create individual tabs
    private TabInfo mTab1;
    private TabInfo mTab2;
    private TabInfo mTab3;
    private TabInfo mTab4;
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
    private ModalDialogManager mModalDialogManager;

    @Before
    public void setUp() throws Exception {
        setUpUrlConstants();
        ChromeFeatureList.sAndroidTabGroupStableIds.setForTesting(true);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.setSelectedTypes(true, null);
        SyncTestUtil.waitForHistorySyncEnabled();
        assertSyncEntityCount(0);

        mSyncTestRule.getActivity().getTabContentManager().setCaptureMinRequestTimeForTesting(0);

        CriteriaHelper.pollUiThread(
                mSyncTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        mModalDialogManager =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        mSyncTestRule.getActivity()::getModalDialogManager);
    }

    @After
    public void tearDown() {
        mCurrentGuidIndex = 0;
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

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testLocal_CreateTabGroup() throws Exception {
        final ChromeTabbedActivity cta = mSyncTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(org.chromium.components.tab_groups.TabGroupColorId.GREY);

        // Verify sync.
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        List<SyncEntity> entities = getSyncEntities();
        Assert.assertEquals("Number of sync entities don't match", 3, entities.size());
        assertSyncEntityCount(3);

        GroupInfo group1 = new GroupInfo("", SavedTabGroupColor.SAVED_TAB_GROUP_COLOR_GREY);
        TabInfo tab1 = new TabInfo(NEW_TAB_TITLE, NEW_TAB_URL, 1);
        TabInfo tab2 = new TabInfo(NEW_TAB_TITLE, NEW_TAB_URL, 2);
        GroupInfo[] expectedGroups =
                createGroupInfos(new GroupInfo[] {group1}, new TabInfo[][] {{tab1, tab2}});
        verifySyncEntities(expectedGroups);
    }

    private void verifySyncEntities(GroupInfo[] expectedGroups) {
        List<GroupInfo> retrievedGroups = constructGroupInfoFromSyncEntities(getSyncEntities());
        Assert.assertEquals(expectedGroups.length, retrievedGroups.size());

        for (int i = 0; i < expectedGroups.length; i++) {
            GroupInfo expectedGroup = expectedGroups[i];
            GroupInfo actualGroup = retrievedGroups.get(i);
            Assert.assertEquals(expectedGroup.title, actualGroup.title);
            Assert.assertEquals(expectedGroup.color, actualGroup.color);
            Assert.assertEquals(expectedGroup.tabs.size(), actualGroup.tabs.size());
            for (int j = 0; j < expectedGroup.tabs.size(); j++) {
                verifyTitleAndUrlForTab(expectedGroup.tabs.get(j), actualGroup.tabs.get(j));
            }
        }
    }

    private void verifyTitleAndUrlForTab(TabInfo expectedTab, TabInfo actualTab) {
        boolean isNtpUrl = TabGroupSyncUtils.isNtpOrAboutBlankUrl(expectedTab.url);
        if (isNtpUrl) {
            Assert.assertTrue(TabGroupSyncUtils.isNtpOrAboutBlankUrl(actualTab.url));
            Assert.assertTrue(
                    NEW_TAB_TITLE.equals(actualTab.title) || "about:blank".equals(actualTab.title));
        } else {
            Assert.assertEquals(expectedTab.url, actualTab.url);
            Assert.assertEquals(expectedTab.title, actualTab.title);
        }
    }

    private List<GroupInfo> constructGroupInfoFromSyncEntities(List<SyncEntity> syncEntities) {
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

    private void verifyFirstCardTitle(String title) {
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(
                                                TabUiTestHelper.getTabSwitcherAncestorId(
                                                        mSyncTestRule.getActivity()))),
                                withId(org.chromium.chrome.test.R.id.tab_list_recycler_view)))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            RecyclerView recyclerView = (RecyclerView) v;
                            TextView firstCardTitleTextView =
                                    recyclerView
                                            .findViewHolderForAdapterPosition(0)
                                            .itemView
                                            .findViewById(org.chromium.chrome.test.R.id.tab_title);
                            assertEquals(title, firstCardTitleTextView.getText().toString());
                        });
    }

    private void verifyFirstCardColor(
            @org.chromium.components.tab_groups.TabGroupColorId int color) {
        onView(
                        allOf(
                                withId(org.chromium.chrome.test.R.id.tab_favicon),
                                withParent(withId(org.chromium.chrome.test.R.id.card_view))))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            ImageView imageView = (ImageView) v;
                            LayerDrawable layerDrawable = (LayerDrawable) imageView.getDrawable();
                            GradientDrawable drawable =
                                    (GradientDrawable) layerDrawable.getDrawable(1);

                            assertEquals(
                                    ColorStateList.valueOf(
                                            ColorPickerUtils.getTabGroupColorPickerItemColor(
                                                    mSyncTestRule.getActivity(), color, false)),
                                    drawable.getColor());
                        });
    }

    private void verifyGroupVisualDataDialogOpenedAndDismiss(ChromeTabbedActivity cta) {
        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the visual data dialog exists.
        onViewWaiting(
                        withId(org.chromium.chrome.test.R.id.visual_data_dialog_layout),
                        /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // TODO(shaktisahu): Do we need to wait till keyboard is showing? Currently fails waiting.
        // Wait until the keyboard is showing.
        // KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
        // CriteriaHelper.pollUiThread(
        //     () -> delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));
        // Dismiss the tab group visual data dialog.
        dismissAllModalDialogs();
        // Verify that the modal dialog is now hidden.
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
    }

    private void verifyModalDialogShowingAnimationCompleteInTabSwitcher() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(true));
                });
    }

    private void verifyModalDialogHidingAnimationCompleteInTabSwitcher() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(false));
                });
    }

    private void dismissAllModalDialogs() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                });
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
        public long position;

        // Required for connecting with tabs. We don't use it for validation.
        public String syncId;

        public TabInfo(String title, String url, long position) {
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
        Assert.assertEquals("Tab title does not match", expectedTab.title, actualTab.getTitle());
        Assert.assertEquals(
                "Tab URL does not match", new GURL(expectedTab.url), actualTab.getOriginalUrl());
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
