// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;

import android.app.Activity;
import android.util.Pair;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuGroup;
import org.chromium.chrome.browser.contextmenu.RevampedContextMenuCoordinator.ListItemType;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for the Revamped context menu.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class RevampedContextMenuCoordinatorTest {
    private RevampedContextMenuCoordinator mCoordinator;
    private Activity mActivity;
    private WindowAndroid mWindow;

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mWindow = new ActivityWindowAndroid(mActivity, false);
        mCoordinator = new RevampedContextMenuCoordinator(0, null);
    }

    @Test
    public void testGetItemListWithImageLink() {
        final ContextMenuParams params = new ContextMenuParams(
                ContextMenuDataMediaType.IMAGE, "", "", "", "", "", "", null, false, 0, 0, 0);
        List<Pair<Integer, List<ContextMenuItem>>> rawItems = new ArrayList<>();
        // Link items
        List<ContextMenuItem> groupOne = new ArrayList<>();
        groupOne.add(new ChromeContextMenuItem(Item.OPEN_IN_NEW_TAB));
        groupOne.add(new ChromeContextMenuItem(Item.OPEN_IN_INCOGNITO_TAB));
        groupOne.add(new ChromeContextMenuItem(Item.SAVE_LINK_AS));
        groupOne.add(new ShareContextMenuItem(R.string.contextmenu_share_link,
                org.chromium.chrome.R.id.contextmenu_share_link, true));
        rawItems.add(new Pair<>(ContextMenuGroup.LINK, groupOne));
        // Image Items
        List<ContextMenuItem> groupTwo = new ArrayList<>();
        groupTwo.add(new ChromeContextMenuItem(Item.OPEN_IMAGE_IN_NEW_TAB));
        groupTwo.add(new ChromeContextMenuItem(Item.SAVE_IMAGE));
        groupTwo.add(new ShareContextMenuItem(R.string.contextmenu_share_image,
                org.chromium.chrome.R.id.contextmenu_share_image, false));
        rawItems.add(new Pair<>(ContextMenuGroup.IMAGE, groupTwo));

        mCoordinator.initializeHeaderCoordinatorForTesting(mActivity, params);
        ModelList itemList = mCoordinator.getItemList(mWindow, rawItems, params);

        assertThat(itemList.get(0).type, equalTo(ListItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(3).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(4).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(5).type, equalTo(ListItemType.CONTEXT_MENU_SHARE_ITEM));
        assertThat(itemList.get(6).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(7).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(8).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(9).type, equalTo(ListItemType.CONTEXT_MENU_SHARE_ITEM));
    }

    @Test
    public void testGetItemListWithLink() {
        // We're testing it for a link, but the mediaType in params is image. That's because if it
        // isn't image or video, the header mediator tries to get a favicon for us and calls
        // Profile.getLastUsedProfile(), which throws an exception because native isn't initialized.
        // mediaType here doesn't have any effect on what we're testing.
        final ContextMenuParams params = new ContextMenuParams(
                ContextMenuDataMediaType.IMAGE, "", "", "", "", "", "", null, false, 0, 0, 0);
        List<Pair<Integer, List<ContextMenuItem>>> rawItems = new ArrayList<>();
        // Link items
        List<ContextMenuItem> groupOne = new ArrayList<>();
        groupOne.add(new ChromeContextMenuItem(Item.OPEN_IN_NEW_TAB));
        groupOne.add(new ChromeContextMenuItem(Item.OPEN_IN_INCOGNITO_TAB));
        groupOne.add(new ChromeContextMenuItem(Item.SAVE_LINK_AS));
        groupOne.add(new ShareContextMenuItem(R.string.contextmenu_share_link,
                org.chromium.chrome.R.id.contextmenu_share_link, true));
        rawItems.add(new Pair<>(ContextMenuGroup.LINK, groupOne));

        mCoordinator.initializeHeaderCoordinatorForTesting(mActivity, params);
        ModelList itemList = mCoordinator.getItemList(mWindow, rawItems, params);

        assertThat(itemList.get(0).type, equalTo(ListItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(3).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(4).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(5).type, equalTo(ListItemType.CONTEXT_MENU_SHARE_ITEM));
    }

    @Test
    public void testGetItemListWithVideo() {
        final ContextMenuParams params = new ContextMenuParams(
                ContextMenuDataMediaType.VIDEO, "", "", "", "", "", "", null, false, 0, 0, 0);
        List<Pair<Integer, List<ContextMenuItem>>> rawItems = new ArrayList<>();
        // Video items
        List<ContextMenuItem> groupOne = new ArrayList<>();
        groupOne.add(new ChromeContextMenuItem(Item.SAVE_VIDEO));
        rawItems.add(new Pair<>(ContextMenuGroup.LINK, groupOne));

        mCoordinator.initializeHeaderCoordinatorForTesting(mActivity, params);
        ModelList itemList = mCoordinator.getItemList(mWindow, rawItems, params);

        assertThat(itemList.get(0).type, equalTo(ListItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
    }
}
