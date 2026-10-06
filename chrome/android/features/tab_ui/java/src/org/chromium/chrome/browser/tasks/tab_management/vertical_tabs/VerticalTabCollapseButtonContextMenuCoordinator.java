// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.app.Activity;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.ExpandOnHoverToggleEntryPoint;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

/**
 * Coordinator for the context menu shown when the user right-clicks the vertical tabs rail collapse
 * button. The menu lets the user turn expand-on-hover on or off.
 */
@NullMarked
class VerticalTabCollapseButtonContextMenuCoordinator {
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
                VerticalTabUtils.setExpandOnHoverEnabled(
                        !VerticalTabUtils.isExpandOnHoverEnabled(),
                        ExpandOnHoverToggleEntryPoint.COLLAPSE_BUTTON_CONTEXT_MENU);
            }
            dismiss();
        };
    }

    private View buildMenuView(boolean isIncognito) {
        ModelList modelList = new ModelList();
        modelList.add(
                new ListItemBuilder()
                        .withTitleRes(
                                VerticalTabUtils.isExpandOnHoverEnabled()
                                        ? R.string.turn_off_expand_tabs_on_hover
                                        : R.string.turn_on_expand_tabs_on_hover)
                        .withMenuId(R.id.toggle_expand_tabs_on_hover_menu_id)
                        .withIsIncognito(isIncognito)
                        .build());
        return TabStripContextMenuCoordinator.inflateMenuContentView(
                mActivity, modelList, contentView -> getListMenuDelegate());
    }

    @Nullable AnchoredPopupWindow getPopupWindowForTesting() {
        return mMenuWindow;
    }
}
