// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.app.Activity;

import androidx.annotation.LayoutRes;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Supplier;

/** A {@link TabOverflowMenuCoordinator} which also reorders items left / right in the tab strip. */
@NullMarked
public abstract class TabStripReorderingHelper<T> extends TabOverflowMenuCoordinator<T> {

    private static @Nullable Boolean sIsGesturesEnabled;
    private final BiConsumer<T, Boolean> mReorderFunction;

    /**
     * @param menuLayout The menu layout to use.
     * @param flyoutMenuLayout The menu layout for flyout popupps to use.
     * @param onItemClickedCallback A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param multiInstanceManager The {@link MultiInstanceManager}.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param collaborationService Used for checking the user is the owner of a group.
     * @param activity The {@link Activity} that the coordinator resides in.
     * @param reorderFunction The function to call to reorder. Its argument is a boolean of whether
     *     the item should be moved towards the start.
     */
    protected TabStripReorderingHelper(
            @LayoutRes int menuLayout,
            @LayoutRes int flyoutMenuLayout,
            OnItemClickedCallback<T> onItemClickedCallback,
            Supplier<TabModel> tabModelSupplier,
            @Nullable MultiInstanceManager multiInstanceManager,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            Activity activity,
            BiConsumer<T, Boolean> reorderFunction) {
        super(
                menuLayout,
                flyoutMenuLayout,
                onItemClickedCallback,
                tabModelSupplier,
                multiInstanceManager,
                tabGroupSyncService,
                collaborationService,
                activity);
        mReorderFunction = reorderFunction;
    }

    /** Return whether the item with id {@param id} can be reordered towards the start. */
    protected abstract boolean canItemMoveTowardStart(T id);

    /** Return whether the item with id {@param id} can be reordered towards the end. */
    protected abstract boolean canItemMoveTowardEnd(T id);

    /** Returns whether an a11y gesture service is on. (Can be overridden for testing.) */
    private boolean isGesturesEnabled() {
        return sIsGesturesEnabled == null
                ? AccessibilityState.isPerformGesturesEnabled()
                : sIsGesturesEnabled;
    }

    public void setIsGesturesEnabledForTesting(boolean enable) {
        Boolean oldValue = sIsGesturesEnabled;
        sIsGesturesEnabled = enable;
        ResettersForTesting.register(() -> sIsGesturesEnabled = oldValue);
    }

    /**
     * Returns list items for moving the item with id {@param id} to the left or to the right.
     *
     * <p>We only return list items when an a11y service that performs gestures is on.
     *
     * <p>If there are no list items, an empty list is returned.
     *
     * @param id The ID of the item of interest.
     * @param moveLeftString The string for moving an item of this type to the left. Note: this is
     *     truly left, not "start", so it is still left in RTL.
     * @param moveRightString The string for moving an item of this type to the right.
     * @return A list of menu list items for reordering the item with id {@param id}.
     */
    protected List<ListItem> createReorderItems(
            T id, String moveLeftString, String moveRightString) {
        if (!isGesturesEnabled()) return List.of();
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        ListItem moveTowardsStartItem =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, isRtl ? moveRightString : moveLeftString)
                                .with(ENABLED, true)
                                .with(
                                        CLICK_LISTENER,
                                        v -> mReorderFunction.accept(id, /* toLeft= */ !isRtl))
                                .build());
        ListItem moveTowardsEndItem =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, isRtl ? moveLeftString : moveRightString)
                                .with(ENABLED, true)
                                .with(
                                        CLICK_LISTENER,
                                        v -> mReorderFunction.accept(id, /* toLeft= */ isRtl))
                                .build());

        List<ListItem> result = new ArrayList<>();
        if (canItemMoveTowardStart(id)) {
            result.add(moveTowardsStartItem);
        }
        if (canItemMoveTowardEnd(id)) {
            result.add(moveTowardsEndItem);
        }
        return result;
    }
}
