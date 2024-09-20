// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.ENABLED;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.TEXT;

import android.app.Activity;
import android.graphics.Rect;
import android.util.Pair;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuGroup;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.ContextMenuDialog;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragStateTracker;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for the context menu. Use density=mdpi so the screen density is 1. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
@EnableFeatures({ChromeFeatureList.CONTEXT_MENU_SYS_UI_MATCHES_ACTIVITY})
public class ContextMenuCoordinatorTest {
    private static final int TOP_CONTENT_OFFSET_PX = 17;

    /**
     * Shadow class used to capture the inputs for {@link
     * ContextMenuCoordinator#createContextMenuDialog}.
     */
    @Implements(ContextMenuDialog.class)
    public static class ShadowContextMenuDialog extends ShadowDialog {
        boolean mShouldRemoveScrim;
        @Nullable View mTouchEventDelegateView;
        Rect mRect;

        public ShadowContextMenuDialog() {}

        @Implementation
        protected void __constructor__(
                Activity ownerActivity,
                int theme,
                int topMarginPx,
                int bottomMarginPx,
                View layout,
                View contentView,
                boolean isPopup,
                boolean shouldRemoveScrim,
                @Nullable Integer popupMargin,
                @Nullable Integer desiredPopupContentWidth,
                @Nullable View touchEventDelegateView,
                Rect rect) {
            mShouldRemoveScrim = shouldRemoveScrim;
            mTouchEventDelegateView = touchEventDelegateView;
            mRect = rect;
        }

        @Override
        @Implementation
        public void show() {}

        @Override
        @Implementation
        public void dismiss() {}
    }

    /** No-op constructor for test cases that does not care of creation of real object. */
    @Implements(ContextMenuHeaderCoordinator.class)
    public static class ShadowContextMenuHeaderCoordinator {
        public ShadowContextMenuHeaderCoordinator() {}

        @Implementation
        public void __constructor__(
                Activity activity,
                ContextMenuParams params,
                Profile profile,
                ContextMenuNativeDelegate nativeDelegate) {}
    }

    /** Helper shadow to set the results for {@link Profile#fromWebContents}. */
    @Implements(Profile.class)
    public static class ShadowProfile {
        static Profile sProfileFromWebContents;

        @Implementation
        public static Profile fromWebContents(WebContents webContents) {
            return sProfileFromWebContents;
        }
    }

    @Rule public JniMocker mocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock ContextMenuNativeDelegate mNativeDelegate;
    @Mock WebContents mWebContentsMock;

    private ContextMenuCoordinator mCoordinator;
    private Activity mActivity;
    private final Profile mProfile = Mockito.mock(Profile.class);

