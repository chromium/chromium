// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;
import static org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties.SUBMENU_ITEMS;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListItemType.SUBMENU_HEADER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.app.Activity;
import android.view.View.OnClickListener;
import android.widget.ListView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ContextMenuItemType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.listmenu.ContextMenuSubmenuItemProperties;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the context menu mediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(UNIT_TESTS)
public class ContextMenuMediatorTest {

    private static final int TEST_MENU_ITEM_ID = 3; // Arbitrary int for testing
    private static final String LABEL = "Menu item label for testing";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private ContextMenuHeaderCoordinator mHeaderCoordinator;
    @Mock private Callback<Integer> mClickCallback;
    @Mock private OnClickListener mItemClickListener;
    @Mock private Runnable mDismissDialog;
    @Mock private Profile mProfile;
    @Mock private ListView mListView;

    private ContextMenuMediator mMediator;

    @Before
    public void setup() {
        mMediator =
                new ContextMenuMediator(
                        mActivity, mHeaderCoordinator, mClickCallback, mDismissDialog);
    }

    @Test
    public void testGetItemListWithImageLink() {
        List<ModelList> rawItems = new ArrayList<>();
        // Link items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(ChromeContextMenuItem.Item.OPEN_IN_NEW_TAB));
        groupOne.add(createListItem(ChromeContextMenuItem.Item.OPEN_IN_INCOGNITO_TAB));
        groupOne.add(createListItem(ChromeContextMenuItem.Item.SAVE_LINK_AS));
        groupOne.add(createShareListItem(ChromeContextMenuItem.Item.SHARE_LINK));
        rawItems.add(groupOne);
        // Image Items
        ModelList groupTwo = new ModelList();
        groupTwo.add(createListItem(ChromeContextMenuItem.Item.OPEN_IMAGE_IN_NEW_TAB));
        groupTwo.add(createListItem(ChromeContextMenuItem.Item.SAVE_IMAGE));
        groupTwo.add(createShareListItem(ChromeContextMenuItem.Item.SHARE_IMAGE));
        rawItems.add(groupTwo);

        ModelList itemList = getItemList(rawItems, /* hasHeader= */ true);

        assertThat(itemList.get(0).type, equalTo(ContextMenuItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(3).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(4).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(
                itemList.get(5).type,
                equalTo(ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON));
        assertThat(itemList.get(6).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(7).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(8).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(
                itemList.get(9).type,
                equalTo(ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON));
        // "Save link as"  and "save image" should be enabled.
        assertTrue(itemList.get(4).model.get(ENABLED));
        assertTrue(itemList.get(8).model.get(ENABLED));
    }

    @Test
    public void testGetItemListWithDownloadBlockedByPolicy() {
        List<ModelList> rawItems = new ArrayList<>();
        // Link items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(ChromeContextMenuItem.Item.OPEN_IN_NEW_TAB));
        groupOne.add(createListItem(ChromeContextMenuItem.Item.OPEN_IN_INCOGNITO_TAB));
        groupOne.add(createListItem(ChromeContextMenuItem.Item.SAVE_LINK_AS, false));
        groupOne.add(createShareListItem(ChromeContextMenuItem.Item.SHARE_LINK));
        rawItems.add(groupOne);
        // Image Items
        ModelList groupTwo = new ModelList();
        groupTwo.add(createListItem(ChromeContextMenuItem.Item.OPEN_IMAGE_IN_NEW_TAB));
        groupTwo.add(createListItem(ChromeContextMenuItem.Item.SAVE_IMAGE, false));
        groupTwo.add(createShareListItem(ChromeContextMenuItem.Item.SHARE_IMAGE));
        rawItems.add(groupTwo);

        ModelList itemList = getItemList(rawItems, /* hasHeader= */ true);

        assertThat(itemList.get(0).type, equalTo(ContextMenuItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(3).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(4).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(
                itemList.get(5).type,
                equalTo(ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON));
        assertThat(itemList.get(6).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(7).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(8).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(
                itemList.get(9).type,
                equalTo(ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON));
        // "Save link as"  and "save image" should be disabled.
        assertFalse(itemList.get(4).model.get(ENABLED));
        assertFalse(itemList.get(8).model.get(ENABLED));
    }

    @Test
    public void testGetItemListWithLink() {
        List<ModelList> rawItems = new ArrayList<>();
        // Link items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(ChromeContextMenuItem.Item.OPEN_IN_NEW_TAB));
        groupOne.add(createListItem(ChromeContextMenuItem.Item.OPEN_IN_INCOGNITO_TAB));
        groupOne.add(createListItem(ChromeContextMenuItem.Item.SAVE_LINK_AS));
        groupOne.add(createShareListItem(ChromeContextMenuItem.Item.SHARE_LINK));
        rawItems.add(groupOne);

        ModelList itemList = getItemList(rawItems, /* hasHeader= */ true);

        assertThat(itemList.get(0).type, equalTo(ContextMenuItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(3).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(4).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(
                itemList.get(5).type,
                equalTo(ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON));
    }

    @Test
    public void testGetItemListWithVideo() {
        List<ModelList> rawItems = new ArrayList<>();
        // Video items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(ChromeContextMenuItem.Item.SAVE_VIDEO));
        rawItems.add(groupOne);
        ModelList itemList = getItemList(rawItems, /* hasHeader= */ true);

        assertThat(itemList.get(0).type, equalTo(ContextMenuItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.MENU_ITEM));
    }

    @Test
    public void testClickItemEnabled() {
        mMediator.clickItemForTesting(TEST_MENU_ITEM_ID, /* enabled= */ true);
        verify(mClickCallback, times(1)).onResult(TEST_MENU_ITEM_ID);
        verify(mDismissDialog, times(1)).run();
    }

    @Test
    public void testClickItemDisabled() {
        mMediator.clickItemForTesting(TEST_MENU_ITEM_ID, /* enabled= */ false);
        verify(mClickCallback, times(1)).onResult(TEST_MENU_ITEM_ID);
        verify(mDismissDialog, never()).run();
    }

    @Test
    public void getItemList_submenuNavigation() {
        // Set up the ModelList
        ModelList items = new ModelList();

        ListItem listItemWithModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(ENABLED, true)
                                .with(TITLE, LABEL)
                                .with(CLICK_LISTENER, mItemClickListener)
                                .build());

        ListItem submenuLevel1 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ContextMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(listItemWithModelClickCallback))
                                .build());

        ListItem submenuLevel0 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ContextMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(submenuLevel1))
                                .build());
        items.add(submenuLevel0);

        // Add an item with no click callback
        ListItem listItemWithoutModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .build());
        items.add(listItemWithoutModelClickCallback);

        // Begin test
        ModelList modelList = getItemList(List.of(items), /* hasHeader= */ false);
        // Click into submenu 0
        submenuLevel0.model.get(CLICK_LISTENER).onClick(mListView);
        assertEquals(2, modelList.size());
        ListItem header = modelList.get(0);
        assertEquals(SUBMENU_HEADER, header.type);
        // Go back to the root level
        header.model.get(CLICK_LISTENER).onClick(mListView);
        verify(mDismissDialog, never()).run(); // Clicking into submenu and back should not dismiss
        // Verify correctness of model contents
        assertEquals(2, modelList.size());
        assertEquals(submenuLevel0, modelList.get(0));
        assertEquals(listItemWithoutModelClickCallback, modelList.get(1));
        // Go into submenu 0 again
        submenuLevel0.model.get(CLICK_LISTENER).onClick(mListView);
        assertEquals(2, modelList.size()); // Should still have 2 items (no extra header)
        // Go into submenu 1
        submenuLevel1.model.get(CLICK_LISTENER).onClick(mListView);
        assertEquals(2, modelList.size()); // Should still have 2 items (no extra header)
        // Assert correctness of contents
        assertEquals(SUBMENU_HEADER, modelList.get(0).type);
        assertEquals(listItemWithModelClickCallback, modelList.get(1));
    }

    @Test
    public void getItemList_withoutModelClickCallback_dismissAdded() {
        // Set up the ModelList
        ModelList items = new ModelList();

        ListItem listItemWithModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(ENABLED, true)
                                .with(TITLE, LABEL)
                                .with(CLICK_LISTENER, mItemClickListener)
                                .build());

        ListItem submenuLevel1 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ContextMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(listItemWithModelClickCallback))
                                .build());

