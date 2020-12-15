// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.TEXT;

import android.app.Activity;
import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuGroup;
import org.chromium.chrome.browser.contextmenu.RevampedContextMenuCoordinator.ListItemType;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserverJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for the Revamped context menu.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class RevampedContextMenuCoordinatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    PerformanceHintsObserver.Natives mNativeMock;
    @Mock
    ContextMenuNativeDelegate mNativeDelegate;

    private RevampedContextMenuCoordinator mCoordinator;
    private Activity mActivity;
    private final Profile mProfile = Mockito.mock(Profile.class);

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mCoordinator = new RevampedContextMenuCoordinator(0, mNativeDelegate);
        MockitoAnnotations.initMocks(this);
        mocker.mock(PerformanceHintsObserverJni.TEST_HOOKS, mNativeMock);
        when(mNativeMock.isContextMenuPerformanceInfoEnabled()).thenReturn(false);
    }

    @Test
    public void testGetItemListWithImageLink() {
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE,
                GURL.emptyGURL(), GURL.emptyGURL(), "", GURL.emptyGURL(), GURL.emptyGURL(), "",
                null, false, 0, 0, 0);
        List<Pair<Integer, ModelList>> rawItems = new ArrayList<>();
        // Link items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(Item.OPEN_IN_NEW_TAB));
        groupOne.add(createListItem(Item.OPEN_IN_INCOGNITO_TAB));
        groupOne.add(createListItem(Item.SAVE_LINK_AS));
        groupOne.add(createShareListItem(Item.SHARE_LINK));
        rawItems.add(new Pair<>(ContextMenuGroup.LINK, groupOne));
        // Image Items
        ModelList groupTwo = new ModelList();
        groupTwo.add(createListItem(Item.OPEN_IMAGE_IN_NEW_TAB));
        groupTwo.add(createListItem(Item.SAVE_IMAGE));
        groupTwo.add(createShareListItem(Item.SHARE_IMAGE));
        rawItems.add(new Pair<>(ContextMenuGroup.IMAGE, groupTwo));

        mCoordinator.initializeHeaderCoordinatorForTesting(
                mActivity, params, mProfile, mNativeDelegate);
        ModelList itemList = mCoordinator.getItemList(mActivity, rawItems, (i) -> {});

        assertThat(itemList.get(0).type, equalTo(ListItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(3).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(4).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(5).type, equalTo(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON));
        assertThat(itemList.get(6).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(7).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(8).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(9).type, equalTo(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON));
    }

    @Test
    public void testGetItemListWithLink() {
        // We're testing it for a link, but the mediaType in params is image. That's because if it
        // isn't image or video, the header mediator tries to get a favicon for us and calls
        // Profile.getLastUsedRegularProfile(), which throws an exception because native isn't
        // initialized. mediaType here doesn't have any effect on what we're testing.
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE,
                GURL.emptyGURL(), GURL.emptyGURL(), "", GURL.emptyGURL(), GURL.emptyGURL(), "",
                null, false, 0, 0, 0);
        List<Pair<Integer, ModelList>> rawItems = new ArrayList<>();
        // Link items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(Item.OPEN_IN_NEW_TAB));
        groupOne.add(createListItem(Item.OPEN_IN_INCOGNITO_TAB));
        groupOne.add(createListItem(Item.SAVE_LINK_AS));
        groupOne.add(createShareListItem(Item.SHARE_LINK));
        rawItems.add(new Pair<>(ContextMenuGroup.LINK, groupOne));

        mCoordinator.initializeHeaderCoordinatorForTesting(
                mActivity, params, mProfile, mNativeDelegate);
        ModelList itemList = mCoordinator.getItemList(mActivity, rawItems, (i) -> {});

        assertThat(itemList.get(0).type, equalTo(ListItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(3).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(4).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
        assertThat(itemList.get(5).type, equalTo(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON));
    }

    @Test
    public void testGetItemListWithVideo() {
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.VIDEO,
                GURL.emptyGURL(), GURL.emptyGURL(), "", GURL.emptyGURL(), GURL.emptyGURL(), "",
                null, false, 0, 0, 0);
        List<Pair<Integer, ModelList>> rawItems = new ArrayList<>();
        // Video items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(Item.SAVE_VIDEO));
        rawItems.add(new Pair<>(ContextMenuGroup.LINK, groupOne));

        mCoordinator.initializeHeaderCoordinatorForTesting(
                mActivity, params, mProfile, mNativeDelegate);
        ModelList itemList = mCoordinator.getItemList(mActivity, rawItems, (i) -> {});

        assertThat(itemList.get(0).type, equalTo(ListItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
    }

    private ListItem createListItem(@Item int item) {
        final PropertyModel model =
                new PropertyModel.Builder(RevampedContextMenuItemProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(TEXT, ChromeContextMenuItem.getTitle(mActivity, item, false))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM, model);
    }

    private ListItem createShareListItem(@Item int item) {
        final PropertyModel model =
                new PropertyModel.Builder(RevampedContextMenuItemWithIconButtonProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(TEXT, ChromeContextMenuItem.getTitle(mActivity, item, false))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON, model);
    }
}
