// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit tests for {@link GroupWindowInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupWindowInfoUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

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
                GroupWindowInfo.forSyncedGroup(mContext, savedGroup, GroupWindowState.IN_CURRENT);

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
                        mContext, savedGroupNullTitle, GroupWindowState.HIDDEN);
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
                        mContext, savedGroupEmptyTitle, GroupWindowState.IN_ANOTHER);
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
}
