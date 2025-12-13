// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.hierarchicalmenu.FlyoutController;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.listmenu.ListSectionDividerProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.FlyoutPopupSpecCalculator;
import org.chromium.ui.widget.RectProvider;

/**
 * Chrome implementation of dropdown context menu which leverages {@link BasicListMenu} and {@link
 * AnchoredPopupWindow}.
 */
@NullMarked
public class ChromeSelectionDropdownMenuDelegate
        implements SelectionDropdownMenuDelegate, FlyoutHandler<AnchoredPopupWindow> {
    private @Nullable ItemClickListener mClickListener;
    private @Nullable View mRootView;
    private @Nullable HierarchicalMenuController mHierarchicalMenuController;

    @Override
    public void show(
            Context context,
            View rootView,
            MVCListAdapter.ModelList items,
            ItemClickListener clickListener,
            HierarchicalMenuController hierarchicalMenuController,
            int x,
            int y) {
        mRootView = rootView;
        mClickListener = clickListener;
        mHierarchicalMenuController = hierarchicalMenuController;

        Rect dropdownRect = new Rect(x, y, x + 1, y + 1);
        BasicListMenu menu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        context, items, (model, view) -> clickListener.onItemClick(model));

        AnchoredPopupWindow popupWindow =
                new AnchoredPopupWindow(
                        context,
                        rootView,
                        new ColorDrawable(Color.TRANSPARENT),
                        menu.getContentView(),
                        new RectProvider(dropdownRect));
        AnchoredPopupWindow.LayoutObserver layoutObserver =
                (positionBelow, x2, y2, width, height, anchorRect) ->
                        popupWindow.setAnimationStyle(
                                positionBelow
                                        ? R.style.StartIconMenuAnim
                                        : R.style.StartIconMenuAnimBottom);
        popupWindow.setLayoutObserver(layoutObserver);
        popupWindow.setVerticalOverlapAnchor(true);
        popupWindow.setHorizontalOverlapAnchor(true);
        popupWindow.setMaxWidth(
                context.getResources().getDimensionPixelSize(R.dimen.home_button_list_menu_width));
        popupWindow.setFocusable(true);
        popupWindow.setOutsideTouchable(true);
        popupWindow.addOnDismissListener(
                () -> {
                    dismiss();
                });

        popupWindow.show();

        mHierarchicalMenuController.setupFlyoutController(
                /* flyoutHandler= */ this, popupWindow, /* drillDownOverrideValue= */ null);
    }

    @Override
    public void dismiss() {
        if (mHierarchicalMenuController == null) {
            return;
        }

        if (mHierarchicalMenuController.getFlyoutController() != null) {
            mHierarchicalMenuController.destroyFlyoutController();
        }
    }

    @Override
    public Rect getPopupRect(AnchoredPopupWindow popupWindow) {
        View contentView = popupWindow.getContentView();

        if (contentView == null) {
            return new Rect();
        }

        return ListMenuUtils.getViewRectRelativeToItsRootView(contentView);
    }

    @Override
    public void dismissPopup(AnchoredPopupWindow popupWindow) {
        popupWindow.dismiss();
    }

    @Override
    public void setWindowFocus(AnchoredPopupWindow popupWindow, boolean hasFocus) {
        ViewGroup contentView = (ViewGroup) popupWindow.getContentView();
        if (contentView == null) return;

        HierarchicalMenuController.setWindowFocusForFlyoutMenus(contentView, hasFocus);
    }

    @Override
    public AnchoredPopupWindow createAndShowFlyoutPopup(
            ListItem item, View view, Runnable dismissRunnable) {
        Context context = view.getContext();

        BasicListMenu menu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        context,
                        ListMenuUtils.getModelListSubtree(item),
                        (model, unusedView) -> {
                            assert mClickListener != null;
                            mClickListener.onItemClick(model);
                        });

        final View contentView = menu.getContentView();

        final int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();

        assert mRootView != null;
        assert mHierarchicalMenuController != null;
        AnchoredPopupWindow popupMenu =
                new AnchoredPopupWindow.Builder(
                                context,
                                mRootView,
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> contentView,
                                new RectProvider(
                                        FlyoutController.calculateFlyoutAnchorRect(
                                                view, mRootView)))
                        .setVerticalOverlapAnchor(true)
                        .setHorizontalOverlapAnchor(false)
                        .setMaxWidth(
                                context.getResources()
                                        .getDimensionPixelSize(R.dimen.home_button_list_menu_width))
                        .setFocusable(true)
                        .setTouchModal(false)
                        .setAnimateFromAnchor(false)
                        .setAnimationStyle(R.style.PopupWindowAnimFade)
                        .setSpecCalculator(new FlyoutPopupSpecCalculator())
                        .setDesiredContentWidth(menu.getMaxItemWidth() + lateralPadding)
                        .addOnDismissListener(
                                () -> {
                                    dismissRunnable.run();
                                })
                        .build();

        popupMenu.show();
        return popupMenu;
    }

    @Override
    public ListItem getDivider() {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListSectionDividerProperties.ALL_KEYS)
                        .with(
                                ListSectionDividerProperties.LEFT_PADDING_DIMEN_ID,
                                R.dimen.list_menu_item_horizontal_padding)
                        .with(
                                ListSectionDividerProperties.RIGHT_PADDING_DIMEN_ID,
                                R.dimen.list_menu_item_horizontal_padding);
        return new ListItem(ListItemType.DIVIDER, builder.build());
    }

    @Override
    public ListItem getMenuItem(
            @Nullable String title,
            @Nullable String contentDescription,
            int groupId,
            int id,
            @Nullable Drawable startIcon,
            boolean isIconTintable,
            boolean groupContainsIcon,
            boolean enabled,
            @Nullable Intent intent,
            int order) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.CONTENT_DESCRIPTION, contentDescription)
                        .with(ListMenuItemProperties.GROUP_ID, groupId)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, id)
                        .with(ListMenuItemProperties.START_ICON_DRAWABLE, startIcon)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.INTENT, intent)
                        .with(
                                ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN,
                                groupContainsIcon)
                        .with(
                                ListMenuItemProperties.TEXT_APPEARANCE_ID,
                                BrowserUiListMenuUtils.getDefaultTextAppearanceStyle())
                        .with(ListMenuItemProperties.ORDER, order);
        if (isIconTintable) {
            modelBuilder.with(
                    ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                    BrowserUiListMenuUtils.getDefaultIconTintColorStateListId());
        }
        return new ListItem(ListItemType.MENU_ITEM, modelBuilder.build());
    }
}
