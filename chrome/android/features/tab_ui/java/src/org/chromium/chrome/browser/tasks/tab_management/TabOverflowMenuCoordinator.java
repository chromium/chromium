// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuUtils.createAdapter;
import static org.chromium.ui.listmenu.ListMenuUtils.setupCallbacksRecursively;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.database.DataSetObserver;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.UiUtils;
import org.chromium.ui.listmenu.ListMenuItemAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.Set;

/**
 * A coordinator for the overflow menu for tabs and tab groups. This applies to both the
 * TabGridDialog toolbar and tab group cards on GTS. It is responsible for creating a list of menu
 * items, setting up the menu, and displaying the menu.
 *
 * @param <T> The type of the ID of the overflow menu's origin. For individual tabs, this is a tab
 *     ID. For tab groups, it's the tab group ID.
 */
@NullMarked
public abstract class TabOverflowMenuCoordinator<T> {

    /**
     * Helper interface for handling menu item clicks.
     *
     * @param <T> The type of the ID of the overflow menu's origin. For individual tabs, this is a
     *     tab ID. For tab groups, it's the tab group ID.
     */
    @FunctionalInterface
    public interface OnItemClickedCallback<T> {
        void onClick(
                @IdRes int menuId,
                T id,
                @Nullable String collaborationId,
                @Nullable ListViewTouchTracker listViewTouchTracker);
    }

    private static class OverflowMenuHolder<T> {
        private final Context mContext;
        private final View mContentView;
        private final ComponentCallbacks mComponentCallbacks;
        private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
        private AnchoredPopupWindow mMenuWindow;
        private boolean mSubmenuNavigationInProgress;

