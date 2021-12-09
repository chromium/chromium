// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.TEXT;

import android.app.Activity;
import android.util.Pair;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuGroup;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinatorTest.ShadowContextMenuDialog;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserverJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.ContextMenuDialog;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for the context menu.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowContextMenuDialog.class)
@Features.DisableFeatures(ChromeFeatureList.CONTEXT_MENU_POPUP_STYLE)
public class ContextMenuCoordinatorTest {
    /**
     * Shadow class used to capture the inputs for {@link
     * ContextMenuCoordinator#createContextMenuDialog}.
     */
    @Implements(ContextMenuDialog.class)
    public static class ShadowContextMenuDialog {
        boolean mShouldRemoveScrim;

        public ShadowContextMenuDialog() {}

        @Implementation
        public void __constructor__(Activity ownerActivity, int theme, float touchPointXPx,
                float touchPointYPx, float topContentOffsetPx, int topMarginPx, int bottomMarginPx,
                View layout, View contentView, boolean isPopup, boolean shouldRemoveScrim,
                @Nullable Integer popupMargin, @Nullable Integer desiredPopupContentWidth) {
            mShouldRemoveScrim = shouldRemoveScrim;
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    PerformanceHintsObserver.Natives mNativeMock;
    @Mock
    ContextMenuNativeDelegate mNativeDelegate;

    private ContextMenuCoordinator mCoordinator;
    private Activity mActivity;
    private final Profile mProfile = Mockito.mock(Profile.class);

    @Before
    public void setUpTest() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator = new ContextMenuCoordinator(0, mNativeDelegate);
        MockitoAnnotations.initMocks(this);
        mocker.mock(PerformanceHintsObserverJni.TEST_HOOKS, mNativeMock);
        when(mNativeMock.isContextMenuPerformanceInfoEnabled()).thenReturn(false);
    }

    @Test
    public void testGetItemListWithImageLink() {
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE,
                GURL.emptyGURL(), GURL.emptyGURL(), "", GURL.emptyGURL(), GURL.emptyGURL(), "",
                null, false, 0, 0, 0, false);
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
        ModelList itemList = mCoordinator.getItemList(mActivity, rawItems, (i) -> {}, true);

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
                null, false, 0, 0, 0, false);
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
        ModelList itemList = mCoordinator.getItemList(mActivity, rawItems, (i) -> {}, true);

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
                null, false, 0, 0, 0, false);
        List<Pair<Integer, ModelList>> rawItems = new ArrayList<>();
        // Video items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(Item.SAVE_VIDEO));
        rawItems.add(new Pair<>(ContextMenuGroup.LINK, groupOne));

        mCoordinator.initializeHeaderCoordinatorForTesting(
                mActivity, params, mProfile, mNativeDelegate);
        ModelList itemList = mCoordinator.getItemList(mActivity, rawItems, (i) -> {}, true);

        assertThat(itemList.get(0).type, equalTo(ListItemType.HEADER));
        assertThat(itemList.get(1).type, equalTo(ListItemType.DIVIDER));
        assertThat(itemList.get(2).type, equalTo(ListItemType.CONTEXT_MENU_ITEM));
    }

    @Test
    public void testCreateContextMenuDialog() {
        View contentView = Mockito.mock(View.class);
        View rootView = Mockito.mock(View.class);

        ContextMenuDialog dialog = ContextMenuCoordinator.createContextMenuDialog(
                mActivity, rootView, contentView, /*isPopup=*/false, 0, 0, 0, 0, 0, 0, 0);
        ShadowContextMenuDialog shadowDialog = (ShadowContextMenuDialog) Shadow.extract(dialog);

        Assert.assertFalse("Dialog should have scrim behind.", shadowDialog.mShouldRemoveScrim);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONTEXT_MENU_POPUP_STYLE)
    public void testCreateContextMenuDialog_PopupStyle() {
        View contentView = Mockito.mock(View.class);
        View rootView = Mockito.mock(View.class);

        ContextMenuDialog dialog = ContextMenuCoordinator.createContextMenuDialog(
                mActivity, rootView, contentView, /*isPopup=*/true, 0, 0, 0, 0, 0, 0, 0);
        ShadowContextMenuDialog shadowDialog = (ShadowContextMenuDialog) Shadow.extract(dialog);

        Assert.assertTrue("Dialog should remove scrim behind.", shadowDialog.mShouldRemoveScrim);
    }

    private ListItem createListItem(@Item int item) {
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuItemProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(TEXT, ChromeContextMenuItem.getTitle(mActivity, item, false))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM, model);
    }

    private ListItem createShareListItem(@Item int item) {
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuItemWithIconButtonProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(TEXT, ChromeContextMenuItem.getTitle(mActivity, item, false))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON, model);
    }
}
