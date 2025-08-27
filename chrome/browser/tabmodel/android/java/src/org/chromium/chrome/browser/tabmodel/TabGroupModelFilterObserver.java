// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// TODO(crbug.com/434015906): Remove all references to RootId after TabCollections is launched.
/** An interface to be notified about changes to a {@link TabGroupModelFilter}. */
@NullMarked
public interface TabGroupModelFilterObserver {
    /** The reason for the tab group being removed from {@link TabGroupModelFilter}. */
    @IntDef({
        DidRemoveTabGroupReason.MERGE,
        DidRemoveTabGroupReason.UNGROUP,
        DidRemoveTabGroupReason.CLOSE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DidRemoveTabGroupReason {
        /** Groups are merged together. */
        int MERGE = 0;

        /** Tab group no longer exists because the last tab in the group was removed. */
        int UNGROUP = 1;

        /**
         * The last tab in the group is closed. This can be from a tab group hide, tab group delete,
         * or closing of individual tabs.
         */
        int CLOSE = 2;

        /** The last tab in the group became pinned. */
        int PIN = 3;
    }

    /**
     * This method is called before a tab is moved to form a group or moved into an existed group.
     *
     * @param movedTab The {@link Tab} which will be moved. If a group will be merged to a tab or
     *     another group, this is the last tab of the merged group.
     * @param newRootId The new root id of the group after merge.
     * @param tabGroupId The tab group id of the group merged to.
     */
    default void willMergeTabToGroup(Tab movedTab, int newRootId, @Nullable Token tabGroupId) {}

    /**
     * This method is called before a group is moved.
     *
     * @param tabGroupId The tab group id of the group being moved.
     * @param currentIndex The current index of the group in the {@link TabModel}.
     */
    default void willMoveTabGroup(Token tabGroupId, int currentIndex) {}

    /**
     * This method is called before a tab within a group is moved out of the group.
     *
     * @param movedTab The tab which will be moved.
     * @param tabGroupId The tabGroupId the tab will have after the move, may be null if not in a
     *     group.
     */
    default void willMoveTabOutOfGroup(Tab movedTab, @Nullable Token destinationTabGroupId) {}

    /**
     * This method is called after a tab is moved to a group.
     *
     * @param movedTab The {@link Tab} which has been moved into the group.
     * @param isDestinationTab Whether the tab is the destination tab of a merge operation. The
     *     destination tab is the tab that all the other tabs in the merge operation will be grouped
     *     into.
     */
    default void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {}

    // TODO(crbug.com/434015906): Passing the last tab here is a limitation of the current TabGroupModelFilterImpl, we should fix this once tab collections is launched.
    /**
     * This method is called after a group is moved.
     *
     * @param movedTab The tab which has been moved. This is the last tab within the group.
     * @param tabModelOldIndex The old index of the {@code movedTab} in the {@link TabModel}.
     * @param tabModelNewIndex The new index of the {@code movedTab} in the {@link TabModel}.
     */
    default void didMoveTabGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {}

    /**
     * This method is called after a tab within a group is moved.
     *
     * @param movedTab The tab which has been moved.
     * @param tabModelOldIndex The old index of the {@code movedTab} in the {@link TabModel}.
     * @param tabModelNewIndex The new index of the {@code movedTab} in the {@link TabModel}.
     */
    default void didMoveWithinGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {}

    /**
     * This method is called after a tab within a group is moved out of the group.
     *
     * @param movedTab The tab which has been moved.
     * @param prevFilterIndex The index in {@link TabGroupModelFilter} of the group where {@code
     *     moveTab} was before ungrouping.
     */
    default void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {}

    /**
     * This method is called after a group is created and an undo group snackbar should be shown.
     *
     * @param undoGroupMetadata Metadata to undo the group operation.
     */
    default void showUndoGroupSnackbar(UndoGroupMetadata undoGroupMetadata) {}

    /**
     * This method is called after a new tab group is created, either through drag and drop, the tab
     * selection editor, or by longpressing a link on a tab and using the context menu.
     *
     * @param destinationTab The destination tab of the group after merge.
     * @param filter The {@link TabGroupModelFilter} that the new group event triggers on.
     */
    default void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {}

    /**
     * This method is called after a new title is set on a tab group.
     *
     * @param tabGroupId The tab group id.
     * @param newTitle The new title.
     */
    default void didChangeTabGroupTitle(Token tabGroupId, @Nullable String newTitle) {}

    /**
     * This method is called after a new color is set on a tab group.
     *
     * @param tabGroupId The tab group id.
     * @param newColor The new color.
     */
    default void didChangeTabGroupColor(Token tabGroupId, @TabGroupColorId int newColor) {}

    /**
     * This method is called when a tab group is collapsed or expanded on the tab strip.
     *
     * @param tabGroupId The tab group id.
     * @param isCollapsed Whether or not the tab group is now collapsed.
     * @param animate Whether the collapse or expand should be animated.
     */
    default void didChangeTabGroupCollapsed(
            Token tabGroupId, boolean isCollapsed, boolean animate) {}

    /**
     * When a tab group's root id needs to change because the tab whose id was previously being used
     * as the root ids is no longer part of the group. This could be a tab deletion that has not yet
     * been committed. Undo operations will not reverse this operation, as it does not have any user
     * facing effects.
     *
     * @param oldRootId The previous root id.
     * @param newRootId The new root id.
     */
    default void didChangeGroupRootId(int oldRootId, int newRootId) {}

    /**
     * Called when a tab group is removed from tab group model filter. This could be the result of
     * merging tabs, ungrouping tabs or closing tabs.
     *
     * @param oldRootId The root id the group previous used.
     * @param oldTabGroupId The tab group ID the group previously used, may be null if being
     *     re-used.
     * @param removalReason The {@link DidRemoveTabGroupReason} for the group being removed.
     */
    default void didRemoveTabGroup(
            int oldRootId,
            @Nullable Token oldTabGroupId,
            @DidRemoveTabGroupReason int removalReason) {}

    /**
     * Called when a tab group closure starts.
     *
     * @param tabGroupId The tab group id.
     * @param isHiding Whether the tab group is set to hide.
     */
    default void willCloseTabGroup(Token tabGroupId, boolean isHiding) {}

    /**
     * Called when a tab group closure is fully committed.
     *
     * @param tabGroupId The tab group id.
     * @param wasHiding Whether the tab group was set to hide when it started closing.
     */
    default void committedTabGroupClosure(Token tabGroupId, boolean wasHiding) {}
}
