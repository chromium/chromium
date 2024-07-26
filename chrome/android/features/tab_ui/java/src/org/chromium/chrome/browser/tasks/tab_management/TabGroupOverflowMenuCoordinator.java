// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.database.DataSetObserver;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.LifetimeAssert;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuItemViewBinder;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * A coordinator for the overflow menu in tab groups. This applies to both the TabGridDialog toolbar
 * and tab group cards on GTS. It is responsible for creating a list of menu items, setting up the
 * menu and displaying the menu.
 */
public abstract class TabGroupOverflowMenuCoordinator {
    /** Helper interface for handling menu item clicks for tab group related actions. */
    @FunctionalInterface
    public interface OnItemClickedCallback {
        void onClick(@IdRes int menuId, int tabId);
    }

    private final Context mContext;
    protected final OnItemClickedCallback mOnItemClickedCallback;
    protected final int mTabId;
    private final ComponentCallbacks mComponentCallbacks;
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private AnchoredPopupWindow mMenuWindow;

    /**
     * @param context The app context.
     * @param anchorView The view to anchor the UI on.
     * @param onItemClickedCallback A callback for listening to clicks.
     * @param tabId The tab ID for the tab or a tab ID from the group being acted on.
     * @param isIncognito Whether the tab/group incognito.
     * @param shouldShowDeleteGroup Whether to show the delete group option.
     */
    public TabGroupOverflowMenuCoordinator(
            Context context,
            View anchorView,
            OnItemClickedCallback onItemClickedCallback,
            int tabId,
            boolean isIncognito,
            boolean shouldShowDeleteGroup) {
        mContext = context;
        mOnItemClickedCallback = onItemClickedCallback;
        mTabId = tabId;
        mComponentCallbacks =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration newConfig) {
                        if (mMenuWindow == null || !mMenuWindow.isShowing()) return;
                        mMenuWindow.dismiss();
                    }

                    @Override
                    public void onLowMemory() {}
                };
        mContext.registerComponentCallbacks(mComponentCallbacks);

        final View contentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.tab_switcher_action_menu_layout, null);
        setupMenu(contentView, anchorView, isIncognito, shouldShowDeleteGroup);
    }

    private void setupMenu(
            View contentView, View anchorView, boolean isIncognito, boolean shouldShowDeleteGroup) {
        ListView listView = contentView.findViewById(R.id.tab_switcher_action_menu_list);
        ModelList modelList = buildMenuItems(isIncognito, shouldShowDeleteGroup);

        ModelListAdapter adapter =
                new ModelListAdapter(modelList) {
                    @Override
                    public long getItemId(int position) {
                        ListItem item = (ListItem) getItem(position);
                        return item.model.get(ListMenuItemProperties.MENU_ITEM_ID);
                    }
                };
        listView.setAdapter(adapter);
        adapter.registerType(
                ListMenuItemType.MENU_ITEM,
                new LayoutViewBuilder(R.layout.list_menu_item),
                ListMenuItemViewBinder::binder);
        listView.setOnItemClickListener(
                (p, v, pos, id) -> {
                    mOnItemClickedCallback.onClick((int) id, mTabId);
                    mMenuWindow.dismiss();
                });

        View decorView = ((Activity) contentView.getContext()).getWindow().getDecorView();
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);

        final @DrawableRes int bgDrawableId =
                isIncognito ? R.drawable.menu_bg_tinted_on_dark_bg : R.drawable.menu_bg_tinted;

        mMenuWindow =
                new AnchoredPopupWindow(
                        mContext,
                        decorView,
                        AppCompatResources.getDrawable(mContext, bgDrawableId),
                        contentView,
                        rectProvider);
        mMenuWindow.setFocusable(true);
        mMenuWindow.setHorizontalOverlapAnchor(true);
        mMenuWindow.setVerticalOverlapAnchor(true);
        mMenuWindow.setAnimationStyle(R.style.EndIconMenuAnim);
        @DimenRes int popupWidthRes = getMenuWidth();
        int popupWidth = mContext.getResources().getDimensionPixelSize(popupWidthRes);
        mMenuWindow.setMaxWidth(popupWidth);

        // Resize if any new elements are added.
        adapter.registerDataSetObserver(
                new DataSetObserver() {
                    @Override
                    public void onChanged() {
                        mMenuWindow.onRectChanged();
                    }
                });

        // When the menu is dismissed, call destroy to unregister the orientation listener.
        mMenuWindow.addOnDismissListener(this::destroy);
    }

    /**
     * Concrete class required to define what the ModelList for the menu contains.
     *
     * @param isIncognito Whether the current tab model is incognito or not.
     * @param shouldShowDeleteGroup Whether to show the delete group option.
     */
    protected abstract ModelList buildMenuItems(boolean isIncognito, boolean shouldShowDeleteGroup);

    /** Concrete class required to get a specific menu width for the menu pop up window. */
    protected abstract @DimenRes int getMenuWidth();

    protected void display() {
        if (mMenuWindow == null) return;

        mMenuWindow.show();
    }

    @VisibleForTesting
    public void destroy() {
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }
}
