// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.hasSize;
import static org.mockito.Mockito.doReturn;

import static org.chromium.content_public.browser.test.util.TestSelectionDropdownMenuDelegate.ListMenuItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
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
import org.chromium.blink_public.common.ContextMenuDataMediaFlags;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.ContextMenuDialog;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuSwitches;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragStateTracker;
import org.chromium.ui.hierarchicalmenu.FlyoutController;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.List;

/** Unit tests for the context menu. Use density=mdpi so the screen density is 1. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
public class ContextMenuCoordinatorTest {
    private static final int TOP_CONTENT_OFFSET_PX = 17;
    public static final String PARENT_LABEL = "Parent item";

    /**
     * Shadow class used to capture the inputs for {@link
     * ContextMenuCoordinator#createContextMenuDialog}.
     */
    @Implements(ContextMenuDialog.class)
    public static class ShadowContextMenuDialog extends ShadowDialog {
        boolean mShouldRemoveScrim;
        boolean mDismissInvoked;
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
        public void dismiss() {
            mDismissInvoked = true;
        }
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

    /** Helper class to access the protected constructor of ViewAndroidDelegate. */
    public static class TestViewAndroidDelegate extends ViewAndroidDelegate {
        public TestViewAndroidDelegate(ViewGroup containerView) {
            super(containerView);
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock ContextMenuNativeDelegate mNativeDelegate;
    @Mock WebContentsImpl mWebContentsMock;
    @Mock private MenuModelBridge mMenuModelBridge;

    private ContextMenuCoordinator mCoordinator;
    private Activity mActivity;
    private final Profile mProfile = Mockito.mock(Profile.class);

    @Before
    public void setUpTest() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mCoordinator =
                new ContextMenuCoordinator(mActivity, TOP_CONTENT_OFFSET_PX, mNativeDelegate);
        ShadowProfile.sProfileFromWebContents = mProfile;
        ContextMenuHeaderCoordinator.setDisableForTesting(true);
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
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testCreateContextMenuDialog_PopupStyle() {
        ContextMenuDialog dialog = createContextMenuDialogForTest(/* isPopup= */ true);
        ShadowContextMenuDialog shadowDialog = (ShadowContextMenuDialog) Shadow.extract(dialog);

        Assert.assertTrue("Dialog should remove scrim behind.", shadowDialog.mShouldRemoveScrim);
        Assert.assertNotNull(
                "TouchEventDelegateView should not be null when drag drop is enabled.",
                shadowDialog.mTouchEventDelegateView);
    }

    @Test
    @Config(
            shadows = {ShadowContextMenuDialog.class, ShadowProfile.class},
            qualifiers = "mdpi")
    public void testDismissDialogCalledOnVisibilityChanged_Hidden() {
        final int triggeringTouchXDp = 100;
        final int triggeringTouchYDp = 200;
        ContextMenuDialog dialog =
                displayContextMenuDialogAtLocation(triggeringTouchXDp, triggeringTouchYDp);
        ShadowContextMenuDialog shadowDialog = (ShadowContextMenuDialog) Shadow.extract(dialog);
        shadowDialog.show();

        WebContentsObserver mWebContentsObserver = mCoordinator.getWebContentsObserverForTesting();

        mWebContentsObserver.onVisibilityChanged(Visibility.HIDDEN);

        Assert.assertTrue(shadowDialog.mDismissInvoked);
    }

    @Test
    @Config(
            shadows = {ShadowContextMenuDialog.class, ShadowProfile.class},
            qualifiers = "mdpi")
    public void testDismissDialogCalledOnVisibilityChanged_Visible() {
        final int triggeringTouchXDp = 100;
        final int triggeringTouchYDp = 200;
        ContextMenuDialog dialog =
                displayContextMenuDialogAtLocation(triggeringTouchXDp, triggeringTouchYDp);
        ShadowContextMenuDialog shadowDialog = (ShadowContextMenuDialog) Shadow.extract(dialog);
        shadowDialog.show();

        WebContentsObserver mWebContentsObserver = mCoordinator.getWebContentsObserverForTesting();

        mWebContentsObserver.onVisibilityChanged(Visibility.VISIBLE);

        Assert.assertFalse(shadowDialog.mDismissInvoked);
    }

    @Test
    public void testGetContextMenuTriggerRectFromWeb() {
        final int shadowImgWidth = 50;
        final int shadowImgHeight = 40;
        setupMocksForDragShadowImage(true, shadowImgWidth, shadowImgHeight);

        final int centerX = 100;
        final int centerY = 200;
        Rect rect = ContextMenuUtils.computeDragShadowRect(mWebContentsMock, centerX, centerY);

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
        Rect rect = ContextMenuUtils.computeDragShadowRect(mWebContentsMock, centerX, centerY);

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
        Rect rect = ContextMenuUtils.computeDragShadowRect(mWebContentsMock, centerX, centerY);

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
            shadows = {ShadowContextMenuDialog.class, ShadowProfile.class},
            qualifiers = "mdpi")
    public void testDisplayMenu() {
        final int triggeringTouchXDp = 100;
        final int triggeringTouchYDp = 200;
        ContextMenuDialog dialog =
                displayContextMenuDialogAtLocation(triggeringTouchXDp, triggeringTouchYDp);
        ShadowContextMenuDialog shadowDialog = Shadow.extract(dialog);

        List<ContextMenuListView> listViews = mCoordinator.getListViewsForTest();
        Assert.assertEquals("There should be exactly 1 ListView.", 1, listViews.size());
        ContextMenuListView listView = listViews.get(0);

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
                "rect.bottom for ContextMenuDialog does not match.", /*200 + 17 =*/
                217,
                rect.bottom);
    }

    @Test
    @DisabledTest(message = "crbug.com/1444964")
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @Config(
            shadows = {ShadowContextMenuDialog.class, ShadowProfile.class},
            qualifiers = "mdpi")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testDisplayMenu_DragEnabled() {
        final int shadowImgWidth = 50;
        final int shadowImgHeight = 40;
        setupMocksForDragShadowImage(true, shadowImgWidth, shadowImgHeight);

        final int triggeringTouchXDp = 100;
        final int triggeringTouchYDp = 200;
        ContextMenuDialog dialog =
                displayContextMenuDialogAtLocation(triggeringTouchXDp, triggeringTouchYDp);
        ShadowContextMenuDialog shadowDialog = Shadow.extract(dialog);

        List<ContextMenuListView> listViews = mCoordinator.getListViewsForTest();
        Assert.assertEquals("There should be exactly 1 ListView.", 1, listViews.size());
        ContextMenuListView listView = listViews.get(0);

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
                "rect.right for ContextMenuDialog does not match.", /*100 + 50 / 2 =*/
                125,
                rect.right);
        Assert.assertEquals(
                "rect.top for ContextMenuDialog does not match.", /*200 + 17 - 40 / 2 =*/
                197,
                rect.top);
        Assert.assertEquals(
                "rect.bottom for ContextMenuDialog does not match.",
                /*200 + 17 + 40 / 2 =*/ 237,
                rect.bottom);
    }

    @Test
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @Config(
            shadows = {ShadowContextMenuDialog.class, ShadowProfile.class},
            qualifiers = "mdpi")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testFocusAfterSubmenuNavigation() {
        final int triggeringTouchXDp = 100;
        final int triggeringTouchYDp = 200;

        List<ListItem> submenu =
                List.of(
                        new ListItem(
                                MENU_ITEM,
                                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                        .with(TITLE, "Example title")
                                        .with(ENABLED, true)
                                        .build()));
        ListItem submenuParent =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, PARENT_LABEL)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, submenu)
                                .build());
        ModelList modelList = new ModelList();
        modelList.add(submenuParent);

        ContextMenuDialog dialog =
                displayContextMenuDialogAtLocation(
                        triggeringTouchXDp, triggeringTouchYDp, List.of(modelList));
        ShadowContextMenuDialog shadowDialog = (ShadowContextMenuDialog) Shadow.extract(dialog);
        shadowDialog.show();

        List<ContextMenuListView> listViews = mCoordinator.getListViewsForTest();
        assertThat("Expected there to be 1 ContextMenuListView", listViews, hasSize(1));
        ContextMenuListView listView = listViews.get(0);
        // Navigate to submenu
        ListItem lastListItem = mCoordinator.getItem(mCoordinator.getCount() - 1);
        Assert.assertEquals(
                "Expected last list item to be the submenu parent",
                PARENT_LABEL,
                String.valueOf(lastListItem.model.get(TITLE)));
        lastListItem.model.get(CLICK_LISTENER).onClick(listView);

        // Verify that 1st item is selected
        Assert.assertEquals(
                "Expected 1st list item to be selected", 0, listView.getSelectedItemPosition());
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
                false,
                ContextMenuUtils.isPopupSupported(mActivity),
                0,
                0,
                0,
                0,
                webContentView,
                new Rect(0, 0, 0, 0),
                null);
    }

    private ContextMenuDialog displayContextMenuDialogAtLocation(
            int triggeringTouchXDp, int triggeringTouchYDp, List<ModelList> items) {
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
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
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        final WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        final Window window = Mockito.mock(Window.class);

        doReturn(new WeakReference<>(mActivity)).when(windowAndroid).getActivity();
        doReturn(window).when(windowAndroid).getWindow();
        final WindowManager.LayoutParams attrs = new WindowManager.LayoutParams();
        doReturn(attrs).when(window).getAttributes();
        final View mockDecorView = Mockito.mock(View.class);
        doReturn(mockDecorView).when(window).getDecorView();

        final ViewGroup mockContainerView = Mockito.mock(ViewGroup.class);
        final ViewAndroidDelegate viewAndroidDelegate =
                new TestViewAndroidDelegate(mockContainerView);
        doReturn(viewAndroidDelegate).when(mWebContentsMock).getViewAndroidDelegate();

        mCoordinator.displayMenu(windowAndroid, mWebContentsMock, params, items, null, null, null);

        FlyoutController<ContextMenuDialog> controller =
                mCoordinator.getHierarchicalMenuControllerForTest().getFlyoutController();
        Assert.assertEquals("mDialogs contains no windows.", 1, controller.getNumberOfPopups());
        return controller.getMainPopup();
    }

    private ContextMenuDialog displayContextMenuDialogAtLocation(
            int triggeringTouchXDp, int triggeringTouchYDp) {
        return displayContextMenuDialogAtLocation(
                triggeringTouchXDp, triggeringTouchYDp, List.of(new ModelList()));
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
