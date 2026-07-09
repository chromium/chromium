// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

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
    public void testIndexOfTabCardsOrInvalid() {
        TabListModel tabListModel = new TabListModel();
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        tabListModel.add(listItemWithType(ModelType.TAB));
        tabListModel.add(listItemWithType(ModelType.MESSAGE));
        tabListModel.add(listItemWithType(ModelType.TAB));
        tabListModel.add(listItemWithType(ModelType.MESSAGE));

        assertEquals(TabModel.INVALID_TAB_INDEX, tabListModel.indexOfTabCardsOrInvalid(-1));
        assertEquals(TabModel.INVALID_TAB_INDEX, tabListModel.indexOfTabCardsOrInvalid(0));
        assertEquals(0, tabListModel.indexOfTabCardsOrInvalid(1));
        assertEquals(TabModel.INVALID_TAB_INDEX, tabListModel.indexOfTabCardsOrInvalid(2));
        assertEquals(1, tabListModel.indexOfTabCardsOrInvalid(3));
        assertEquals(TabModel.INVALID_TAB_INDEX, tabListModel.indexOfTabCardsOrInvalid(4));
        assertEquals(TabModel.INVALID_TAB_INDEX, tabListModel.indexOfTabCardsOrInvalid(5));
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
    }
}
