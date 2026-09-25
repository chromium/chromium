// Copyright 2026 The Chromium Authors
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

/** An interface to be notified about tab group changes on a {@link TabModel}. */
@NullMarked
public interface TabGroupObserver {
    /** The reason for the tab group being removed. */
    @IntDef({
        DidRemoveTabGroupReason.MERGE,
        DidRemoveTabGroupReason.UNGROUP,
        DidRemoveTabGroupReason.CLOSE,
        DidRemoveTabGroupReason.PIN
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
     * This method is called before a group is moved.
     *
     * @param tabGroupId The tab group id of the group being moved.
     */
    default void willMoveTabGroup(Token tabGroupId) {}

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

    /**
     * This method is called after a group is moved.
     *
     * @param tabGroupId The tab group ID of the group that was moved.
     * @param tabModelOldIndex The old index of the first tab in the group in the {@link TabModel}.
     * @param tabModelNewIndex The new index of the first tab in the group in the {@link TabModel}.
     */
    default void didMoveTabGroup(Token tabGroupId, int tabModelOldIndex, int tabModelNewIndex) {}

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
     * @param oldTabGroupId The tab group ID of the group where {@code movedTab} was before
     *     ungrouping.
     */
    default void didMoveTabOutOfGroup(Tab movedTab, Token oldTabGroupId) {}

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
     */
    default void didCreateNewGroup(Tab destinationTab) {}

    /**
     * This method is called after a new title is set on a tab group.
     *
     * @param tabGroupId The tab group id.
     * @param newTitle The new title.
     */
    default void didChangeTabGroupTitle(Token tabGroupId, String newTitle) {}

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
     * Called when a tab group is about to be removed from the tab model (e.g. as a result of
     * closure, ungrouping, or merging), prior to {@link #didRemoveTabGroup}.
     *
     * @param tabGroupId The tab group id being removed.
     */
    default void willRemoveTabGroup(Token tabGroupId) {}

    /**
     * Called when a tab group is removed from the tab model. This could be the result of merging
     * tabs, ungrouping tabs, pinning tabs, or closing tabs.
     *
     * @param oldTabGroupId The tab group ID the group previously used.
     * @param removalReason The {@link DidRemoveTabGroupReason} for the group being removed.
     */
    default void didRemoveTabGroup(
            Token oldTabGroupId, @DidRemoveTabGroupReason int removalReason) {}

    /**
     * Called when a tab group closure starts.
     *
     * @param tabGroupId The tab group id.
     * @param isHiding Whether the tab group is set to hide.
     */
    default void willCloseTabGroup(Token tabGroupId, boolean isHiding) {}
}