        OverflowMenuHolder(
                RectProvider anchorViewRectProvider,
                boolean horizontalOverlapAnchor,
                boolean verticalOverlapAnchor,
                @StyleRes int animStyle,
                @HorizontalOrientation int horizontalOrientation,
                @LayoutRes int menuLayout,
                Drawable menuBackground,
                ModelList modelList,
                OnItemClickedCallback<T> onItemClickedCallback,
                T id,
                @Nullable String collaborationId,
                int popupWidthPx,
                @Nullable Callback<OverflowMenuHolder<T>> onDismiss,
                Activity activity) {
            mContext = new ContextThemeWrapper(activity, R.style.OverflowMenuThemeOverlay);
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

            mContentView = LayoutInflater.from(mContext).inflate(menuLayout, null);

            TouchTrackingListView touchTrackingListView =
                    mContentView.findViewById(R.id.tab_group_action_menu_list);
            ListMenuItemAdapter adapter =
                    createAdapter(
                            modelList,
                            Set.of(),
                            (model) -> {
                                // Because ListMenuItemAdapter always uses the delegate if there is
                                // one, we need to manually call click listeners.
                                if (model.containsKey(CLICK_LISTENER)
                                        && model.get(CLICK_LISTENER) != null) {
                                    // Set mSubmenuNavigationInProgress to prevent the popup from
                                    // being destroyed. It will be cleaned up in the DataSetObserver
                                    // below.
                                    mSubmenuNavigationInProgress = true;
                                    model.get(CLICK_LISTENER).onClick(mContentView);
                                    return;
                                }
                                onItemClickedCallback.onClick(
                                        model.get(MENU_ITEM_ID),
                                        id,
                                        collaborationId,
                                        /* listViewTouchTracker= */ touchTrackingListView);
                                mMenuWindow.dismiss();
                            });
            touchTrackingListView.setItemsCanFocus(true);
            touchTrackingListView.setAdapter(adapter);

            View decorView = activity.getWindow().getDecorView();

            mMenuWindow =
                    new AnchoredPopupWindow(
                            mContext,
                            decorView,
                            menuBackground,
                            mContentView,
                            anchorViewRectProvider);
            mMenuWindow.setFocusable(true);
            mMenuWindow.setHorizontalOverlapAnchor(horizontalOverlapAnchor);
            mMenuWindow.setVerticalOverlapAnchor(verticalOverlapAnchor);
            mMenuWindow.setPreferredHorizontalOrientation(horizontalOrientation);
            mMenuWindow.setElevation(
                    mContentView
                            .getResources()
                            .getDimensionPixelSize(R.dimen.tab_overflow_menu_elevation));
            // Override animation style or animate from anchor as default.
            if (animStyle == Resources.ID_NULL) {
                mMenuWindow.setAnimateFromAnchor(true);
            } else {
                mMenuWindow.setAnimationStyle(animStyle);
            }
            mMenuWindow.setMaxWidth(popupWidthPx);

            // Resize if any new elements are added.
            adapter.registerDataSetObserver(
                    new DataSetObserver() {
                        @Override
                        public void onChanged() {
                            if (mSubmenuNavigationInProgress) {
                                mSubmenuNavigationInProgress = false;
                                return;
                            }
                            resize();
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

        View getContentView() {
            return mContentView;
        }

        void show() {
            mMenuWindow.show();
        }

        void resize() {
            mMenuWindow.onRectChanged();
        }

        void dismiss() {
            mMenuWindow.dismiss();
        }

        void destroy() {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
            // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
            // with a stack trace showing the stack during LifetimeAssert.create().
            LifetimeAssert.destroy(mLifetimeAssert);
        }
    }

    protected final CollaborationService mCollaborationService;
    protected final Supplier<TabModel> mTabModelSupplier;
    protected @Nullable TabGroupSyncService mTabGroupSyncService;

    private final @LayoutRes int mMenuLayout;
    private final @Nullable Context mContext;
    private final OnItemClickedCallback<T> mOnItemClickedCallback;
    private @Nullable OverflowMenuHolder<T> mMenuHolder;

    /**
     * @param menuLayout The menu layout to use.
     * @param onItemClickedCallback A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param collaborationService Used for checking the user is the owner of a group.
     * @param context The {@link Context} that the coordinator resides in.
     */
    protected TabOverflowMenuCoordinator(
            @LayoutRes int menuLayout,
            OnItemClickedCallback<T> onItemClickedCallback,
            Supplier<TabModel> tabModelSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            @Nullable Context context) {
        mMenuLayout = menuLayout;
        mOnItemClickedCallback = onItemClickedCallback;
        mTabModelSupplier = tabModelSupplier;
        mTabGroupSyncService = tabGroupSyncService;
        assert collaborationService != null;
        mCollaborationService = collaborationService;
        mContext = context;
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
     * @param id The ID of the tab or tab group originator.
     */
    protected abstract void buildMenuActionItems(ModelList itemList, T id);

    /**
     * Concrete class required to define what to add for collaborations.
     *
     * @param itemList The {@link ModelList} to populate.
     * @param memberRole The role of the current user in the group.
     */
    protected void buildCollaborationMenuItems(ModelList itemList, @MemberRole int memberRole) {}

    /**
     * A function to run after the menu is created but before it is shown, to make any adjustments.
     */
    protected void afterCreate() {}

    /**
     * Concrete class required to get a specific menu width for the menu pop up window.
     *
     * @param anchorViewWidthPx The width of the anchor view, in px.
     * @return The desired width of the popup, in px.
     */
    protected abstract int getMenuWidth(int anchorViewWidthPx);

    /** Returns the collaborationId relevant for the object with ID {@code id} */
    protected abstract @Nullable String getCollaborationIdOrNull(T id);

    /** Returns menu background drawable. */
    public static Drawable getMenuBackground(Context context, boolean isIncognito) {
        // LINT.IfChange
        final @DrawableRes int bgDrawableId =
                isIncognito ? R.drawable.menu_bg_tinted_on_dark_bg : R.drawable.menu_bg_tinted;

        if (!isIncognito) {
            ColorStateList menuBgColor =
                    ColorStateList.valueOf(SemanticColorUtils.getMenuBgColor(context));
            return UiUtils.getTintedDrawable(context, bgDrawableId, menuBgColor);
        }
        return AppCompatResources.getDrawable(context, bgDrawableId);
        // Lint.ThenChange cannot handle multiline comments.
        // LINT.ThenChange(//components/browser_ui/widget/android/java/res/values/dimens.xml|//components/browser_ui/widget/android/java/res/values-night/dimens.xml)
    }

    private static void offsetPopupRect(Context context, boolean isIncognito, Rect rect) {
        if (isIncognito) return;
        Resources resources = context.getResources();
        rect.offset(0, -resources.getDimensionPixelSize(R.dimen.popup_menu_shadow_length));
        Drawable menuBackground = getMenuBackground(context, isIncognito);
        Rect padding = new Rect();
        menuBackground.getPadding(padding);
        // Subtract off the horizontal padding (for dark mode).
        rect.right -= (padding.left + padding.right);
        // Make up for padding lost above and then additionally add in the shadow padding so the
        // content will be the correct width.
        rect.right += resources.getDimensionPixelSize(R.dimen.popup_menu_shadow_length) * 4;
    }

    // TODO(crbug.com/357878838): Pass the activity through constructor and setup test to test this
    // method
    /**
     * See {@link #createAndShowMenu(RectProvider, Object, boolean, boolean, int, int, Activity)}}
     */
    protected void createAndShowMenu(View anchorView, T id, Activity activity) {
        createAndShowMenu(
                new ViewRectProvider(anchorView),
                id,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ true,
                R.style.EndIconMenuAnim,
                HorizontalOrientation.MAX_AVAILABLE_SPACE,
                activity);
    }

    /**
     * See {@link #createAndShowMenu(RectProvider, Object, boolean, boolean, int, int, Activity,
     * boolean)}.
     *
     * <p>This overload acquires the incognito status from the tab model supplier provided to this
     * class.
     */
    protected void createAndShowMenu(
            RectProvider anchorViewRectProvider,
            T id,
            boolean horizontalOverlapAnchor,
            boolean verticalOverlapAnchor,
            @StyleRes int animStyle,
            @HorizontalOrientation int horizontalOrientation,
            Activity activity) {
        createAndShowMenu(
                anchorViewRectProvider,
                id,
                horizontalOverlapAnchor,
                verticalOverlapAnchor,
                animStyle,
                horizontalOrientation,
                activity,
                /* isIncognito= */ mTabModelSupplier.get().isIncognitoBranded());
    }

    /**
     * Creates a menu view and renders it within an {@link AnchoredPopupWindow}
     *
     * @param anchorViewRectProvider Rect provider for view to anchor the menu.
     * @param id ID of the object the menu needs to be shown for.
     * @param horizontalOverlapAnchor If true, horizontally overlaps menu with the anchor view.
     * @param verticalOverlapAnchor If true, vertically overlaps menu with the anchor view.
     * @param animStyle Animation style to apply for menu show/hide.
     * @param horizontalOrientation {@link HorizontalOrientation} to use for the menu position.
     * @param activity Activity to get resources and decorView for menu.
     * @param isIncognito Whether to theme the overflow menu with incognito colors.
     */
    protected void createAndShowMenu(
            RectProvider anchorViewRectProvider,
            T id,
            boolean horizontalOverlapAnchor,
            boolean verticalOverlapAnchor,
            @StyleRes int animStyle,
            @HorizontalOrientation int horizontalOrientation,
            Activity activity,
            boolean isIncognito) {

        assert mMenuHolder == null;
        @Nullable String collaborationId = getCollaborationIdOrNull(id);
        Drawable menuBackground = getMenuBackground(activity, isIncognito);
        // Initialize the model before creating the adapter so that
        // ListMenuItemAdapter#areAllItemsEnabled returns the correct result instead of receiving an
        // empty model list.
        // If the model list is empty, then areAllItemsEnabled will return true and will not be
        // updated after items are added. Then, keyboard focus will visit all items, including
        // dividers.
        ModelList modelList = new ModelList();
        configureMenuItems(modelList, id);
        // Apply offset from the background.
        if (mContext != null) {
            offsetPopupRect(mContext, isIncognito, anchorViewRectProvider.getRect());
        }
        mMenuHolder =
                new OverflowMenuHolder<>(
                        anchorViewRectProvider,
                        horizontalOverlapAnchor,
                        verticalOverlapAnchor,
                        animStyle,
                        horizontalOrientation,
                        mMenuLayout,
                        menuBackground,
                        modelList,
                        mOnItemClickedCallback,
                        id,
                        collaborationId,
                        getMenuWidth(anchorViewRectProvider.getRect().width()),
                        this::onDismiss,
                        activity);
        buildCustomView(mMenuHolder.getContentView(), isIncognito);
        afterCreate();
        mMenuHolder.show();
    }

    /**
     * Resizes the menu if the menu holder is available. This is used to adjust the menu size when
     * adding collaboration items for {@link TabGroupContextMenuCoordinator}.
     */
    protected void resizeMenu() {
        if (mMenuHolder != null) {
            mMenuHolder.resize();
        }
    }

    /**
     * Dismisses the menu. No-op if the menu holder is {@code null}, and therefore the menu is not
     * already showing.
     */
    public void dismiss() {
        if (mMenuHolder != null) {
            mMenuHolder.dismiss();
        }
    }

    /** Returns true if the menu is currently showing. */
    public boolean isMenuShowing() {
        if (mMenuHolder == null) return false;
        return mMenuHolder.mMenuWindow.isShowing();
    }

    protected void onMenuDismissed() {}

    protected @Nullable TabModel getTabModel() {
        return mTabModelSupplier.get();
    }

    /**
     * @return The DP measure {@param dimenRes}, converted to px.
     */
    protected int getDimensionPixelSize(@DimenRes int dimenRes) {
        assert mContext != null : "context needs to be non-null to get pixel size";
        return mContext.getResources().getDimensionPixelSize(dimenRes);
    }

    private void onDismiss(OverflowMenuHolder<T> menuHolder) {
        assert mMenuHolder == menuHolder;
        mMenuHolder = null;
        onMenuDismissed();
    }

    private void configureMenuItems(ModelList modelList, T id) {
        @Nullable String collaborationId = getCollaborationIdOrNull(id);
        boolean hasCollaborationData =
                TabShareUtils.isCollaborationIdValid(collaborationId)
                        && mCollaborationService.getServiceStatus().isAllowedToJoin();
        buildMenuActionItems(modelList, id);
        if (hasCollaborationData) {
            buildCollaborationMenuItems(
                    modelList, mCollaborationService.getCurrentUserRoleForGroup(collaborationId));
        }
        // Set up callbacks for submenu navigation
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                modelList,
                () -> {
                    if (mMenuHolder != null) {
                        mMenuHolder.dismiss();
                    }
                });
    }

    public void configureMenuItemsForTesting(ModelList modelList, T id) {
        configureMenuItems(modelList, id);
    }

    public void destroyMenuForTesting() {
        // This is needed because mMenuHolder#destroy is usually called as an onDismissListener.
        // However, in Robolectric tests, the onDismissListener may not be called, so the menu won't
        // be destroyed, and the test will report a lifecycle error.
        if (mMenuHolder != null) mMenuHolder.destroy();
    }

    /**
     * Changes the focusability of the menu.
     *
     * @param focusable True if the menu is focusable, false otherwise.
     */
    public void setMenuFocusable(boolean focusable) {
        if (mMenuHolder != null) {
            mMenuHolder.mMenuWindow.setFocusable(focusable);
        }
    }
}
