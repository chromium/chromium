// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

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
}
