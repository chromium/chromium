// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.undo;

import androidx.annotation.IntDef;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** An immutable snapshot of the state required to show the undo snackbar. */
@NullMarked
public final class UndoClosureSnapshot {
    /** The kind of closure event a snapshot was captured from. */
    @IntDef({ClosureKind.TAB_MODEL, ClosureKind.SAVED_GROUPS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClosureKind {
        /** Tabs and tab groups closed in a tab model. */
        int TAB_MODEL = 0;

        /** Tabs and saved tab groups closed via an explicit trigger. */
        int SAVED_GROUPS = 1;
    }

    /** Represents a closed tab captured at close time. */
    public static final class ClosedTab {
        /** The identifier of the closed tab. */
        public final @TabId int id;

        /** The title of the closed tab. */
        public final String title;

        /** The identifier of the tab group this tab belonged to, if any. */
        public final @Nullable Token groupId;

        /**
         * The index of the tab within its tab group prior to closure, or {@link
         * TabList#INVALID_TAB_INDEX} if unknown or ungrouped. Only populated by a pre-removal
         * capture.
         */
        public final int indexInGroup;

        /**
         * The index of the tab in the model prior to closure, or {@link TabList#INVALID_TAB_INDEX}
         * when not populated by a pre-removal capture.
         */
        public final int modelIndex;

        /** Whether the tab was pinned prior to closure. */
        public final boolean pinned;

        /**
         * Whether the tab was selected prior to closure. Only populated by a pre-removal capture.
         */
        public final boolean wasSelected;

        /**
         * Constructs a closed tab representation.
         *
         * @param id The identifier of the closed tab.
         * @param title The title of the closed tab.
         * @param groupId The identifier of the tab group this tab belonged to, or null if
         *     ungrouped.
         * @param indexInGroup The index of the tab within its tab group prior to closure, or {@link
         *     TabList#INVALID_TAB_INDEX} if unknown or ungrouped.
         * @param modelIndex The index of the tab in the model prior to closure.
         * @param pinned Whether the tab was pinned prior to closure.
         * @param wasSelected Whether the tab was selected prior to closure.
         */
        public ClosedTab(
                @TabId int id,
                String title,
                @Nullable Token groupId,
                int indexInGroup,
                int modelIndex,
                boolean pinned,
                boolean wasSelected) {
            this.id = id;
            this.title = title;
            this.groupId = groupId;
            this.indexInGroup = indexInGroup;
            this.modelIndex = modelIndex;
            this.pinned = pinned;
            this.wasSelected = wasSelected;
        }
    }

    /** Represents a tab group captured at close time. */
    public static final class GroupData {
        /** The title of the closed tab group. */
        public final @Nullable String title;

        /** The color of the closed tab group. */
        public final int color;

        /** Whether the closed tab group was collapsed. */
        public final boolean collapsed;

        /** Whether the closed tab group was hiding. */
        public final boolean isHiding;

        /** Whether the closed tab group survives in the comprehensive model. */
        public final boolean survivesInModel;

        /**
         * Constructs a closed tab group representation.
         *
         * @param title The title of the closed tab group.
         * @param color The color of the closed tab group.
         * @param collapsed Whether the closed tab group was collapsed.
         * @param isHiding Whether the closed tab group was hiding.
         * @param survivesInModel Whether the closed tab group survives in the comprehensive model.
         */
        public GroupData(
                @Nullable String title,
                int color,
                boolean collapsed,
                boolean isHiding,
                boolean survivesInModel) {
            this.title = title;
            this.color = color;
            this.collapsed = collapsed;
            this.isHiding = isHiding;
            this.survivesInModel = survivesInModel;
        }
    }

    /** The kind of closure event this snapshot was captured from. */
    public final @ClosureKind int kind;

    /** The list of tabs captured in this snapshot. */
    public final List<ClosedTab> tabs;

    /** A mapping from group identifiers to their state for the groups involved in this snapshot. */
    public final Map<Token, GroupData> groups;

    /** Whether tab group sync is enabled for the profile. */
    public final boolean isTabGroupSyncEnabled;

    /** Whether all tabs in the model were closed in this event. */
    public final boolean isAllTabs;

    /** The list of saved tab group sync identifiers closed in this event. */
    public final List<String> savedTabGroupSyncIds;

    /**
     * The saved group's title (empty if it has none), or null if the sync service is unavailable or
     * the group was not found. Only set for SAVED_GROUPS snapshots capturing a single saved group.
     * When null, the snackbar shows the generic group count instead of the group's name.
     */
    public final @Nullable String savedGroupTitle;

    /**
     * The number of tabs in the saved tab group, or 0 if the sync service is unavailable or the
     * group was not found. Only meaningful when a single saved group is captured.
     */
    public final int savedGroupTabCount;

    /**
     * The TabRestoreService entry id, or {@link TabRestoreEntryId#INVALID_TAB_RESTORE_ENTRY_ID}
     * when not populated by a pre-removal capture.
     */
    public final @TabRestoreEntryId int entryId;

    /** The UI surface from which the tab closure originated. */
    public final @TabClosingSource int closingSource;

    /** Optional callback invoked after the closed tabs are restored on undo. */
    public final @Nullable Runnable undoRunnable;

    /**
     * Constructs an immutable snapshot.
     *
     * @param kind The kind of closure event this snapshot represents.
     * @param tabs The list of closed tabs.
     * @param groups The mapping from group identifiers to their state.
     * @param isTabGroupSyncEnabled Whether tab group sync is enabled.
     * @param isAllTabs Whether all tabs were closed.
     * @param savedTabGroupSyncIds The list of closed saved tab group sync identifiers.
     * @param savedGroupTitle The title of the saved group, if applicable.
     * @param savedGroupTabCount The tab count of the saved group, if applicable.
     * @param entryId The TabRestoreService entry id.
     * @param closingSource The UI surface from which the tab closure originated.
     * @param undoRunnable Optional callback invoked after the closed tabs are restored on undo.
     */
    private UndoClosureSnapshot(
            @ClosureKind int kind,
            List<ClosedTab> tabs,
            Map<Token, GroupData> groups,
            boolean isTabGroupSyncEnabled,
            boolean isAllTabs,
            List<String> savedTabGroupSyncIds,
            @Nullable String savedGroupTitle,
            int savedGroupTabCount,
            @TabRestoreEntryId int entryId,
            @TabClosingSource int closingSource,
            @Nullable Runnable undoRunnable) {
        this.kind = kind;
        this.tabs = Collections.unmodifiableList(new ArrayList<>(tabs));
        this.groups = Collections.unmodifiableMap(new HashMap<>(groups));
        this.isTabGroupSyncEnabled = isTabGroupSyncEnabled;
        this.isAllTabs = isAllTabs;
        this.savedTabGroupSyncIds =
                Collections.unmodifiableList(new ArrayList<>(savedTabGroupSyncIds));
        this.savedGroupTitle = savedGroupTitle;
        this.savedGroupTabCount = savedGroupTabCount;
        this.entryId = entryId;
        this.closingSource = closingSource;
        this.undoRunnable = undoRunnable;
    }

    /**
     * Creates a {@link ClosureKind#TAB_MODEL} snapshot. The saved-group-only fields are left empty,
     * null or 0.
     *
     * @param tabs The list of closed tabs.
     * @param groups The mapping from group identifiers to their state.
     * @param isTabGroupSyncEnabled Whether tab group sync is enabled.
     * @param isAllTabs Whether all tabs were closed.
     * @param entryId The TabRestoreService entry id.
     * @param closingSource The UI surface from which the tab closure originated.
     * @param undoRunnable Optional callback invoked after the closed tabs are restored on undo.
     * @return The immutable snapshot.
     */
    public static UndoClosureSnapshot forTabModel(
            List<ClosedTab> tabs,
            Map<Token, GroupData> groups,
            boolean isTabGroupSyncEnabled,
            boolean isAllTabs,
            @TabRestoreEntryId int entryId,
            @TabClosingSource int closingSource,
            @Nullable Runnable undoRunnable) {
        return new UndoClosureSnapshot(
                ClosureKind.TAB_MODEL,
                tabs,
                groups,
                isTabGroupSyncEnabled,
                isAllTabs,
                /* savedTabGroupSyncIds= */ Collections.emptyList(),
                /* savedGroupTitle= */ null,
                /* savedGroupTabCount= */ 0,
                entryId,
                closingSource,
                undoRunnable);
    }

    /**
     * Creates a {@link ClosureKind#SAVED_GROUPS} snapshot. The tab-model-only fields are left empty
     * or false.
     *
     * @param tabs The list of closed tabs.
     * @param savedTabGroupSyncIds The list of closed saved tab group sync identifiers.
     * @param savedGroupTitle The title of the saved group, if applicable.
     * @param savedGroupTabCount The tab count of the saved group, if applicable.
     * @return The immutable snapshot.
     */
    public static UndoClosureSnapshot forSavedGroups(
            List<ClosedTab> tabs,
            List<String> savedTabGroupSyncIds,
            @Nullable String savedGroupTitle,
            int savedGroupTabCount) {
        return new UndoClosureSnapshot(
                ClosureKind.SAVED_GROUPS,
                tabs,
                /* groups= */ Collections.emptyMap(),
                /* isTabGroupSyncEnabled= */ false,
                /* isAllTabs= */ false,
                savedTabGroupSyncIds,
                savedGroupTitle,
                savedGroupTabCount,
                /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                /* closingSource= */ TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null);
    }
}
