// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuUtils.createAdapter;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.listmenu.ListMenuItemAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.Set;

/**
 * Coordinator for the context menu on the tab strip. It is responsible for creating a list of menu
 * items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class TabStripContextMenuCoordinator {
    private final Context mContext;
    private final MultiInstanceManager mMultiInstanceManager;
    private @Nullable AnchoredPopupWindow mMenuWindow;

    /**
     * @param context The {@link Context} to build the menu with.
     * @param multiInstanceManager The {@link MultiInstanceManager} instance to facilitate window
     *     tasks.
     */
    public TabStripContextMenuCoordinator(
            Context context, MultiInstanceManager multiInstanceManager) {
        mContext = context;
        mMultiInstanceManager = multiInstanceManager;
    }

    /**
     * Shows the context menu.
     *
     * @param anchorViewRectProvider The {@link RectProvider} for the anchor view.
     * @param isIncognito Whether the menu is shown in incognito mode.
     * @param activity The {@link Activity} in which the menu is shown.
     */
    public void showMenu(
            RectProvider anchorViewRectProvider, boolean isIncognito, Activity activity) {
        ModelList modelList = new ModelList();
        configureMenuItems(modelList, isIncognito);

        Drawable background = TabOverflowMenuCoordinator.getMenuBackground(mContext, isIncognito);

        // TODO (crbug.com/436283175): Update the name of this resource for generic use.
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.tab_switcher_action_menu_layout, null);
        clipContentViewOutline(contentView);

        // TODO (crbug.com/436283175): Update the name of this resource for generic use.
        TouchTrackingListView touchTrackingListView =
                contentView.findViewById(R.id.tab_group_action_menu_list);
        ListMenuItemAdapter adapter =
                createAdapter(modelList, Set.of(), getListMenuDelegate(contentView));
        touchTrackingListView.setItemsCanFocus(true);
        touchTrackingListView.setAdapter(adapter);

        View decorView = activity.getWindow().getDecorView();
        mMenuWindow =
                new AnchoredPopupWindow(
                        mContext, decorView, background, contentView, anchorViewRectProvider);
        mMenuWindow.setFocusable(true);
        mMenuWindow.setHorizontalOverlapAnchor(true);
        mMenuWindow.setVerticalOverlapAnchor(true);
        mMenuWindow.setPreferredHorizontalOrientation(HorizontalOrientation.LAYOUT_DIRECTION);
        mMenuWindow.setElevation(
                contentView.getResources().getDimension(R.dimen.tab_overflow_menu_elevation));
        mMenuWindow.setAnimateFromAnchor(true);
        var popupWidthPx =
                MathUtils.clamp(
                        anchorViewRectProvider.getRect().width(),
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width));
        mMenuWindow.setMaxWidth(popupWidthPx);
        mMenuWindow.show();
    }

    private void configureMenuItems(ModelList itemList, boolean isIncognito) {
        // Add "Name window" option.
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()
                && ChromeFeatureList.sRobustWindowManagement.isEnabled()) {
            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.menu_name_window)
                            .withMenuId(R.id.name_window)
                            .withIsIncognito(isIncognito)
                            .build());
        }
    }

    private void clipContentViewOutline(View contentView) {
        GradientDrawable outlineDrawable = new GradientDrawable();
        outlineDrawable.setShape(GradientDrawable.RECTANGLE);
        outlineDrawable.setCornerRadius(
                AttrUtils.getDimensionPixelSize(
                        contentView.getContext(), R.attr.popupBgCornerRadius));
        contentView.setBackground(outlineDrawable);
        contentView.setClipToOutline(true);
    }

    @VisibleForTesting
    @Nullable AnchoredPopupWindow getPopupWindow() {
        return mMenuWindow;
    }

    @VisibleForTesting
    Delegate getListMenuDelegate(View contentView) {
        return (model, view) -> {
            // Because ListMenuItemAdapter always uses the delegate if there is
            // one, we need to manually call click listeners.
            if (model.containsKey(CLICK_LISTENER) && model.get(CLICK_LISTENER) != null) {
                model.get(CLICK_LISTENER).onClick(contentView);
                return;
            }
            if (model.get(MENU_ITEM_ID) == R.id.name_window) {
                mMultiInstanceManager.showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);
            }
            assumeNonNull(mMenuWindow).dismiss();
        };
    }
}
