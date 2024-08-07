// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.database.DataSetObserver;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuItemViewBinder;
import org.chromium.ui.listmenu.ListSectionDividerViewBinder;
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

    private static class OverflowMenuHolder {
        private static final int INVALID_ITEM_ID = -1;
        private final Context mContext;
        private final View mContentView;
        private final ModelList mModelList = new ModelList();
        private final ComponentCallbacks mComponentCallbacks;
        private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
        private AnchoredPopupWindow mMenuWindow;

        OverflowMenuHolder(
                View anchorView,
                @LayoutRes int menuLayout,
                OnItemClickedCallback onItemClickedCallback,
                boolean isIncognito,
                int tabId,
                @DimenRes int popupWidthRes,
                @Nullable Callback<OverflowMenuHolder> onDismiss,
                Activity activity) {
            Context context = anchorView.getContext();
            mContext = context;
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
            context.registerComponentCallbacks(mComponentCallbacks);

            mContentView = LayoutInflater.from(context).inflate(menuLayout, null);

            ListView listView = mContentView.findViewById(R.id.tab_group_action_menu_list);
            ModelListAdapter adapter =
                    new ModelListAdapter(mModelList) {
                        @Override
                        public long getItemId(int position) {
                            ListItem item = (ListItem) getItem(position);
                            if (getItemViewType(position) == ListMenuItemType.MENU_ITEM) {
                                return item.model.get(ListMenuItemProperties.MENU_ITEM_ID);
                            } else {
                                return INVALID_ITEM_ID;
                            }
                        }
                    };
            adapter.registerType(
                    ListMenuItemType.MENU_ITEM,
                    new LayoutViewBuilder(R.layout.list_menu_item),
                    ListMenuItemViewBinder::binder);
            adapter.registerType(
                    ListMenuItemType.DIVIDER,
                    new LayoutViewBuilder(R.layout.list_section_divider),
                    ListSectionDividerViewBinder::bind);
            listView.setAdapter(adapter);
            listView.setOnItemClickListener(
                    (p, v, pos, id) -> {
                        onItemClickedCallback.onClick((int) id, tabId);
                        mMenuWindow.dismiss();
                    });

            View decorView = activity.getWindow().getDecorView();
            ViewRectProvider rectProvider = new ViewRectProvider(anchorView);

            final @DrawableRes int bgDrawableId =
                    isIncognito ? R.drawable.menu_bg_tinted_on_dark_bg : R.drawable.menu_bg_tinted;

            mMenuWindow =
                    new AnchoredPopupWindow(
                            context,
                            decorView,
                            AppCompatResources.getDrawable(context, bgDrawableId),
                            mContentView,
                            rectProvider);
            mMenuWindow.setFocusable(true);
            mMenuWindow.setHorizontalOverlapAnchor(true);
            mMenuWindow.setVerticalOverlapAnchor(true);
            mMenuWindow.setAnimationStyle(R.style.EndIconMenuAnim);
            int popupWidth = context.getResources().getDimensionPixelSize(popupWidthRes);
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
            mMenuWindow.addOnDismissListener(
                    () -> {
                        if (onDismiss != null) {
                            onDismiss.onResult(this);
                        }
                        destroy();
                    });
        }

        ModelList getModelList() {
            return mModelList;
        }

        View getContentView() {
            return mContentView;
        }

        void show() {
            mMenuWindow.show();
        }

        void dismiss() {
            mMenuWindow.dismiss();
        }

        void destroy() {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
            // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
            // with a stack trace showing the stack during LifetimeAssert.create().
            LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        }
    }

    private final @LayoutRes int mMenuLayout;
    private final OnItemClickedCallback mOnItemClickedCallback;
    private final Supplier<TabModel> mTabModelSupplier;
    private final boolean mIsTabGroupSyncEnabled;
    private final @Nullable IdentityManager mIdentityManager;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final @Nullable DataSharingService mDataSharingService;
    private @Nullable OverflowMenuHolder mMenuHolder;

    /**
     * @param menuLayout The menu layout to use.
     * @param onItemClickedCallback A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param isTabGroupSyncEnabled Whether to tab group sync is enabled.
     * @param identityManager Used for checking the current account.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param dataSharingService Used for checking the user is the owner of a group.
     */
    protected TabGroupOverflowMenuCoordinator(
            @LayoutRes int menuLayout,
            OnItemClickedCallback onItemClickedCallback,
            Supplier<TabModel> tabModelSupplier,
            boolean isTabGroupSyncEnabled,
            @Nullable IdentityManager identityManager,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @Nullable DataSharingService dataSharingService) {
        mMenuLayout = menuLayout;
        mOnItemClickedCallback = onItemClickedCallback;
        mTabModelSupplier = tabModelSupplier;
        mIsTabGroupSyncEnabled = isTabGroupSyncEnabled;
        mIdentityManager = identityManager;
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
    }

    /**
     * Implemented in {@link TabGroupContextMenuCoordinator} to initialize the custom view for the
     * tab group context menu. This method inflates necessary components, including the color picker
     * and group title text.
     *
     * @param contentView The root view of the content where the custom view will be initialized.
     * @param isIncognito Whether the current tab model is incognito or not.
     */
    protected void buildCustomView(View contentView, boolean isIncognito) {}

    /**
     * Concrete class required to define what the ModelList for the menu contains.
     *
     * @param itemList The {@link ModelList} to populate.
     * @param isIncognito Whether the current tab model is incognito or not.
     * @param isTabGroupSyncEnabled Whether to tab group sync is enabled.
     * @param hasCollaborationData Whether the menu will call buildCollaborationMenuItems after.
     */
    protected abstract void buildMenuActionItems(
            ModelList itemList,
            boolean isIncognito,
            boolean isTabGroupSyncEnabled,
            boolean hasCollaborationData);

    /**
     * Concrete class required to define what to add for collaborations.
     *
     * @param itemList The {@link ModelList} to populate.
     * @param identityManager Used for checking the current account.
     * @param outcome The outcome of fetching collaboration data.
     */
    protected abstract void buildCollaborationMenuItems(
            ModelList itemList, IdentityManager identityManager, GroupDataOrFailureOutcome outcome);

    /** Concrete class required to get a specific menu width for the menu pop up window. */
    protected abstract @DimenRes int getMenuWidth();

    protected void createAndShowMenu(View anchorView, int tabId, @NonNull Activity activity) {
        assert mMenuHolder == null;
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();
        mMenuHolder =
                new OverflowMenuHolder(
                        anchorView,
                        mMenuLayout,
                        mOnItemClickedCallback,
                        isIncognito,
                        tabId,
                        getMenuWidth(),
                        this::onDismiss,
                        activity);
        buildCustomView(mMenuHolder.getContentView(), isIncognito);
        configureMenuItems(mMenuHolder.getModelList(), isIncognito, tabId);
        mMenuHolder.show();
    }

    private void onDismiss(OverflowMenuHolder menuHolder) {
        assert mMenuHolder == menuHolder;
        mMenuHolder = null;
    }

    private void configureMenuItems(ModelList modelList, boolean isIncognito, int tabId) {
        @Nullable String collaborationId = getCollaborationIdOrNull(tabId);
        boolean hasCollaborationData =
                !TextUtils.isEmpty(collaborationId)
                        && mIdentityManager != null
                        && mDataSharingService != null;
        buildMenuActionItems(modelList, isIncognito, mIsTabGroupSyncEnabled, hasCollaborationData);
        if (hasCollaborationData) {
            mDataSharingService.readGroup(
                    collaborationId,
                    (outcome) -> buildCollaborationMenuItems(modelList, mIdentityManager, outcome));
        }
    }

    private @Nullable String getCollaborationIdOrNull(int tabId) {
        if (mTabModelSupplier == null || mTabGroupSyncService == null) {
            return null;
        } else {
            return TabShareUtils.getCollaborationIdOrNull(
                    tabId, mTabModelSupplier.get(), mTabGroupSyncService);
        }
    }

    void dismissForTesting() {
        mMenuHolder.dismiss();
    }
}
