// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** An interface to be notified about changes to a {@link TabGroupModelFilter}. */
public interface TabGroupModelFilterObserver {
    /** The reason for the tab group being removed from {@link TabGroupModelFilter}. */
    @IntDef({
        DidRemoveTabGroupReason.MERGE,
        DidRemoveTabGroupReason.UNGROUP,
        DidRemoveTabGroupReason.CLOSE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DidRemoveTabGroupReason {
        /** Groups are merged together. */
        int MERGE = 0;

        /** Tab group no longer exists because the last tab in the group was removed. */
        int UNGROUP = 1;

        /**
         * The last tab in the group is closed. This can be from a tab group hide, tab group delete,
         * or closing of individual tabs.
         */
        int CLOSE = 2;
    }

    /**
     * This method is called before a tab is moved to form a group or moved into an existed group.
     *
     * @param movedTab The {@link Tab} which will be moved. If a group will be merged to a tab or
     *     another group, this is the last tab of the merged group.
     * @param newRootId The new root id of the group after merge.
     */
    default void willMergeTabToGroup(Tab movedTab, int newRootId) {}

    /**
     * This method is called before a group is moved.
     *
     * @param tabModelOldIndex The old index of the {@code movedTab} in the {@link TabModel}.
     * @param tabModelNewIndex The new index of the {@code movedTab} in the {@link TabModel}.
     */
    default void willMoveTabGroup(int tabModelOldIndex, int tabModelNewIndex) {}

    /**
     * This method is called before a tab within a group is moved out of the group.
     *
     * @param movedTab The tab which will be moved.
     * @param newRootId The new root id of the group from which {@code movedTab} is moved out.
     */
    default void willMoveTabOutOfGroup(Tab movedTab, int newRootId) {}

    /**
     * This method is called after a tab is moved to form a group or moved into an existed group.
     *
     * @param movedTab The {@link Tab} which has been moved. If a group is merged to a tab or
     *     another group, this is the last tab of the merged group.
     * @param selectedTabIdInGroup The id of the selected {@link Tab} in group.
     */
    default void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup) {}

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
     *     moveTab} is in before ungrouping.
     */
    default void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {}

    /**
     * This method is called after a group is created manually by user. Either using the
     * TabListEditor (Group tab menu item) or using drag and drop.
     *
     * @param tabs The list of modified {@link Tab}s.
     * @param tabOriginalIndex The original tab index for each modified tab.
     * @param tabOriginalRootId The original root id for each modified tab.
     * @param tabOriginalTabGroupId The original tab group id for each modified tab.
     * @param destinationGroupTitle The original destination group title.
     * @param destinationGroupColorId The original destination group color id.
     * @param destinationGroupTitleCollapsed Whether the destination group was originally collapsed.
     */
    default void didCreateGroup(
            List<Tab> tabs,
            List<Integer> tabOriginalIndex,
            List<Integer> tabOriginalRootId,
            List<Token> tabOriginalTabGroupId,
            String destinationGroupTitle,
            int destinationGroupColorId,
            boolean destinationGroupTitleCollapsed) {}

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
     * @param rootId The current rootId of the tab group.
     * @param newTitle The new title.
     */
    default void didChangeTabGroupTitle(int rootId, String newTitle) {}

    /**
     * This method is called after a new color is set on a tab group.
     *
     * @param rootId The current rootId of the tab group.
     * @param newColor The new color.
     */
    default void didChangeTabGroupColor(int rootId, @TabGroupColorId int newColor) {}

    /**
     * This method is called when a tab group is collapsed or expanded on the tab strip.
     *
     * @param rootId The current rootId of the tab group.
     * @param isCollapsed Whether or not the tab group is now collapsed.
     */
    default void didChangeTabGroupCollapsed(int rootId, boolean isCollapsed) {}

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
     * Called when a tab group closure is fully committed.
     *
     * @param tabGroupId The tab group id.
     * @param wasHiding Whether the tab group was set to hide when it started closing.
     */
    default void committedTabGroupClosure(Token tabGroupId, boolean wasHiding) {}
}
