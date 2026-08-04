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
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkAllTabsHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.TabStripLayoutType;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripMenuMetricsUtils.StripMenuAction;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.RecentlyClosedEntryType;
import org.chromium.chrome.browser.task_manager.TaskManager;
import org.chromium.chrome.browser.task_manager.TaskManagerFactory;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.LayoutSwitchEntryPoint;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.listmenu.ListMenuItemAdapter;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.Set;
import java.util.function.BooleanSupplier;

/**
 * Coordinator for the context menu on the tab strip. It is responsible for creating a list of menu
 * items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class TabStripContextMenuCoordinator {
    @VisibleForTesting static final String FEEDBACK_CATEGORY_SUFFIX = ".tabstrip";

    private final Context mContext;
    private final TabModel mTabModel;
    private final MultiInstanceManager mMultiInstanceManager;
    private final WindowAndroid mWindowAndroid;
    private final SnackbarManager mSnackbarManager;
    private final Runnable mOnNewTabClick;
    private final @Nullable BooleanSupplier mCanActivateTabLayoutToggleMenuSupplier;
    private final @TabStripLayoutType int mTabStripLayout;
    private @Nullable AnchoredPopupWindow mMenuWindow;

    /**
     * Creates the TabStripContextMenuCoordinator object.
     *
     * @param tabModel The {@link TabModel} to act on.
     * @param multiInstanceManager The {@link MultiInstanceManager} to manage windows.
     * @param windowAndroid The {@link WindowAndroid} current window.
     * @param snackbarManager The {@link SnackbarManager} used to show snackbar UI.
     * @param onNewTabClick Runnable executed on new tab button click.
     * @param canActivateTabLayoutToggleMenuSupplier Supplies whether tab layout toggle menu can be
     *     activated.
     * @param tabStripLayout The active {@link TabStripLayoutType}.
     */
    public static TabStripContextMenuCoordinator createContextMenuCoordinator(
            TabModel tabModel,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid,
            SnackbarManager snackbarManager,
            Runnable onNewTabClick,
            @Nullable BooleanSupplier canActivateTabLayoutToggleMenuSupplier,
            @TabStripLayoutType int tabStripLayout) {
        return new TabStripContextMenuCoordinator(
                tabModel,
                multiInstanceManager,
                windowAndroid,
                snackbarManager,
                onNewTabClick,
                canActivateTabLayoutToggleMenuSupplier,
                tabStripLayout);
    }

    private TabStripContextMenuCoordinator(
            TabModel tabModel,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid,
            SnackbarManager snackbarManager,
            Runnable onNewTabClick,
            @Nullable BooleanSupplier canActivateTabLayoutToggleMenuSupplier,
            @TabStripLayoutType int tabStripLayout) {
        mTabModel = tabModel;
        mMultiInstanceManager = multiInstanceManager;
        mWindowAndroid = windowAndroid;
        mContext = assumeNonNull(windowAndroid.getActivity().get());
        mSnackbarManager = snackbarManager;
        mOnNewTabClick = onNewTabClick;
        mCanActivateTabLayoutToggleMenuSupplier = canActivateTabLayoutToggleMenuSupplier;
        mTabStripLayout = tabStripLayout;
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
        if (modelList.isEmpty()) return;

        Drawable background = TabOverflowMenuCoordinator.getMenuBackground(mContext, isIncognito);

        // TODO (crbug.com/436283175): Update the name of this resource for generic use.
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.tab_switcher_action_menu_layout, null);
        ListMenuUtils.clipContentViewOutline(contentView, R.attr.popupBgCornerRadius);

        // TODO (crbug.com/436283175): Update the name of this resource for generic use.
        TouchTrackingListView touchTrackingListView =
                contentView.findViewById(R.id.tab_group_action_menu_list);
        ListMenuItemAdapter adapter =
                createAdapter(modelList, Set.of(), getListMenuDelegate(contentView));
        touchTrackingListView.setItemsCanFocus(true);
        touchTrackingListView.setAdapter(adapter);

        View decorView = activity.getWindow().getDecorView();

        // Similar to Chrome Desktop (W/M/L), compute the translated strings' width
        // dynamically, clamp the value between a preselected
        // tab_strip_context_menu_(min_width/max_width), and apply the result as
        // the DesiredContentWidth. This ensures that the each context menu item is
        // always one line long, and does not wrap to 2 or more lines for long strings.
        int[] contentDimensions =
                UiUtils.computeListAdapterContentDimensions(adapter, touchTrackingListView);
        int minWidthPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width);
        int maxWidthPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width);
        var popupWidthPx =
                MathUtils.clamp(
                        Math.max(anchorViewRectProvider.getRect().width(), contentDimensions[0]),
                        minWidthPx,
                        maxWidthPx);

        AnchoredPopupWindow.Builder builder =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                decorView,
                                background,
                                () -> contentView,
                                anchorViewRectProvider)
                        .setFocusable(true)
                        .setOutsideTouchable(true)
                        .setHorizontalOverlapAnchor(true)
                        .setVerticalOverlapAnchor(true)
                        .setAllowOverlapCaptionBar(true)
                        .setPreferredHorizontalOrientation(HorizontalOrientation.LAYOUT_DIRECTION)
                        .setMaxWidth(maxWidthPx)
                        .setDesiredContentWidth(popupWidthPx)
                        .setAllowNonTouchableSize(true)
                        .setElevation(
                                contentView
                                        .getResources()
                                        .getDimension(R.dimen.tab_overflow_menu_elevation))
                        .setAnimateFromAnchor(true);
        mMenuWindow = builder.build();
        mMenuWindow.show();
    }

    private void configureMenuItems(ModelList itemList, boolean isIncognito) {
        // Add "New tab" option.
        itemList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.menu_new_tab)
                        .withMenuId(R.id.new_tab_menu_id)
                        .withIsIncognito(isIncognito)
                        .build());
        // Add "Reopen closed tab/tabs/group" option.
        @RecentlyClosedEntryType
        int recentlyClosedEntryType = mTabModel.getMostRecentlyClosedEntryType();
        if (recentlyClosedEntryType != RecentlyClosedEntryType.NONE) {
            int titleRes = R.string.menu_reopen_closed_tab;
            if (recentlyClosedEntryType == RecentlyClosedEntryType.TABS) {
                titleRes = R.string.menu_reopen_closed_tabs;
            } else if (recentlyClosedEntryType == RecentlyClosedEntryType.GROUP) {
                titleRes = R.string.menu_reopen_closed_group;
            }
            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(titleRes)
                            .withMenuId(R.id.reopen_closed_entry)
                            .withIsIncognito(false)
                            .build());
        }
        // Add "Bookmark all tabs" option.
        if (!isIncognito && mTabModel.getCount() > 1) {
            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.menu_bookmark_all_tabs)
                            .withMenuId(R.id.bookmark_all_tabs)
                            .withIsIncognito(false)
                            .build());
        }
        // Add "Name window" option.
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.menu_name_window)
                            .withMenuId(R.id.name_window)
                            .withIsIncognito(isIncognito)
                            .build());
        }
        // Add "Pin Gemini" option with divider
        Profile profile = mTabModel.getProfile();
        if (profile != null) {
            profile = profile.getOriginalProfile();
            if (GlicEnabling.isEnabledForProfile(profile)) {
                itemList.add(BasicListMenu.buildMenuDivider(isIncognito));

                boolean isPinned = GlicUtils.isButtonPinnedToTabStrip(profile);
                itemList.add(
                        new ListItemBuilder()
                                .withTitleRes(isPinned ? R.string.glic_unpin : R.string.glic_pin)
                                .withMenuId(isPinned ? R.id.unpin_glic : R.id.pin_glic)
                                .withIsIncognito(isIncognito)
                                .build());
            }
        }
        // Add vertical tabs section (layout option and send feedback) with divider
        if (VerticalTabUtils.isVerticalTabsEligible(mContext)) {
            itemList.add(BasicListMenu.buildMenuDivider(isIncognito));

            boolean isEnablingVerticalTabs = mTabStripLayout == TabStripLayoutType.HORIZONTAL;
            int layoutTitleRes =
                    isEnablingVerticalTabs
                            ? R.string.show_tabs_vertically
                            : R.string.show_tabs_horizontally;

            boolean enabled =
                    mCanActivateTabLayoutToggleMenuSupplier == null
                            || mCanActivateTabLayoutToggleMenuSupplier.getAsBoolean();

            boolean showNewBadge =
                    isEnablingVerticalTabs
                            && VerticalTabUtils.shouldShowNewBadgeForVerticalTabs(mContext);

            CharSequence title;
            if (showNewBadge) {
                // Increment view count every time the badge is shown.
                VerticalTabUtils.incrementNewBadgeViewCount();
                // Prepare the title with the "New" badge.
                title = VerticalTabUtils.getTitleWithNewBadge(mContext, layoutTitleRes);
            } else {
                // Show the regular title without the "New" badge.
                title = mContext.getString(layoutTitleRes);
            }

            ListItem item =
                    new ListItemBuilder()
                            .withTitle(title)
                            .withMenuId(R.id.toggle_tab_layout_menu_id)
                            .withIsIncognito(isIncognito)
                            .withEnabled(enabled)
                            .build();
            itemList.add(item);

            // Add "Send feedback" option
            if (FeedbackPolicyManager.getInstance().isUserFeedbackAllowed()) {
                itemList.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.send_feedback_about_tab_strip)
                                .withMenuId(R.id.send_feedback_about_tab_strip_menu_id)
                                .withIsIncognito(isIncognito)
                                .build());
            }
        }
        // Add "Task Manager" option with divider.
        if (TaskManager.isEnabled()) {
            itemList.add(BasicListMenu.buildMenuDivider(isIncognito));
            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.menu_task_manager)
                            .withMenuId(R.id.task_manager)
                            .withIsIncognito(isIncognito)
                            .build());
        }
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
            Profile profile = mTabModel.getProfile();
            if (profile != null) {
                profile = profile.getOriginalProfile();
            }
            if (model.get(MENU_ITEM_ID) == R.id.new_tab_menu_id) {
                mOnNewTabClick.run();
            } else if (model.get(MENU_ITEM_ID) == R.id.reopen_closed_entry) {
                TabStripMenuMetricsUtils.recordStripMenuUserAction(
                        StripMenuAction.REOPEN_CLOSED_ENTRY, mTabStripLayout);
                mTabModel.openMostRecentlyClosedEntry();
            } else if (model.get(MENU_ITEM_ID) == R.id.bookmark_all_tabs) {
                BookmarkAllTabsHandler.bookmarkAllTabs(mTabModel, mWindowAndroid, mSnackbarManager);
            } else if (model.get(MENU_ITEM_ID) == R.id.name_window) {
                mMultiInstanceManager.showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);
            } else if (model.get(MENU_ITEM_ID) == R.id.toggle_tab_layout_menu_id) {
                TabStripMenuMetricsUtils.recordStripMenuUserAction(
                        StripMenuAction.TOGGLE_TAB_LAYOUT, mTabStripLayout);
                boolean isEnablingVerticalTabs = mTabStripLayout == TabStripLayoutType.HORIZONTAL;
                VerticalTabUtils.recordLayoutToggle(
                        LayoutSwitchEntryPoint.TAB_STRIP_CONTEXT_MENU, isEnablingVerticalTabs);
                if (mContext instanceof MenuOrKeyboardActionController controller) {
                    controller.onMenuOrKeyboardAction(
                            R.id.toggle_tab_layout_menu_id, /* fromMenu= */ false);
                }
            } else if (model.get(MENU_ITEM_ID) == R.id.pin_glic
                    || model.get(MENU_ITEM_ID) == R.id.unpin_glic) {
                boolean isPin = model.get(MENU_ITEM_ID) == R.id.pin_glic;
                if (isPin) {
                    TabStripMenuMetricsUtils.recordStripMenuUserAction(
                            StripMenuAction.PIN_GLIC, mTabStripLayout);
                } else {
                    TabStripMenuMetricsUtils.recordStripMenuUserAction(
                            StripMenuAction.UNPIN_GLIC, mTabStripLayout);
                }
                if (profile != null) GlicUtils.setButtonPinnedToTabStrip(profile, isPin);
            } else if (model.get(MENU_ITEM_ID) == R.id.task_manager) {
                TabStripMenuMetricsUtils.recordStripMenuUserAction(
                        StripMenuAction.TASK_MANAGER, mTabStripLayout);
                TaskManager taskManager = TaskManagerFactory.createTaskManager();
                taskManager.launch(ContextUtils.getApplicationContext());
            } else if (model.get(MENU_ITEM_ID) == R.id.send_feedback_about_tab_strip_menu_id) {
                TabStripMenuMetricsUtils.recordStripMenuUserAction(
                        StripMenuAction.SEND_FEEDBACK, mTabStripLayout);
                Activity activity = mWindowAndroid.getActivity().get();
                if (activity != null && profile != null) {
                    String categoryTag = getFeedbackCategoryTag();
                    HelpAndFeedbackLauncherFactory.getForProfile(profile)
                            .showFeedback(activity, /* url= */ null, categoryTag);
                }
            }
            assumeNonNull(mMenuWindow).dismiss();
        };
    }

    /**
     * Returns the appropriate feedback category tag to send with the feedback request. A Listnr
     * allowlisted category tag is required when sending feedback otherwise Listnr will drop the
     * request silently.
     */
    @VisibleForTesting
    @Nullable String getFeedbackCategoryTag() {
        String prefix;
        if (VersionInfo.isCanaryBuild()) {
            prefix = "com.chrome.canary";
        } else if (VersionInfo.isDevBuild()) {
            prefix = "com.chrome.dev";
        } else if (VersionInfo.isBetaBuild()) {
            prefix = "com.chrome.beta";
        } else if (VersionInfo.isStableBuild()) {
            prefix = "com.android.chrome";
        } else {
            return null;
        }
        return prefix + FEEDBACK_CATEGORY_SUFFIX;
    }

    /**
     * Dismisses the menu. No-op if the menu holder is {@code null}, and therefore the menu is not
     * already showing.
     */
    public void dismiss() {
        if (mMenuWindow != null) {
            mMenuWindow.dismiss();
        }
    }

    /** Permanently cleans up this component. */
    public void destroy() {
        dismiss();
        mMenuWindow = null;
    }

    /**
     * @return Whether the context menu is currently showing.
     */
    public boolean isMenuShowing() {
        return mMenuWindow != null && mMenuWindow.isShowing();
    }
}