    @Before
    public void setUpTest() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mCoordinator = new ContextMenuCoordinator(TOP_CONTENT_OFFSET_PX, mNativeDelegate);
        MockitoAnnotations.initMocks(this);
        ShadowProfile.sProfileFromWebContents = mProfile;
    }

    @Test
    public void testGetItemListWithImageLink() {
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);
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
        // "Save link as"  and "save image" should be enabled.
        assertTrue(itemList.get(4).model.get(ENABLED));
        assertTrue(itemList.get(8).model.get(ENABLED));
    }

    @Test
    public void testGetItemListWithDownloadBlockedByPolicy() {
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);
        List<Pair<Integer, ModelList>> rawItems = new ArrayList<>();
        // Link items
        ModelList groupOne = new ModelList();
        groupOne.add(createListItem(Item.OPEN_IN_NEW_TAB));
        groupOne.add(createListItem(Item.OPEN_IN_INCOGNITO_TAB));
        groupOne.add(createListItem(Item.SAVE_LINK_AS, false));
        groupOne.add(createShareListItem(Item.SHARE_LINK));
        rawItems.add(new Pair<>(ContextMenuGroup.LINK, groupOne));
        // Image Items
        ModelList groupTwo = new ModelList();
        groupTwo.add(createListItem(Item.OPEN_IMAGE_IN_NEW_TAB));
        groupTwo.add(createListItem(Item.SAVE_IMAGE, false));
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
        // "Save link as"  and "save image" should be disabled.
        assertFalse(itemList.get(4).model.get(ENABLED));
        assertFalse(itemList.get(8).model.get(ENABLED));
    }

    @Test
    public void testGetItemListWithLink() {
        // We're testing it for a link, but the mediaType in params is image. That's because if it
        // isn't image or video, the header mediator tries to get a favicon for us and calls
        // ProfileManager.getLastUsedRegularProfile(), which throws an exception because native
        // isn't
        // initialized. mediaType here doesn't have any effect on what we're testing.
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);
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
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.VIDEO,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);
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
    @Config(
            shadows = {ShadowContextMenuDialog.class},
            qualifiers = "mdpi")
    public void testCreateContextMenuDialog() {
        ContextMenuDialog dialog = createContextMenuDialogForTest(/* isPopup= */ false);
        ShadowContextMenuDialog shadowDialog = (ShadowContextMenuDialog) Shadow.extract(dialog);

        Assert.assertFalse("Dialog should have scrim behind.", shadowDialog.mShouldRemoveScrim);
    }

    @Test
    @DisabledTest(message = "crbug.com/1444964")
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @Config(
            shadows = {ShadowContextMenuDialog.class},
            qualifiers = "mdpi")
    @CommandLineFlags.Add(ChromeSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testCreateContextMenuDialog_PopupStyle() {
        ContextMenuDialog dialog = createContextMenuDialogForTest(/* isPopup= */ true);
        ShadowContextMenuDialog shadowDialog = (ShadowContextMenuDialog) Shadow.extract(dialog);

        Assert.assertTrue("Dialog should remove scrim behind.", shadowDialog.mShouldRemoveScrim);
        Assert.assertNotNull(
                "TouchEventDelegateView should not be null when drag drop is enabled.",
                shadowDialog.mTouchEventDelegateView);
    }

    @Test
    public void testGetContextMenuTriggerRectFromWeb() {
        final int shadowImgWidth = 50;
        final int shadowImgHeight = 40;
        setupMocksForDragShadowImage(true, shadowImgWidth, shadowImgHeight);

        final int centerX = 100;
        final int centerY = 200;
        Rect rect =
                ContextMenuCoordinator.getContextMenuTriggerRectFromWeb(
                        mWebContentsMock, centerX, centerY);

        Assert.assertEquals("rect.left does not match.", /*100 - 50 / 2 =*/ 75, rect.left);
        Assert.assertEquals("rect.right does not match.", /*100 + 50 / 2 =*/ 125, rect.right);
        Assert.assertEquals("rect.top does not match.", /*200 - 40 / 2 =*/ 180, rect.top);
        Assert.assertEquals("rect.bottom does not match.", /*200 + 40 / 2 =*/ 220, rect.bottom);
    }

    @Test
    public void testGetContextMenuTriggerRectFromWeb_DragNotStarted() {
        setupMocksForDragShadowImage(false, 50, 40);

        final int centerX = 100;
        final int centerY = 200;
        Rect rect =
                ContextMenuCoordinator.getContextMenuTriggerRectFromWeb(
                        mWebContentsMock, centerX, centerY);

        // Rect should be a point when drag not started.
        Assert.assertEquals("rect.left does not match.", centerX, rect.left);
        Assert.assertEquals("rect.right does not match.", centerX, rect.right);
        Assert.assertEquals("rect.top does not match.", centerY, rect.top);
        Assert.assertEquals("rect.bottom does not match.", centerY, rect.bottom);
    }

    @Test
    public void testGetContextMenuTriggerRectFromWeb_NoViewAndroidDelegate() {
        final int centerX = 100;
        final int centerY = 200;
        Rect rect =
                ContextMenuCoordinator.getContextMenuTriggerRectFromWeb(
                        mWebContentsMock, centerX, centerY);

        // Rect should be a point when no ViewAndroidDelegate attached to web content.
        Assert.assertEquals("rect.left does not match.", centerX, rect.left);
        Assert.assertEquals("rect.right does not match.", centerX, rect.right);
        Assert.assertEquals("rect.top does not match.", centerY, rect.top);
        Assert.assertEquals("rect.bottom does not match.", centerY, rect.bottom);
    }

    @Test
    @DisabledTest(message = "crbug.com/1444964")
    @DisableFeatures(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU)
    @Config(
            shadows = {
                ShadowContextMenuDialog.class,
                ShadowContextMenuHeaderCoordinator.class,
                ShadowProfile.class
            },
            qualifiers = "mdpi")
    public void testDisplayMenu() {
        final int triggeringTouchXDp = 100;
        final int triggeringTouchYDp = 200;
        ContextMenuDialog dialog =
                displayContextMenuDialogAtLocation(triggeringTouchXDp, triggeringTouchYDp);
        ShadowContextMenuDialog shadowDialog = Shadow.extract(dialog);

        ContextMenuListView listView = mCoordinator.getListViewForTest();
        Assert.assertNotNull("List view should not be null.", listView);
        Assert.assertFalse(
                "Fading edge should not be enabled.", listView.isVerticalFadingEdgeEnabled());

        // Verify rect is calculated correctly. Note that the calculation done below assume the
        // density is 1.0.
        Rect rect = shadowDialog.mRect;
        Assert.assertEquals("rect.left for ContextMenuDialog does not match.", 100, rect.left);
        Assert.assertEquals("rect.right for ContextMenuDialog does not match.", 100, rect.right);
        Assert.assertEquals(
                "rect.top for ContextMenuDialog does not match.", /*200 + 17 =*/ 217, rect.top);
        Assert.assertEquals(
                "rect.bottom for ContextMenuDialog does not match.",
                /*200 + 17 =*/ 217,
                rect.bottom);
    }

    @Test
    @DisabledTest(message = "crbug.com/1444964")
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @Config(
            shadows = {
                ShadowContextMenuDialog.class,
                ShadowContextMenuHeaderCoordinator.class,
                ShadowProfile.class
            },
            qualifiers = "mdpi")
    @CommandLineFlags.Add(ChromeSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testDisplayMenu_DragEnabled() {
        final int shadowImgWidth = 50;
        final int shadowImgHeight = 40;
        setupMocksForDragShadowImage(true, shadowImgWidth, shadowImgHeight);

        final int triggeringTouchXDp = 100;
        final int triggeringTouchYDp = 200;
        ContextMenuDialog dialog =
                displayContextMenuDialogAtLocation(triggeringTouchXDp, triggeringTouchYDp);
        ShadowContextMenuDialog shadowDialog = Shadow.extract(dialog);

        ContextMenuListView listView = mCoordinator.getListViewForTest();
        Assert.assertNotNull("List view should not be null.", listView);
        Assert.assertTrue("Fading edge should be enabled.", listView.isVerticalFadingEdgeEnabled());
        Assert.assertEquals(
                "Fading edge size is wrong.",
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.context_menu_fading_edge_size),
                listView.getVerticalFadingEdgeLength());

        // Verify rect is calculated correctly.
        Rect rect = shadowDialog.mRect;
        Assert.assertEquals(
                "rect.left for ContextMenuDialog does not match.", /*100 - 50 / 2 =*/
                75,
                rect.left);
        Assert.assertEquals(
                "rect.right for ContextMenuDialog does not match.",
                /*100 + 50 / 2 =*/ 125,
                rect.right);
        Assert.assertEquals(
                "rect.top for ContextMenuDialog does not match.",
                /*200 + 17 - 40 / 2 =*/ 197,
                rect.top);
        Assert.assertEquals(
                "rect.bottom for ContextMenuDialog does not match.",
                /*200 + 17 + 40 / 2 =*/ 237,
                rect.bottom);
    }

    private ListItem createListItem(@Item int item) {
        return createListItem(item, /* enabled= */ true);
    }

    private ListItem createListItem(@Item int item, boolean enabled) {
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuItemProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(ENABLED, enabled)
                        .with(
                                TEXT,
                                ChromeContextMenuItem.getTitle(mActivity, mProfile, item, false))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM, model);
    }

    private ListItem createShareListItem(@Item int item) {
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuItemWithIconButtonProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(ENABLED, true)
                        .with(
                                TEXT,
                                ChromeContextMenuItem.getTitle(mActivity, mProfile, item, false))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON, model);
    }

    private ContextMenuDialog createContextMenuDialogForTest(boolean isPopup) {
        View contentView = Mockito.mock(View.class);
        View rootView = Mockito.mock(View.class);
        View webContentView = Mockito.mock(View.class);

        return ContextMenuCoordinator.createContextMenuDialog(
                mActivity,
                rootView,
                contentView,
                isPopup,
                0,
                0,
                0,
                0,
                webContentView,
                new Rect(0, 0, 0, 0));
    }

    private ContextMenuDialog displayContextMenuDialogAtLocation(
            int triggeringTouchXDp, int triggeringTouchYDp) {
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        triggeringTouchXDp,
                        triggeringTouchYDp,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);

        final WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        doReturn(new WeakReference<Activity>(mActivity)).when(windowAndroid).getActivity();

        List<Pair<Integer, ModelList>> rawItems = new ArrayList<>();

        mCoordinator.displayMenu(
                windowAndroid, mWebContentsMock, params, rawItems, null, null, null);

        ContextMenuDialog dialog = mCoordinator.getDialogForTest();
        Assert.assertNotNull("ContextMenuDialog is null", dialog);
        return dialog;
    }

    private void setupMocksForDragShadowImage(
            boolean isDragging, int dragShadowWidth, int dragShadowHeight) {
        final ViewAndroidDelegate viewAndroidDelegate = Mockito.mock(ViewAndroidDelegate.class);
        final DragStateTracker dragStateTracker = Mockito.mock(DragStateTracker.class);
        doReturn(viewAndroidDelegate).when(mWebContentsMock).getViewAndroidDelegate();
        doReturn(dragStateTracker).when(viewAndroidDelegate).getDragStateTracker();

        doReturn(isDragging).when(dragStateTracker).isDragStarted();
        doReturn(dragShadowWidth).when(dragStateTracker).getDragShadowWidth();
        doReturn(dragShadowHeight).when(dragStateTracker).getDragShadowHeight();
    }
}
