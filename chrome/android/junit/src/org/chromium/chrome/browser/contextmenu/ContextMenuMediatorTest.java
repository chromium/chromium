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
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListItemType.SUBMENU_HEADER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

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
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the context menu mediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(UNIT_TESTS)
public class ContextMenuMediatorTest {

    // For submenu navigation tests
    private static final int TEST_MENU_ITEM_ID = 3; // Arbitrary int for testing
    private static final String TOP_LEVEL_ITEM = "Top level item";
    private static final String SUBMENU_LEVEL_0 = "Submenu level 0";
    private static final String SUBMENU_0_CHILD_1 = "Submenu 0 child 1";
    private static final String SUBMENU_LEVEL_1 = "Submenu level 1";
    private static final String SUBMENU_1_CHILD_0 = "Submenu 1 child 0";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private ContextMenuHeaderCoordinator mHeaderCoordinator;
    @Mock private Callback<Integer> mClickCallback;
    @Mock private OnClickListener mItemClickListener;
    @Mock private Runnable mDismissDialog;
    @Mock private Profile mProfile;
    @Mock private ListView mListView;

    private ContextMenuMediator mMediator;

    // For submenu navigation tests
    private ListItem mListItemWithModelClickCallback;
    private ListItem mSubmenuLevel1;
    private ListItem mSubmenu0Child1;
    private ListItem mSubmenuLevel0;
    private ListItem mListItemWithoutModelClickCallback;

    private HierarchicalMenuController mHierarchicalMenuController;

    @Before
    public void setup() {
        mMediator =
                new ContextMenuMediator(
                        mActivity, mHeaderCoordinator, mClickCallback, mDismissDialog);

        mListItemWithModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(ENABLED, true)
                                .with(TITLE, SUBMENU_1_CHILD_0)
                                .with(CLICK_LISTENER, mItemClickListener)
                                .build());

        mSubmenuLevel1 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_LEVEL_1)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(mListItemWithModelClickCallback))
                                .build());

        mSubmenu0Child1 =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_0_CHILD_1)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .build());
        mSubmenuLevel0 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_LEVEL_0)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(mSubmenuLevel1, mSubmenu0Child1))
                                .build());

        // Add an item with no click callback
        mListItemWithoutModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, TOP_LEVEL_ITEM)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .build());

        mHierarchicalMenuController = ListMenuUtils.createHierarchicalMenuController(mActivity);
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
        groupOne.add(
                createListItem(
                        ChromeContextMenuItem.Item.PICTURE_IN_PICTURE, "Picture in Picture"));
        rawItems.add(groupOne);
        ModelList itemList = getItemList(rawItems, /* hasHeader= */ true);

        assertThat(itemList.get(0).type, equalTo(ContextMenuItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.MENU_ITEM));
        assertThat(itemList.get(3).type, equalTo(ListItemType.MENU_ITEM));
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
        ModelList inputModelList = new ModelList();
        inputModelList.add(mSubmenuLevel0);
        inputModelList.add(mListItemWithoutModelClickCallback);
        ModelList modelList = getItemList(List.of(inputModelList), /* hasHeader= */ false);
        // Click into submenu 0
        activateClickListener(mSubmenuLevel0);
        assertEquals(
                "Expected submenu level 0 to have 3 items (1 header and 2 children)",
                3,
                modelList.size());
        ListItem header = modelList.get(0);
        assertEquals(
                "Expected 1st element after clicking into submenu level 0 to have header type",
                SUBMENU_HEADER,
                header.type);
        // Go back to the root level
        activateClickListener(header);
        verify(mDismissDialog, never()).run(); // Clicking into submenu and back should not dismiss
        // Verify correctness of model contents
        assertEquals("Expected root level to have 2 items", 2, modelList.size());
        assertEquals(
                "Expected 1st element of root level to be submenu level 0",
                mSubmenuLevel0,
                modelList.get(0));
        assertEquals(
                "Expected 2nd element of root level to be a menu item",
                mListItemWithoutModelClickCallback,
                modelList.get(1));
        // Go into submenu 0 again
        activateClickListener(mSubmenuLevel0);
        assertEquals(
                "Expected submenu 0 to still have 3 items", // No extra header or items
                3,
                modelList.size()); // Should still have 2 children (no extra header)
        // Go into submenu 1
        activateClickListener(mSubmenuLevel1);
        assertEquals(
                "Expected submenu 1 to have 2 items (1 header and 1 child)", // No extra header
                2,
                modelList.size());
        // Assert correctness of contents
        assertEquals(
                "Expected 1st element after clicking into submenu level 1 to have header type",
                SUBMENU_HEADER,
                modelList.get(0).type);
        assertEquals(
                "Expected 2nd element to be correct child",
                mListItemWithModelClickCallback,
                modelList.get(1));
    }

    @Test
    public void getItemList_withoutModelClickCallback_dismissAdded() {
        ModelList inputModelList = new ModelList();
        inputModelList.add(mSubmenuLevel0);
        inputModelList.add(mListItemWithoutModelClickCallback);
        getItemList(List.of(inputModelList), /* hasHeader= */ false);
        activateClickListener(mListItemWithoutModelClickCallback);
        verify(mClickCallback, times(1)).onResult(TEST_MENU_ITEM_ID);
        verify(mDismissDialog, times(1)).run();
    }

    @Test
    public void getItemList_withModelClickCallback_dismissAdded() {
        ModelList inputModelList = new ModelList();
        inputModelList.add(mSubmenuLevel0);
        inputModelList.add(mListItemWithoutModelClickCallback);
        getItemList(List.of(inputModelList), /* hasHeader= */ false);
        activateClickListener(mListItemWithModelClickCallback);
        verify(mClickCallback, never()).onResult(any());
        verify(mDismissDialog, times(1)).run();
    }

    private ModelList getItemList(List<ModelList> items, boolean hasHeader) {
        return mMediator.updateAndGetModelList(
                items, hasHeader, /* hierarchicalMenuController= */ mHierarchicalMenuController);
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

    private ListItem createListItem(@ChromeContextMenuItem.Item int item, String title) {
        final PropertyModel model =
                new PropertyModel.Builder(MENU_ITEM_ID, TITLE, ENABLED, CLICK_LISTENER)
                        .with(MENU_ITEM_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(ENABLED, true)
                        .with(TITLE, title)
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

    private void activateClickListener(ListItem item) {
        item.model.get(CLICK_LISTENER).onClick(mListView);
    }
}
