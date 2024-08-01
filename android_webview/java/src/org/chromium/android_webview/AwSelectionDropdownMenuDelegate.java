// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.Display;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * WebView implementation of dropdown text selection menu delegate. The functionality provided by
 * this class is only available on Android U+.
 */
public class AwSelectionDropdownMenuDelegate implements SelectionDropdownMenuDelegate {

    private static final String TAG = "AwSelectionDropdown";

    private @Nullable PopupWindow mPopupWindow;
    private @Nullable Context mWindowContext;

    private AwSelectionDropdownMenuDelegate() {
        // No external instantiation.
    }

    @Override
    public void show(
            Context context,
            View rootView,
            MVCListAdapter.ModelList items,
            ItemClickListener clickListener,
            int x,
            int y) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            // WebView text selection drop-down menu is only supported on Android U+.
            return;
        }

        // Dismiss the previous popup window if it's showing.
        dismiss();

        // Offset the x & y coordinates based on the root view's location in
        // the window.
        final int[] locationInWindow = new int[2];
        rootView.getLocationInWindow(locationInWindow);
        x += locationInWindow[0];
        y += locationInWindow[1];

        final BasicListMenu menu = getListMenu(context, items, clickListener);
        final int[] menuDimensions = menu.getMenuDimensions();
        final int menuWidth =
                getIdealMenuWidth(
                        context,
                        menuDimensions[0],
                        context.getResources()
                                .getDimensionPixelSize(R.dimen.list_menu_popup_max_width));
        final int menuHeight = menuDimensions[1];

        // We will always try to show the menu to the right and below the anchor point unless
        // there isn't enough room. Padding is intentionally left out of the calculations below
        // because we want to allow the drop-down menu to show above the root's padding.
        final int spaceToRightOfMenu = rootView.getRight() - x;
        final boolean canShowRightOfAnchorPoint = spaceToRightOfMenu >= menuWidth;
        if (!canShowRightOfAnchorPoint) {
            // Check if there is enough room to the left instead.
            final int spaceToLeftOfMenu = x - rootView.getLeft();
            final boolean canShowLeftOfAnchorPoint = spaceToLeftOfMenu >= menuWidth;
            if (!canShowLeftOfAnchorPoint) {
                // There is not enough horizontal room for the drop-down menu.
                cleanup();
                return;
            }
        }

        final int spaceBelowMenu = rootView.getBottom() - y;
        final boolean canShowBelowAnchorPoint = spaceBelowMenu >= menuHeight;
        if (!canShowBelowAnchorPoint) {
            // Check if there is enough room above instead.
            final int spaceAboveMenu = y - rootView.getTop();
            final boolean canShowAboveAnchorPoint = spaceAboveMenu >= menuHeight;
            if (!canShowAboveAnchorPoint) {
                // There is not enough vertical room for the drop-down menu.
                cleanup();
                return;
            }
        }

        // Figure out the horizontal and vertical positioning of the menu based on space
        // available.
        x = canShowRightOfAnchorPoint ? x : x - menuWidth;
        y = canShowBelowAnchorPoint ? y : y - menuHeight;

        mPopupWindow = new PopupWindow(menu.getContentView(), menuWidth, menuHeight, true);
        mPopupWindow.setAnimationStyle(android.R.style.Animation_Dialog);
        mPopupWindow.setElevation(
                context.getResources().getDimensionPixelSize(R.dimen.list_menu_elevation));
        mPopupWindow.setOnDismissListener(this::cleanup);
        mPopupWindow.setFocusable(true);
        try {
            mPopupWindow.showAtLocation(rootView, Gravity.NO_GRAVITY, x, y);
        } catch (WindowManager.BadTokenException e) {
            // The app likely passed the wrong context into WebView e.g. the application
            // context, is being used in a multi-display environment, and the popup
            // window show attempt was on the wrong display.
            Log.e(
                    TAG,
                    "Could not show text selection drop-down. Did you pass the Activity Context to"
                            + " the WebView constructor?");
            cleanup();
        }
    }

    @Override
    public void dismiss() {
        if (mPopupWindow != null) {
            mPopupWindow.dismiss();
        }
    }

    @Override
    public int getGroupId(PropertyModel itemModel) {
        return PropertyModel.getFromModelOrDefault(itemModel, ListMenuItemProperties.GROUP_ID, 0);
    }

    @Override
    public int getItemId(PropertyModel itemModel) {
        return PropertyModel.getFromModelOrDefault(
                itemModel, ListMenuItemProperties.MENU_ITEM_ID, 0);
    }

    @Nullable
    @Override
    public Intent getItemIntent(PropertyModel itemModel) {
        return PropertyModel.getFromModelOrDefault(itemModel, ListMenuItemProperties.INTENT, null);
    }

    @Nullable
    @Override
    public View.OnClickListener getClickListener(PropertyModel itemModel) {
        return PropertyModel.getFromModelOrDefault(
                itemModel, ListMenuItemProperties.CLICK_LISTENER, null);
    }

    @Override
    public ListItem getDivider() {
        return BasicListMenu.buildMenuDivider();
    }

    @Override
    public ListItem getMenuItem(
            String title,
            @Nullable String contentDescription,
            int groupId,
            int id,
            @Nullable Drawable startIcon,
            boolean isIconTintable,
            boolean groupContainsIcon,
            boolean enabled,
            @Nullable View.OnClickListener clickListener,
            @Nullable Intent intent) {
        return BasicListMenu.buildListMenuItem(
                title,
                contentDescription,
                groupId,
                id,
                startIcon,
                isIconTintable,
                groupContainsIcon,
                enabled,
                clickListener,
                intent);
    }

    /** For nulling out references after drop-down dismissal or the inability to show. */
    private void cleanup() {
        mPopupWindow = null;
        mWindowContext = null;
    }

    @RequiresApi(Build.VERSION_CODES.S)
    @NonNull
    private BasicListMenu getListMenu(
            final @NonNull Context context,
            MVCListAdapter.ModelList items,
            ItemClickListener clickListener) {
        // `createWindowContext` on some devices writes to disk. See crbug.com/1408587.
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
            mWindowContext =
                    context.createWindowContext(
                            display, WindowManager.LayoutParams.TYPE_APPLICATION, null);
        }

        assert mWindowContext != null : "Window context cannot be null.";

        LayoutInflater inflater =
                (LayoutInflater) mWindowContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View contentView = inflater.inflate(R.layout.list_menu_layout, null);
        ListView listView = contentView.findViewById(R.id.menu_list);
        return new BasicListMenu(
                mWindowContext, items, contentView, listView, clickListener::onItemClick, 0);
    }

    /**
     * Returns the preferred dropdown width. Will ideally return the width of the widest list item
     * provided it falls within the bounds of a static min and max width.
     */
    private static int getIdealMenuWidth(
            @NonNull Context context, final int longestItemWidth, final int maxDropdownWidth) {
        final int minDropdownWidth =
                context.getResources().getDimensionPixelSize(R.dimen.list_menu_popup_min_width);
        return Math.min(Math.max(minDropdownWidth, longestItemWidth), maxDropdownWidth);
    }

    /**
     * This method is a no-op if the device is not Android U+. Creates and sets the WebView
     * drop-down text selection menu delegate on the {@link SelectionPopupController}.
     *
     * @param controller the selection popup controller to attach the delegate to.
     */
    public static void maybeSetWebViewDropdownSelectionMenuDelegate(
            SelectionPopupController controller) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return;
        controller.setDropdownMenuDelegate(new AwSelectionDropdownMenuDelegate());
    }
}
