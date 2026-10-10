// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.ExpandOnHoverToggleEntryPoint;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

/**
 * Coordinator for the context menu shown when the user right-clicks the vertical tabs rail collapse
 * button. The menu lets the user turn expand-on-hover on or off.
 */
@NullMarked
public class VerticalTabCollapseButtonContextMenuCoordinator {
    private final Activity mActivity;
    private @Nullable Runnable mOnMenuDismissedCallback;
    private @Nullable AnchoredPopupWindow mMenuWindow;

    /**
     * @param activity The {@link Activity} used to build and show the menu.
     * @param onMenuDismissedCallback Invoked when the menu is dismissed.
     */
    VerticalTabCollapseButtonContextMenuCoordinator(
            Activity activity, Runnable onMenuDismissedCallback) {
        mActivity = activity;
        mOnMenuDismissedCallback = onMenuDismissedCallback;
    }

    /**
     * Shows the menu.
     *
     * @param anchorViewRectProvider The {@link RectProvider} to anchor the menu to.
     * @param isIncognito Whether the menu is shown in incognito mode.
     */
    void showMenu(RectProvider anchorViewRectProvider, boolean isIncognito) {
        dismiss();
        View contentView = buildMenuView(isIncognito);
        View decorView = mActivity.getWindow().getDecorView();
        mMenuWindow =
                TabStripContextMenuCoordinator.buildMenuWindow(
                        mActivity, decorView, contentView, anchorViewRectProvider, isIncognito);
        mMenuWindow.addOnDismissListener(
                () -> {
                    if (mOnMenuDismissedCallback != null) {
                        mOnMenuDismissedCallback.run();
                    }
                });
        mMenuWindow.show();
    }

    /** Returns whether the menu is currently showing. */
    boolean isMenuShowing() {
        return mMenuWindow != null && mMenuWindow.isShowing();
    }

    /** Dismisses the menu, if showing. */
    void dismiss() {
        if (mMenuWindow != null) {
            mMenuWindow.dismiss();
        }
    }

    /** Permanently cleans up this component. */
    void destroy() {
        mOnMenuDismissedCallback = null;
        dismiss();
        mMenuWindow = null;
    }

    Delegate getListMenuDelegate() {
        return (model, view) -> {
            if (model.get(MENU_ITEM_ID) == R.id.toggle_expand_tabs_on_hover_menu_id) {
                VerticalTabUtils.setExpandOnHoverEnabledInSharedPref(
                        !VerticalTabUtils.isExpandOnHoverEnabled(),
                        ExpandOnHoverToggleEntryPoint.COLLAPSE_BUTTON_CONTEXT_MENU);
            }
            dismiss();
        };
    }

    /**
     * Builds the checkable "Auto expand tabs on hover" menu item. A checkmark is shown at the end
     * of the item while expand-on-hover is on. Selecting the item is expected to flip the setting.
     *
     * @param context The {@link Context} used to resolve resources.
     * @param isIncognito Whether the menu is shown in incognito mode.
     * @return The menu item.
     */
    public static ListItem buildExpandOnHoverMenuItem(Context context, boolean isIncognito) {
        boolean isChecked = VerticalTabUtils.isExpandOnHoverEnabled();
        ListItem item =
                new ListItemBuilder()
                        .withTitleRes(R.string.expand_tabs_on_hover)
                        .withEndIconRes(isChecked ? R.drawable.material_ic_check_24dp : 0)
                        .withMenuId(R.id.toggle_expand_tabs_on_hover_menu_id)
                        .withIsIncognito(isIncognito)
                        .build();
        // Only reserve space for the checkmark while it is shown, so the menu is narrower when off.
        item.model.set(
                ListMenuItemProperties.END_ICON_MARGIN_START,
                isChecked
                        ? context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.vertical_tabs_expand_on_hover_menu_end_icon_margin)
                        : 0);
        item.model.set(ListMenuItemProperties.CHECKABLE, true);
        item.model.set(ListMenuItemProperties.CHECKED, isChecked);
        return item;
    }

    private View buildMenuView(boolean isIncognito) {
        ModelList modelList = new ModelList();
        modelList.add(buildExpandOnHoverMenuItem(mActivity, isIncognito));
        return TabStripContextMenuCoordinator.inflateMenuContentView(
                mActivity, modelList, contentView -> getListMenuDelegate());
    }

    @Nullable AnchoredPopupWindow getPopupWindowForTesting() {
        return mMenuWindow;
    }
}
