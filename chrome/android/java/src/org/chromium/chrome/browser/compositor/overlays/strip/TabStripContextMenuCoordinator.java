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
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.TabModel.RecentlyClosedEntryType;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView;
import org.chromium.ui.listmenu.BasicListMenu;
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
    private final TabStripContextMenuDelegate mDelegate;
    private @Nullable AnchoredPopupWindow mMenuWindow;

    /**
     * @param context The {@link Context} to build the menu with.
     * @param delegate The {@link TabStripContextMenuDelegate} to handle menu actions.
     */
    public TabStripContextMenuCoordinator(Context context, TabStripContextMenuDelegate delegate) {
        mContext = context;
        mDelegate = delegate;
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
        clipContentViewOutline(contentView);

        // TODO (crbug.com/436283175): Update the name of this resource for generic use.
        TouchTrackingListView touchTrackingListView =
                contentView.findViewById(R.id.tab_group_action_menu_list);
        ListMenuItemAdapter adapter =
                createAdapter(modelList, Set.of(), getListMenuDelegate(contentView));
        touchTrackingListView.setItemsCanFocus(true);
        touchTrackingListView.setAdapter(adapter);

        View decorView = activity.getWindow().getDecorView();
        var popupWidthPx =
                MathUtils.clamp(
                        anchorViewRectProvider.getRect().width(),
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width));

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
                        .setPreferredHorizontalOrientation(HorizontalOrientation.LAYOUT_DIRECTION)
                        .setMaxWidth(popupWidthPx)
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
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.TAB_STRIP_EMPTY_SPACE_CONTEXT_MENU_ANDROID)) {
            // Add "New tab" option.
            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.menu_new_tab)
                            .withMenuId(R.id.new_tab_menu_id)
                            .withIsIncognito(isIncognito)
                            .build());
            // Add "Reopen closed tab/tabs/group" option.
            @RecentlyClosedEntryType
            int recentlyClosedEntryType = mDelegate.getRecentlyClosedEntryType();
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
            if (!isIncognito && mDelegate.getTabCount() > 1) {
                itemList.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.menu_bookmark_all_tabs)
                                .withMenuId(R.id.bookmark_all_tabs)
                                .withIsIncognito(false)
                                .build());
            }
        }
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
        // Add "Pin Gemini" option with divider
        if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.TAB_STRIP_EMPTY_SPACE_CONTEXT_MENU_ANDROID)
                && ChromeFeatureList.sGlic.isEnabled()) {
            if (!isIncognito) {
                itemList.add(BasicListMenu.buildMenuDivider(/* isIncognito= */ false));

                boolean isPinned =
                        ChromeSharedPreferences.getInstance()
                                .readBoolean(
                                        ChromePreferenceKeys.GLIC_BUTTON_PINNED,
                                        /* defaultValue= */ true);
                if (isPinned) {
                    itemList.add(
                            new ListItemBuilder()
                                    .withTitleRes(R.string.menu_unpin_glic)
                                    .withMenuId(R.id.unpin_glic)
                                    .withIsIncognito(false)
                                    .build());
                } else {
                    itemList.add(
                            new ListItemBuilder()
                                    .withTitleRes(R.string.menu_pin_glic)
                                    .withMenuId(R.id.pin_glic)
                                    .withIsIncognito(false)
                                    .build());
                }
            }
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
            if (model.get(MENU_ITEM_ID) == R.id.new_tab_menu_id) {
                mDelegate.onNewTab();
            } else if (model.get(MENU_ITEM_ID) == R.id.reopen_closed_entry) {
                mDelegate.onReopenClosedEntry();
            } else if (model.get(MENU_ITEM_ID) == R.id.bookmark_all_tabs) {
                mDelegate.onBookmarkAllTabs();
            } else if (model.get(MENU_ITEM_ID) == R.id.name_window) {
                mDelegate.onNameWindow();
            } else if (model.get(MENU_ITEM_ID) == R.id.pin_glic) {
                mDelegate.onPinGlic();
            } else if (model.get(MENU_ITEM_ID) == R.id.unpin_glic) {
                mDelegate.onUnpinGlic();
            }
            assumeNonNull(mMenuWindow).dismiss();
        };
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