        ListItem submenuLevel0 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ContextMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(submenuLevel1))
                                .build());
        items.add(submenuLevel0);

        // Add an item with no click callback
        ListItem listItemWithoutModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .build());
        items.add(listItemWithoutModelClickCallback);

        // Begin test
        getItemList(List.of(items), /* hasHeader= */ false);
        listItemWithoutModelClickCallback.model.get(CLICK_LISTENER).onClick(mListView);
        verify(mClickCallback, times(1)).onResult(TEST_MENU_ITEM_ID);
        verify(mDismissDialog, times(1)).run();
    }

    @Test
    public void getItemList_withModelClickCallback_dismissAdded() {
        // Set up the ModelList
        ModelList items = new ModelList();

        ListItem listItemWithModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(ENABLED, true)
                                .with(TITLE, LABEL)
                                .with(CLICK_LISTENER, mItemClickListener)
                                .build());

        ListItem submenuLevel1 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ContextMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(listItemWithModelClickCallback))
                                .build());

        ListItem submenuLevel0 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ContextMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(submenuLevel1))
                                .build());
        items.add(submenuLevel0);

        // Add an item with no click callback
        ListItem listItemWithoutModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, LABEL)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .build());
        items.add(listItemWithoutModelClickCallback);

        // Begin test
        getItemList(List.of(items), /* hasHeader= */ false);
        listItemWithModelClickCallback.model.get(CLICK_LISTENER).onClick(mListView);
        verify(mClickCallback, never()).onResult(any());
        verify(mDismissDialog, times(1)).run();
    }

    private ModelList getItemList(List<ModelList> items, boolean hasHeader) {
        return mMediator.updateAndGetModelList(items, hasHeader);
    }

    private ListItem createListItem(@ChromeContextMenuItem.Item int item) {
        return createListItem(item, /* enabled= */ true);
    }

    private ListItem createListItem(@ChromeContextMenuItem.Item int item, boolean enabled) {
        final PropertyModel model =
                new PropertyModel.Builder(MENU_ITEM_ID, TITLE, ENABLED, CLICK_LISTENER)
                        .with(MENU_ITEM_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(ENABLED, enabled)
                        .with(
                                TITLE,
                                ChromeContextMenuItem.getTitle(mActivity, mProfile, item, false))
                        .build();
        return new ListItem(ListItemType.MENU_ITEM, model);
    }

    private ListItem createShareListItem(@ChromeContextMenuItem.Item int item) {
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuItemWithIconButtonProperties.ALL_KEYS)
                        .with(MENU_ITEM_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(ENABLED, true)
                        .with(
                                TITLE,
                                ChromeContextMenuItem.getTitle(mActivity, mProfile, item, false))
                        .build();
        return new ListItem(ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON, model);
    }
}
