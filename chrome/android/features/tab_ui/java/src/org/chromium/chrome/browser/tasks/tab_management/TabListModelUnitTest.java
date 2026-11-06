// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabGroupItem;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabItem;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link TabListModel}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListModelUnitTest {
    private ListItem listItemWithType(@ModelType int type) {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, type)
                        .build();
        return new ListItem(UiType.TAB, propertyModel);
    }

    @Test
    public void testGetTabGroupIdForIndex() {
        Token groupId = Token.createRandom();
        TabListModel tabListModel = new TabListModel();
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        tabListModel.add(listItemWithType(ModelType.TAB));
        PropertyModel groupModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .build();
        tabListModel.add(new ListItem(UiType.TAB_GROUP, groupModel));

        assertNull(tabListModel.getTabGroupIdForIndex(-1));
        assertNull(tabListModel.getTabGroupIdForIndex(0));
        assertNull(tabListModel.getTabGroupIdForIndex(1));
        assertEquals(groupId, tabListModel.getTabGroupIdForIndex(2));
        assertNull(tabListModel.getTabGroupIdForIndex(3));
    }

    @Test
    public void testGetFirstTabPropertyModel() {
        TabListModel tabListModel = new TabListModel();
        assertNull(tabListModel.getFirstTabPropertyModel());

        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        assertNull(tabListModel.getFirstTabPropertyModel());

        ListItem firstTabItem = listItemWithType(ModelType.TAB);
        assertNotNull(firstTabItem.model);
        tabListModel.add(firstTabItem);
        assertEquals(firstTabItem.model, tabListModel.getFirstTabPropertyModel());

        tabListModel.add(listItemWithType(ModelType.TAB));
        assertEquals(firstTabItem.model, tabListModel.getFirstTabPropertyModel());

        tabListModel.clear();
        assertNull(tabListModel.getFirstTabPropertyModel());
        ListItem newFirstTabItem = listItemWithType(ModelType.TAB);
        assertNotNull(newFirstTabItem.model);
        tabListModel.add(newFirstTabItem);
        assertEquals(newFirstTabItem.model, tabListModel.getFirstTabPropertyModel());

        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        assertEquals(newFirstTabItem.model, tabListModel.getFirstTabPropertyModel());
    }

    @Test
    public void testTabRegionStart() {
        TabListModel tabListModel = new TabListModel();
        assertEquals(0, tabListModel.tabRegionStart());

        // Only message cards: the tab region starts after all of them.
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        assertEquals(1, tabListModel.tabRegionStart());
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        assertEquals(tabListModel.size(), tabListModel.tabRegionStart());

        // Leading message cards are skipped.
        tabListModel.add(listItemWithType(ModelType.TAB_GROUP));
        assertEquals(2, tabListModel.tabRegionStart());
        tabListModel.add(listItemWithType(ModelType.TAB));
        assertEquals(2, tabListModel.tabRegionStart());

        // Trailing message cards are ignored.
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        assertEquals(2, tabListModel.tabRegionStart());

        // Tabs only: the tab region starts at the beginning.
        tabListModel.clear();
        assertEquals(0, tabListModel.tabRegionStart());
        tabListModel.add(listItemWithType(ModelType.TAB));
        assertEquals(0, tabListModel.tabRegionStart());
        tabListModel.add(listItemWithType(ModelType.TAB_GROUP));
        assertEquals(0, tabListModel.tabRegionStart());
    }

    // TabListDataProvider item lookup tests.

    @Test
    public void testMatchesItem_TabItem() {
        PropertyModel tabModel = tabListItem(/* tabId= */ 1).model;
        PropertyModel messageModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.MESSAGE)
                        .with(TabProperties.TAB_ID, 1)
                        .build();
        PropertyModel groupModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_ID, 1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, Token.createRandom())
                        .build();

        TabItem matchingItem = tabItem(/* tabId= */ 1);
        TabItem otherItem = tabItem(/* tabId= */ 2);

        assertTrue(TabListModel.matchesItem(tabModel, matchingItem));
        assertFalse(TabListModel.matchesItem(tabModel, otherItem));
        assertFalse(TabListModel.matchesItem(messageModel, matchingItem));
        assertFalse(TabListModel.matchesItem(groupModel, matchingItem));
    }

    @Test
    public void testMatchesItem_TabGroupItem() {
        Token groupId = Token.createRandom();
        PropertyModel groupModel = tabGroupListItem(groupId).model;
        PropertyModel tabWithHeaderModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .build();
        PropertyModel tabModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB)
                        .build();

        TabGroupItem matchingItem = tabGroupItem(groupId);
        TabGroupItem otherItem = tabGroupItem(Token.createRandom());

        assertTrue(TabListModel.matchesItem(groupModel, matchingItem));
        assertTrue(TabListModel.matchesItem(tabWithHeaderModel, matchingItem));
        assertFalse(TabListModel.matchesItem(groupModel, otherItem));
        assertFalse(TabListModel.matchesItem(tabModel, matchingItem));
    }

    @Test
    public void testMatchesItems() {
        TabListModel tabListModel = new TabListModel();
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        tabListModel.add(tabListItem(/* tabId= */ 1));
        tabListModel.add(tabListItem(/* tabId= */ 2));

        // Message cards are ignored.
        assertTrue(
                tabListModel.matchesItems(
                        List.of(tabItem(/* tabId= */ 1), tabItem(/* tabId= */ 2))));
        // Order matters.
        assertFalse(
                tabListModel.matchesItems(
                        List.of(tabItem(/* tabId= */ 2), tabItem(/* tabId= */ 1))));
        // Missing items.
        assertFalse(tabListModel.matchesItems(List.of(tabItem(/* tabId= */ 1))));
    }

    @Test
    public void testResolveInsertionIndex_NullAfter() {
        TabListModel tabListModel = new TabListModel();
        // Empty model.
        assertEquals(0, tabListModel.resolveInsertionIndex(/* after= */ null));

        // Leading message card.
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        assertEquals(1, tabListModel.resolveInsertionIndex(/* after= */ null));

        // Existing tabs: insert before them, after the message card.
        tabListModel.add(tabListItem(/* tabId= */ 1));
        tabListModel.add(tabListItem(/* tabId= */ 2));
        assertEquals(1, tabListModel.resolveInsertionIndex(/* after= */ null));
    }

    @Test
    public void testIndexFromItemAndResolveInsertionIndex_TabItem() {
        TabListModel tabListModel = new TabListModel();
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        tabListModel.add(tabListItem(/* tabId= */ 1));
        tabListModel.add(tabListItem(/* tabId= */ 2));

        assertEquals(1, tabListModel.indexFromItem(tabItem(/* tabId= */ 1)));
        assertEquals(2, tabListModel.indexFromItem(tabItem(/* tabId= */ 2)));
        assertEquals(2, tabListModel.resolveInsertionIndex(tabItem(/* tabId= */ 1)));
        assertEquals(3, tabListModel.resolveInsertionIndex(tabItem(/* tabId= */ 2)));
    }

    @Test
    public void testIndexFromItemAndResolveInsertionIndex_TabGroupItem() {
        TabListModel tabListModel = new TabListModel();
        Token groupId = Token.createRandom();
        Token headerGroupId = Token.createRandom();
        tabListModel.add(tabListItem(/* tabId= */ 1));
        // A TAB_GROUP card.
        tabListModel.add(tabGroupListItem(groupId));
        // A TAB card carrying a TAB_GROUP_HEADER_ID.
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, headerGroupId)
                        .build();
        tabListModel.add(new ListItem(UiType.TAB, headerModel));

        assertEquals(1, tabListModel.indexFromItem(tabGroupItem(groupId)));
        assertEquals(2, tabListModel.indexFromItem(tabGroupItem(headerGroupId)));
        assertEquals(2, tabListModel.resolveInsertionIndex(tabGroupItem(groupId)));
        assertEquals(3, tabListModel.resolveInsertionIndex(tabGroupItem(headerGroupId)));
    }

    @Test
    public void testIndexFromItem_NotFound() {
        TabListModel tabListModel = new TabListModel();
        assertEquals(
                TabModel.INVALID_TAB_INDEX, tabListModel.indexFromItem(tabItem(/* tabId= */ 1)));

        tabListModel.add(tabListItem(/* tabId= */ 1));
        assertEquals(
                TabModel.INVALID_TAB_INDEX, tabListModel.indexFromItem(tabItem(/* tabId= */ 2)));
        assertEquals(
                TabModel.INVALID_TAB_INDEX,
                tabListModel.indexFromItem(tabGroupItem(Token.createRandom())));
    }

    // End TabListDataProvider item lookup tests.

    @Test
    public void testArchivedTabGroupHelpers() {
        TabListModel tabListModel = new TabListModel();

        // Add a regular TAB card
        PropertyModel tabModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB)
                        .build();
        tabListModel.add(new ListItem(UiType.TAB, tabModel));

        // Add an active TAB_GROUP card
        PropertyModel activeGroupModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GROUP_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB_GROUP)
                        .build();
        tabListModel.add(new ListItem(UiType.TAB_GROUP, activeGroupModel));

        // Add an ARCHIVED_TAB_GROUP card
        PropertyModel archivedGroupModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GROUP_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.ARCHIVED_TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_SYNC_ID, "sync_id_1")
                        .build();
        tabListModel.add(new ListItem(UiType.TAB_GROUP, archivedGroupModel));

        // Verify count helper: only the ARCHIVED_TAB_GROUP card should be counted
        assertEquals(1, tabListModel.getArchivedTabGroupCardCount());

        // Verify index lookup helper
        assertEquals(2, tabListModel.indexFromArchivedTabGroupSyncId("sync_id_1"));
        assertEquals(
                TabModel.INVALID_TAB_INDEX,
                tabListModel.indexFromArchivedTabGroupSyncId("non_existent"));

        // Verify model lookup helper
        assertEquals(
                archivedGroupModel, tabListModel.getModelFromArchivedTabGroupSyncId("sync_id_1"));
        assertNull(tabListModel.getModelFromArchivedTabGroupSyncId("non_existent"));

        // Verify indexFromModel lookup helper.
        assertEquals(0, tabListModel.indexFromModel(tabModel));
        assertEquals(1, tabListModel.indexFromModel(activeGroupModel));
        assertEquals(2, tabListModel.indexFromModel(archivedGroupModel));
        assertEquals(TabModel.INVALID_TAB_INDEX, tabListModel.indexFromModel(null));
        PropertyModel unaddedModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        assertEquals(TabModel.INVALID_TAB_INDEX, tabListModel.indexFromModel(unaddedModel));
    }

    @Test
    public void testMoveItem() {
        TabListModel tabListModel = new TabListModel();
        ListItem item0 = listItemWithType(ModelType.TAB);
        ListItem item1 = listItemWithType(ModelType.TAB_GROUP);
        ListItem item2 = listItemWithType(ModelType.MESSAGE);

        tabListModel.add(item0);
        tabListModel.add(item1);
        tabListModel.add(item2);

        // Move item0 from index 0 to index 1
        tabListModel.moveItem(0, 1);
        assertEquals(item1, tabListModel.get(0));
        assertEquals(item0, tabListModel.get(1));
        assertEquals(item2, tabListModel.get(2));

        // Move item0 (now index 1) to index 2 (end of list)
        tabListModel.moveItem(1, 2);
        assertEquals(item1, tabListModel.get(0));
        assertEquals(item2, tabListModel.get(1));
        assertEquals(item0, tabListModel.get(2));

        // Move item0 (now index 2) back to index 0
        tabListModel.moveItem(2, 0);
        assertEquals(item0, tabListModel.get(0));
        assertEquals(item1, tabListModel.get(1));
        assertEquals(item2, tabListModel.get(2));

        // Test invalid moves (should no-op)
        tabListModel.moveItem(-1, 1);
        tabListModel.moveItem(0, -1);
        tabListModel.moveItem(0, 3); // desIndex == size() is out-of-bounds
        tabListModel.moveItem(0, 4); // desIndex > size()
        tabListModel.moveItem(1, 1); // srcIndex == desIndex

        assertEquals(item0, tabListModel.get(0));
        assertEquals(item1, tabListModel.get(1));
        assertEquals(item2, tabListModel.get(2));
    }

    private ListItem tabListItem(int tabId) {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB)
                        .with(TabProperties.TAB_ID, tabId)
                        .build();
        return new ListItem(UiType.TAB, propertyModel);
    }

    private TabItem tabItem(int tabId) {
        return new TabItem(
                tabId,
                /* isSelected= */ false,
                /* isPinned= */ false,
                /* isMultiSelected= */ false);
    }

    private ListItem tabGroupListItem(Token groupId) {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CardProperties.CARD_TYPE, ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .build();
        return new ListItem(UiType.TAB_GROUP, propertyModel);
    }

    private TabGroupItem tabGroupItem(Token groupId) {
        return new TabGroupItem(groupId, /* isSelected= */ false, /* isCollapsed= */ false);
    }
}
