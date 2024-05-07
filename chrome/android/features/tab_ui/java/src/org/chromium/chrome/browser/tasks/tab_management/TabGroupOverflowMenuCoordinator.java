// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
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
    private final Context mContext;
    protected final @Nullable Callback<Integer> mOnItemClickedGridDialogCallback;
    protected final @Nullable TabListGroupMenuCoordinator.OnItemClickedCallback
            mOnItemClickedListGroupCallback;
    protected final @Nullable Integer mTabId;
    protected final boolean mShouldShowDeleteGroup;
    private final ComponentCallbacks mComponentCallbacks;
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private AnchoredPopupWindow mMenuWindow;

    public TabGroupOverflowMenuCoordinator(
            Context context,
            View anchorView,
            @Nullable Callback<Integer> onItemClickedGridDialog,
            @Nullable TabListGroupMenuCoordinator.OnItemClickedCallback onItemClickedListGroup,
            @Nullable Integer tabId,
            boolean isIncognito,
            boolean shouldShowDeleteGroup) {
        mContext = context;
        mOnItemClickedGridDialogCallback = onItemClickedGridDialog;
        mOnItemClickedListGroupCallback = onItemClickedListGroup;
        mTabId = tabId;
        mShouldShowDeleteGroup = shouldShowDeleteGroup;
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
        setupMenu(contentView, anchorView, isIncognito);
    }

    private void setupMenu(View contentView, View anchorView, boolean isIncognito) {
        ListView listView = contentView.findViewById(R.id.tab_switcher_action_menu_list);
        ModelList modelList = buildMenuItems(isIncognito);

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
                    runCallback((int) id);
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

        // When the menu is dismissed, call destroy to unregister the orientation listener.
        mMenuWindow.addOnDismissListener(this::destroy);
    }

    /**
     * Concrete class required to define what the ModelList for the menu contains.
     *
     * @param isIncognito Whether the current tab model is incognito or not.
     */
    protected abstract ModelList buildMenuItems(boolean isIncognito);

    /**
     * Concrete class required to run a specific callback on a menu item clicked.
     *
     * @param isIncognito Whether the current tab model is incognito or not.
     */
    protected abstract void runCallback(int menuId);

    /** Concrete class required to get a specific menu width for the menu pop up window. */
    protected abstract @DimenRes int getMenuWidth();

    protected void display() {
        if (mMenuWindow == null) return;

        mMenuWindow.show();
    }

    private void destroy() {
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }
}
