// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link GroupWindowInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupWindowInfoUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mTab4;
    @Mock private Tab mTab5;
    @Mock private TabList mComprehensiveModel;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
    }

    @Test
    public void testForSyncedGroup_explicitTitle() {
        Token token = new Token(1L, 2L);
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = new LocalTabGroupId(token);
        savedGroup.syncId = "sync-123";
        savedGroup.title = "My Group";
        savedGroup.color = TabGroupColorId.BLUE;
        savedGroup.updateTimeMs = 123456789L;

        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;
        savedGroup.savedTabs = List.of(tab1, tab2);

        GroupWindowInfo info =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroup, GroupWindowState.IN_CURRENT);

        assertEquals(token, info.localId);
        assertEquals("sync-123", info.syncId);
        assertEquals("My Group", info.title);
        assertEquals(TabGroupColorId.BLUE, info.color);
        assertEquals(2, info.tabCount);
        assertEquals(2, info.faviconUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, info.faviconUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, info.faviconUrls.get(1));
        assertEquals(GroupWindowState.IN_CURRENT, info.groupWindowState);
        assertEquals(123456789L, info.lastModifiedTimeMs);
    }

    @Test
    public void testForSyncedGroup_fallbackTitle() {
        SavedTabGroup savedGroupNullTitle = new SavedTabGroup();
        savedGroupNullTitle.title = null;
        savedGroupNullTitle.syncId = "sync-1";
        savedGroupNullTitle.color = TabGroupColorId.RED;
        savedGroupNullTitle.savedTabs = List.of(new SavedTabGroupTab(), new SavedTabGroupTab());

        GroupWindowInfo infoNull =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroupNullTitle, GroupWindowState.HIDDEN);
        assertEquals(TabGroupTitleUtils.getDefaultTitle(mContext, 2), infoNull.title);
        assertEquals(GroupWindowState.HIDDEN, infoNull.groupWindowState);
        assertNull(infoNull.localId);

        SavedTabGroup savedGroupEmptyTitle = new SavedTabGroup();
        savedGroupEmptyTitle.title = "";
        savedGroupEmptyTitle.syncId = "sync-2";
        savedGroupEmptyTitle.color = TabGroupColorId.GREEN;
        savedGroupEmptyTitle.savedTabs = List.of(new SavedTabGroupTab());

        GroupWindowInfo infoEmpty =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroupEmptyTitle, GroupWindowState.IN_ANOTHER);
        assertEquals(TabGroupTitleUtils.getDefaultTitle(mContext, 1), infoEmpty.title);
        assertEquals(GroupWindowState.IN_ANOTHER, infoEmpty.groupWindowState);
    }

    @Test
    public void testForLocalGroup_inCurrent() {
        Token groupId = new Token(3L, 4L);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab1.isClosing()).thenReturn(false);
        when(mTab1.getTimestampMillis()).thenReturn(1000L);

        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab2.isClosing()).thenReturn(false);
        when(mTab2.getTimestampMillis()).thenReturn(2000L);

        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(2);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.ORANGE);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getTabGroupTitle(groupId)).thenReturn("Local Group Title");

        GroupWindowInfo info =
                GroupWindowInfo.forLocalGroup(
                        mContext, mTabModel, groupId, GroupWindowState.IN_CURRENT);

        assertEquals(groupId, info.localId);
        assertNull(info.syncId);
        assertEquals("Local Group Title", info.title);
        assertEquals(TabGroupColorId.ORANGE, info.color);
        assertEquals(2, info.tabCount);
        assertEquals(2, info.faviconUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, info.faviconUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, info.faviconUrls.get(1));
        assertEquals(GroupWindowState.IN_CURRENT, info.groupWindowState);
        assertEquals(2000L, info.lastModifiedTimeMs);
    }

    @Test
    public void testForLocalGroup_inCurrentClosing() {
        Token groupId = new Token(5L, 6L);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab1.isClosing()).thenReturn(true);
        when(mTab1.getTimestampMillis()).thenReturn(5000L);

        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab2.isClosing()).thenReturn(true);
        when(mTab2.getTimestampMillis()).thenReturn(3000L);

        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(2);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.GREY);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getTabGroupTitle(groupId)).thenReturn("");

        GroupWindowInfo info =
                GroupWindowInfo.forLocalGroup(
                        mContext, mTabModel, groupId, GroupWindowState.IN_CURRENT_CLOSING);

        assertEquals(groupId, info.localId);
        assertNull(info.syncId);
        assertEquals(
                TabGroupTitleUtils.getDisplayableTitle(mContext, mTabModel, groupId), info.title);
        assertEquals(TabGroupColorId.GREY, info.color);
        assertEquals(2, info.tabCount);
        assertEquals(GroupWindowState.IN_CURRENT_CLOSING, info.groupWindowState);
        assertEquals(5000L, info.lastModifiedTimeMs);
    }

    @Test
    public void testForLocalGroup_partiallyClosing() {
        Token groupId = new Token(7L, 8L);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab1.isClosing()).thenReturn(true);
        when(mTab1.getTimestampMillis()).thenReturn(5000L);

        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab2.isClosing()).thenReturn(false);
        when(mTab2.getTimestampMillis()).thenReturn(1000L);

        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(2);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.PURPLE);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getTabGroupTitle(groupId)).thenReturn("Active partially closing");

        GroupWindowInfo info =
                GroupWindowInfo.forLocalGroup(
                        mContext, mTabModel, groupId, GroupWindowState.IN_CURRENT);

        assertEquals(groupId, info.localId);
        assertNull(info.syncId);
        assertEquals("Active partially closing", info.title);
        assertEquals(TabGroupColorId.PURPLE, info.color);
        assertEquals(2, info.tabCount);
        assertEquals(GroupWindowState.IN_CURRENT, info.groupWindowState);
        assertEquals(5000L, info.lastModifiedTimeMs);
    }

    @Test
    public void testForLocalGroup_explicitState() {
        Token groupId = new Token(9L, 10L);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab1.getTimestampMillis()).thenReturn(3000L);

        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(1);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.BLUE);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupTitle(groupId)).thenReturn("Local Explicit State");

        GroupWindowInfo info =
                GroupWindowInfo.forLocalGroup(
                        mContext, mTabModel, groupId, GroupWindowState.IN_ANOTHER);

        assertEquals(groupId, info.localId);
        assertNull(info.syncId);
        assertEquals("Local Explicit State", info.title);
        assertEquals(TabGroupColorId.BLUE, info.color);
        assertEquals(1, info.tabCount);
        assertEquals(1, info.faviconUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, info.faviconUrls.get(0));
        assertEquals(GroupWindowState.IN_ANOTHER, info.groupWindowState);
        assertEquals(3000L, info.lastModifiedTimeMs);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testForSyncedGroup_prioritizesLocalDataWhenClosing() {
        Token token = new Token(11L, 12L);
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = new LocalTabGroupId(token);
        savedGroup.syncId = "sync-closing";
        savedGroup.title = "Stale Remote Title";
        savedGroup.color = TabGroupColorId.BLUE;
        savedGroup.updateTimeMs = 1000L;
        savedGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTab1.getTabGroupId()).thenReturn(token);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab1.isClosing()).thenReturn(true);
        when(mTab1.getTimestampMillis()).thenReturn(9000L);

        when(mTab2.getTabGroupId()).thenReturn(token);
        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab2.isClosing()).thenReturn(true);
        when(mTab2.getTimestampMillis()).thenReturn(8000L);

        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mTab1, mTab2).iterator());
        when(mTabModel.getTabsInGroup(token)).thenReturn(Collections.emptyList());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);
        when(mTabModel.getTabGroupTitle(token)).thenReturn("Live Local Title");
        when(mTabModel.getTabGroupColorWithFallback(token)).thenReturn(TabGroupColorId.ORANGE);

        GroupWindowInfo info =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroup, GroupWindowState.IN_CURRENT_CLOSING);

        assertEquals(token, info.localId);
        assertEquals("sync-closing", info.syncId);
        assertEquals("Live Local Title", info.title);
        assertEquals(TabGroupColorId.ORANGE, info.color);
        assertEquals(2, info.tabCount);
        assertEquals(2, info.faviconUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, info.faviconUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, info.faviconUrls.get(1));
        assertEquals(GroupWindowState.IN_CURRENT_CLOSING, info.groupWindowState);
        assertEquals(9000L, info.lastModifiedTimeMs);

        // When local rawTitle is empty, should fall back to savedGroup.title.
        when(mTabModel.getTabGroupTitle(token)).thenReturn("");
        GroupWindowInfo infoFallbackSaved =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroup, GroupWindowState.IN_CURRENT_CLOSING);
        assertEquals("Stale Remote Title", infoFallbackSaved.title);

        // When both local rawTitle and savedGroup.title are empty, should fall back to default.
        savedGroup.title = "";
        GroupWindowInfo infoFallbackDefault =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroup, GroupWindowState.IN_CURRENT_CLOSING);
        assertEquals(TabGroupTitleUtils.getDefaultTitle(mContext, 2), infoFallbackDefault.title);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testForSyncedGroup_flagDisabled_doesNotPrioritizeLocalDataWhenClosing() {
        Token token = new Token(11L, 12L);
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = new LocalTabGroupId(token);
        savedGroup.syncId = "sync-closing";
        savedGroup.title = "Remote Title";
        savedGroup.color = TabGroupColorId.BLUE;
        savedGroup.updateTimeMs = 1000L;
        savedGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTab1.getTabGroupId()).thenReturn(token);
        when(mTab1.isClosing()).thenReturn(true);
        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mTab1).iterator());
        when(mTabModel.getTabsInGroup(token)).thenReturn(Collections.emptyList());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        GroupWindowInfo info =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroup, GroupWindowState.IN_CURRENT_CLOSING);

        // When flag is disabled, it should fall back to remote SavedTabGroup metadata.
        assertEquals("Remote Title", info.title);
        assertEquals(TabGroupColorId.BLUE, info.color);
        assertEquals(1, info.tabCount);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testForLocalGroup_flagDisabled_matchesBaseline() {
        Token groupId = Token.createRandom();
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(3);
        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(groupId)).thenReturn("Local Explicit");
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.BLUE);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());

        GroupWindowInfo info =
                GroupWindowInfo.forLocalGroup(
                        mContext, mTabModel, groupId, GroupWindowState.IN_CURRENT);

        assertEquals(groupId, info.localId);
        assertNull(info.syncId);
        assertEquals("Local Explicit", info.title);
        assertEquals(TabGroupColorId.BLUE, info.color);
        assertEquals(3, info.tabCount);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testForSyncedGroup_activeTabs_usesTabModelDirectlyWithoutComprehensiveModel() {
        Token token = new Token(21L, 22L);
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = new LocalTabGroupId(token);
        savedGroup.syncId = "sync-active";

        when(mTab1.getTabGroupId()).thenReturn(token);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTabModel.getTabsInGroup(token)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupTitle(token)).thenReturn("Active Group");
        when(mTabModel.getTabGroupColorWithFallback(token)).thenReturn(TabGroupColorId.BLUE);

        GroupWindowInfo info =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroup, GroupWindowState.IN_CURRENT);

        assertEquals("Active Group", info.title);
        assertEquals(1, info.tabCount);
        assertEquals(1, info.faviconUrls.size());
        verify(mTabModel, never()).getComprehensiveModel();
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testForSyncedGroup_faviconsClampedToCornerCount() {
        Token token = new Token(31L, 32L);
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = new LocalTabGroupId(token);
        savedGroup.syncId = "sync-many-tabs";

        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab3.getUrl()).thenReturn(JUnitTestGURLs.URL_3);
        when(mTab4.getUrl()).thenReturn(JUnitTestGURLs.RED_1);
        when(mTab5.getUrl()).thenReturn(JUnitTestGURLs.RED_2);

        when(mTabModel.getTabsInGroup(token))
                .thenReturn(List.of(mTab1, mTab2, mTab3, mTab4, mTab5));

        GroupWindowInfo info =
                GroupWindowInfo.forSyncedGroup(
                        mContext, mTabModel, savedGroup, GroupWindowState.IN_CURRENT);

        assertEquals(5, info.tabCount);
        assertEquals(TabGroupFaviconCluster.CORNER_COUNT, info.faviconUrls.size());
    }
}
