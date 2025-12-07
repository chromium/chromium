// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE_ID;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.PluralsRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.UiUtils;
import org.chromium.ui.hierarchicalmenu.FlyoutController;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.AccessibilityListObserver;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * A coordinator for the overflow menu for tabs and tab groups. This applies to both the
 * TabGridDialog toolbar and tab group cards on GTS. It is responsible for creating a list of menu
 * items, setting up the menu, and displaying the menu.
 *
 * @param <T> The type of the ID of the overflow menu's origin. For individual tabs, this is a tab
 *     ID. For tab groups, it's the tab group ID.
 */
@NullMarked
public abstract class TabOverflowMenuCoordinator<T>
        implements FlyoutHandler<TabOverflowMenuHolder<T>> {

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

    protected final CollaborationService mCollaborationService;
    protected final Supplier<TabModel> mTabModelSupplier;
    protected final @Nullable MultiInstanceManager mMultiInstanceManager;
    protected @Nullable TabGroupSyncService mTabGroupSyncService;

    private final Activity mActivity;
    private final @LayoutRes int mMenuLayout;
    private final @LayoutRes int mFlyoutMenuLayout;
    private final OnItemClickedCallback<T> mOnItemClickedCallback;
    private final HierarchicalMenuController mHierarchicalMenuController;

    private boolean mIsIncognito;
    private @Nullable String mCollaborationId;
    private @Nullable T mId;

    /**
     * @param menuLayout The menu layout to use.
     * @param flyoutMenuLayout The menu layout for flyout popups to use.
     * @param onItemClickedCallback A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param multiInstanceManager The {@link MultiInstanceManager}.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param collaborationService Used for checking the user is the owner of a group.
     * @param activity The {@link Activity} that the coordinator resides in.
     */
    protected TabOverflowMenuCoordinator(
            @LayoutRes int menuLayout,
            @LayoutRes int flyoutMenuLayout,
            OnItemClickedCallback<T> onItemClickedCallback,
            Supplier<TabModel> tabModelSupplier,
            @Nullable MultiInstanceManager multiInstanceManager,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            Activity activity) {
        mMenuLayout = menuLayout;
        mFlyoutMenuLayout = flyoutMenuLayout;
        mOnItemClickedCallback = onItemClickedCallback;
        mTabModelSupplier = tabModelSupplier;
        mMultiInstanceManager = multiInstanceManager;
        mTabGroupSyncService = tabGroupSyncService;
        assert collaborationService != null;
        mCollaborationService = collaborationService;
        mActivity = activity;
        mHierarchicalMenuController = ListMenuUtils.createHierarchicalMenuController(activity);
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
        mCollaborationId = getCollaborationIdOrNull(id);
        mIsIncognito = isIncognito;
        mId = id;

        // Initialize the model before creating the adapter so that
        // ListMenuItemAdapter#areAllItemsEnabled returns the correct result instead of receiving an
        // empty model list.
        // If the model list is empty, then areAllItemsEnabled will return true and will not be
        // updated after items are added. Then, keyboard focus will visit all items, including
        // dividers.
        ModelList modelList = new ModelList();
        configureMenuItems(modelList, id);
        // Apply offset from the background.
        if (mActivity != null) {
            offsetPopupRect(mActivity, isIncognito, anchorViewRectProvider.getRect());
        }
        TabOverflowMenuHolder<T> menuHolder =
                new TabOverflowMenuHolder<>(
                        anchorViewRectProvider,
                        horizontalOverlapAnchor,
                        verticalOverlapAnchor,
                        animStyle,
                        horizontalOrientation,
                        mMenuLayout,
                        getMenuBackground(activity, mIsIncognito),
                        modelList,
                        mOnItemClickedCallback,
                        id,
                        mCollaborationId,
                        getMenuWidth(anchorViewRectProvider.getRect().width()),
                        this::onDismiss,
                        activity,
                        /* isFlyout= */ false);
        buildCustomView(menuHolder.getContentView(), isIncognito);
        afterCreate();

        modelList.addObserver(
                mHierarchicalMenuController
                .new AccessibilityListObserver(
                        menuHolder.getContentView(),
                        /* headerView= */ null,
                        menuHolder.getContentView().findViewById(R.id.tab_group_action_menu_list),
                        /* headerModelList= */ null,
                        modelList));

        menuHolder.show();

        mHierarchicalMenuController.setupFlyoutController(
                /* flyoutHandler= */ this, menuHolder, /* drillDownOverrideValue= */ null);
    }

    /**
     * Resizes the menu if the menu holder is available. This is used to adjust the menu size when
     * adding collaboration items for {@link TabGroupContextMenuCoordinator}.
     */
    protected void resizeMenu() {
        FlyoutController<TabOverflowMenuHolder> controller =
                mHierarchicalMenuController.getFlyoutController();
        if (controller != null) {
            controller.getMainPopup().resize();
        }
    }

    /**
     * Dismisses the menu. No-op if the menu holder is {@code null}, and therefore the menu is not
     * already showing.
     */
    public void dismiss() {
        if (mHierarchicalMenuController.getFlyoutController() != null) {
            mHierarchicalMenuController.destroyFlyoutController();
        }
    }

    /** Returns true if the menu is currently showing. */
    public boolean isMenuShowing() {
        FlyoutController<TabOverflowMenuHolder> controller =
                mHierarchicalMenuController.getFlyoutController();
        if (controller == null) {
            return false;
        }

        return controller.getMainPopup().getMenuWindow().isShowing();
    }

    protected void onMenuDismissed() {}

    protected @Nullable TabModel getTabModel() {
        return mTabModelSupplier.get();
    }

    /**
     * @return The DP measure {@param dimenRes}, converted to px.
     */
    protected int getDimensionPixelSize(@DimenRes int dimenRes) {
        assert mActivity != null : "Activity needs to be non-null to get pixel size";
        return mActivity.getResources().getDimensionPixelSize(dimenRes);
    }

    private void onDismiss(TabOverflowMenuHolder<T> menuHolder) {
        if (mHierarchicalMenuController.getFlyoutController() != null) {
            mHierarchicalMenuController.destroyFlyoutController();
        }

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
        // Set up callbacks for submenu navigation.
        mHierarchicalMenuController.setupCallbacksRecursively(
                /* headerModelList= */ null,
                modelList,
                () -> {
                    dismiss();
                });
    }

    public void configureMenuItemsForTesting(ModelList modelList, T id) {
        configureMenuItems(modelList, id);
    }

    public void destroyMenuForTesting() {
        // This is needed because mMenuHolder#destroy is usually called as an onDismissListener.
        // However, in Robolectric tests, the onDismissListener may not be called, so the menu won't
        // be destroyed, and the test will report a lifecycle error.
        FlyoutController<TabOverflowMenuHolder<T>> controller =
                mHierarchicalMenuController.getFlyoutController();
        if (controller == null) {
            return;
        }

        controller.destroy();
        controller = null;
    }

    /**
     * Changes the focusability of the menu.
     *
     * @param focusable True if the menu is focusable, false otherwise.
     */
    public void setMenuFocusable(boolean focusable) {
        FlyoutController<TabOverflowMenuHolder> controller =
                mHierarchicalMenuController.getFlyoutController();
        if (controller != null) {
            controller.getMainPopup().getMenuWindow().setFocusable(focusable);
        }
    }

    public @Nullable ModelList getModelListForTesting() {
        FlyoutController<TabOverflowMenuHolder> controller =
                mHierarchicalMenuController.getFlyoutController();
        if (controller == null) {
            return null;
        }

        return controller.getMainPopup().getModelList();
    }

    public @Nullable View getContentViewForTesting() {
        FlyoutController<TabOverflowMenuHolder> controller =
                mHierarchicalMenuController.getFlyoutController();
        if (controller == null) {
            return null;
        }

        return controller.getMainPopup().getContentView();
    }

    /**
     * Create a {@link ListItem} that opens a submenu to choose a window to move to.
     *
     * @param id The identifier of the tab or group to move, of type {@code T}.
     * @param isIncognito Whether we are in incognito mode.
     * @param pluralsRes The pluralizable string resource to move item(s) to another window.
     * @param menuId The menu ID to use when clicking.
     * @return The {@link ListItem} letting a user choose a window to move to.
     */
    @RequiresNonNull("mMultiInstanceManager")
    protected ListItem createMoveToWindowItem(
            T id, boolean isIncognito, @PluralsRes int pluralsRes, @IdRes int menuId) {
        // TODO(crbug.com/437418051): Clean up move_tab_to_another_window strings.
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)) {
            return new ListItemBuilder()
                    .withTitle(
                            mActivity
                                    .getResources()
                                    .getQuantityString(
                                            pluralsRes,
                                            MultiWindowUtils.getInstanceCountWithFallback(
                                                    PersistedInstanceType.ACTIVE)))
                    .withMenuId(menuId)
                    .withIsIncognito(isIncognito)
                    .build();
        }
        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE_ID, R.string.menu_new_window)
                                .with(ENABLED, true)
                                .with(
                                        CLICK_LISTENER,
                                        v -> {
                                            moveToNewWindow(id);
                                        })
                                .build()));
        List<InstanceInfo> activeInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ACTIVE);
        if (activeInstances.size() > 1) {
            submenuItems.add(buildMenuDivider(isIncognito));
            for (InstanceInfo instanceInfo : activeInstances) {
                if (mMultiInstanceManager.getCurrentInstanceId() == instanceInfo.instanceId) {
                    continue;
                }
                submenuItems.add(
                        new ListItem(
                                MENU_ITEM,
                                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                        .with(TITLE, instanceInfo.title)
                                        .with(
                                                CLICK_LISTENER,
                                                (v) -> {
                                                    moveToWindow(instanceInfo, id);
                                                })
                                        .with(ENABLED, true)
                                        .build()));
            }
        }
        return new ListItem(
                MENU_ITEM_WITH_SUBMENU,
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(
                                TITLE,
                                mActivity
                                        .getResources()
                                        .getQuantityString(pluralsRes, 2)) // Any # > 1
                        .with(SUBMENU_ITEMS, submenuItems)
                        .with(ENABLED, true)
                        .build());
    }

    /** Creates a new window and moves item with ID {@param id} to it. */
    @RequiresNonNull("mMultiInstanceManager")
    protected void moveToNewWindow(T id) {}

    /** Moves item with ID {@param id} to window with instance info {@param instanceInfo}. */
    @RequiresNonNull("mMultiInstanceManager")
    protected void moveToWindow(InstanceInfo instanceInfo, T id) {}

    @Override
    public Rect getPopupRect(TabOverflowMenuHolder<T> popupWindow) {
        View contentView = popupWindow.getContentView();

        if (contentView == null) {
            return new Rect();
        }

        return ListMenuUtils.getViewRectRelativeToItsRootView(contentView);
    }

    @Override
    public void dismissPopup(TabOverflowMenuHolder<T> popupWindow) {
        popupWindow.dismiss();
    }

    @Override
    public void setWindowFocus(TabOverflowMenuHolder<T> popupWindow, boolean hasFocus) {
        ViewGroup contentView = (ViewGroup) popupWindow.getMenuWindow().getContentView();
        if (contentView == null) {
            return;
        }
        contentView.setFocusable(true);

        HierarchicalMenuController.setWindowFocusForFlyoutMenus(contentView, hasFocus);
    }

    @Override
    public TabOverflowMenuHolder<T> createAndShowFlyoutPopup(
            ListItem item, View view, Runnable dismissRunnable) {
        ModelList modelList = getModelListSubtree(item);

        Rect anchorRect =
                FlyoutController.calculateFlyoutAnchorRect(
                        view, mActivity.getWindow().getDecorView());

        if (!mIsIncognito) {
            anchorRect.offset(
                    0,
                    -mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.popup_menu_shadow_length));
        }

        RectProvider rectProvider = new RectProvider(anchorRect);

        assert mId != null;
        TabOverflowMenuHolder<T> menuHolder =
                new TabOverflowMenuHolder<>(
                        rectProvider,
                        /* horizontalOverlapAnchor= */ false,
                        /* verticalOverlapAnchor= */ true,
                        Resources.ID_NULL,
                        HorizontalOrientation.MAX_AVAILABLE_SPACE,
                        mFlyoutMenuLayout,
                        getMenuBackground(mActivity, mIsIncognito),
                        modelList,
                        mOnItemClickedCallback,
                        mId,
                        mCollaborationId,
                        getMenuWidth(rectProvider.getRect().width()),
                        (holder) -> dismissRunnable.run(),
                        mActivity,
                        /* isFlyout= */ true);

        menuHolder.show();
        return menuHolder;
    }

    private static ModelList getModelListSubtree(ListItem item) {
        ModelList modelList = new ModelList();
        for (ListItem listItem : item.model.get(ListMenuSubmenuItemProperties.SUBMENU_ITEMS)) {
            modelList.add(listItem);
        }
        return modelList;
    }
}
