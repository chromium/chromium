// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import androidx.annotation.IntDef;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** An immutable snapshot of the state required to show the undo snackbar. */
@NullMarked
final class UndoClosureSnapshot {
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
    static final class ClosedTab {
        /** The identifier of the closed tab. */
        public final int id;

        /** The title of the closed tab. */
        public final String title;

        /** The identifier of the tab group this tab belonged to, if any. */
        public final @Nullable Token groupId;

        /**
         * Constructs a closed tab representation.
         *
         * @param id The identifier of the closed tab.
         * @param title The title of the closed tab.
         * @param groupId The identifier of the tab group this tab belonged to, or null if
         *     ungrouped.
         */
        ClosedTab(int id, String title, @Nullable Token groupId) {
            this.id = id;
            this.title = title;
            this.groupId = groupId;
        }
    }

    /** The kind of closure event this snapshot was captured from. */
    public final @ClosureKind int kind;

    /** The list of tabs captured in this snapshot. */
    public final List<ClosedTab> tabs;

    /**
     * A mapping from group identifiers to their titles for the groups involved in this snapshot.
     */
    public final Map<Token, @Nullable String> groupTitles;

    /** The set of group identifiers that are hiding as a result of this closure event. */
    public final Set<Token> hidingGroupIds;

    /**
     * The set of group identifiers that survive in the comprehensive model despite this closure
     * event.
     */
    public final Set<Token> survivingGroupIds;

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
     * Creates a {@link ClosureKind#TAB_MODEL} snapshot. The saved-group-only fields are left empty,
     * null or 0.
     *
     * @param tabs The list of closed tabs.
     * @param groupTitles The mapping from group identifiers to their titles.
     * @param hidingGroupIds The set of group identifiers that are hiding.
     * @param survivingGroupIds The set of group identifiers that survive.
     * @param isTabGroupSyncEnabled Whether tab group sync is enabled.
     * @param isAllTabs Whether all tabs were closed.
     * @return The immutable snapshot.
     */
    static UndoClosureSnapshot forTabModel(
            List<ClosedTab> tabs,
            Map<Token, @Nullable String> groupTitles,
            Set<Token> hidingGroupIds,
            Set<Token> survivingGroupIds,
            boolean isTabGroupSyncEnabled,
            boolean isAllTabs) {
        return new UndoClosureSnapshot(
                ClosureKind.TAB_MODEL,
                tabs,
                groupTitles,
                hidingGroupIds,
                survivingGroupIds,
                isTabGroupSyncEnabled,
                isAllTabs,
                /* savedTabGroupSyncIds= */ Collections.emptyList(),
                /* savedGroupTitle= */ null,
                /* savedGroupTabCount= */ 0);
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
    static UndoClosureSnapshot forSavedGroups(
            List<ClosedTab> tabs,
            List<String> savedTabGroupSyncIds,
            @Nullable String savedGroupTitle,
            int savedGroupTabCount) {
        return new UndoClosureSnapshot(
                ClosureKind.SAVED_GROUPS,
                tabs,
                /* groupTitles= */ Collections.emptyMap(),
                /* hidingGroupIds= */ Collections.emptySet(),
                /* survivingGroupIds= */ Collections.emptySet(),
                /* isTabGroupSyncEnabled= */ false,
                /* isAllTabs= */ false,
                savedTabGroupSyncIds,
                savedGroupTitle,
                savedGroupTabCount);
    }

    /**
     * Constructs an immutable snapshot.
     *
     * @param kind The kind of closure event this snapshot represents.
     * @param tabs The list of closed tabs.
     * @param groupTitles The mapping from group identifiers to their titles.
     * @param hidingGroupIds The set of group identifiers that are hiding.
     * @param survivingGroupIds The set of group identifiers that survive.
     * @param isTabGroupSyncEnabled Whether tab group sync is enabled.
     * @param isAllTabs Whether all tabs were closed.
     * @param savedTabGroupSyncIds The list of closed saved tab group sync identifiers.
     * @param savedGroupTitle The title of the saved group, if applicable.
     * @param savedGroupTabCount The tab count of the saved group, if applicable.
     */
    private UndoClosureSnapshot(
            @ClosureKind int kind,
            List<ClosedTab> tabs,
            Map<Token, @Nullable String> groupTitles,
            Set<Token> hidingGroupIds,
            Set<Token> survivingGroupIds,
            boolean isTabGroupSyncEnabled,
            boolean isAllTabs,
            List<String> savedTabGroupSyncIds,
            @Nullable String savedGroupTitle,
            int savedGroupTabCount) {
        this.kind = kind;
        this.tabs = Collections.unmodifiableList(new ArrayList<>(tabs));
        this.groupTitles = Collections.unmodifiableMap(new HashMap<>(groupTitles));
        this.hidingGroupIds = Collections.unmodifiableSet(new HashSet<>(hidingGroupIds));
        this.survivingGroupIds = Collections.unmodifiableSet(new HashSet<>(survivingGroupIds));
        this.isTabGroupSyncEnabled = isTabGroupSyncEnabled;
        this.isAllTabs = isAllTabs;
        this.savedTabGroupSyncIds =
                Collections.unmodifiableList(new ArrayList<>(savedTabGroupSyncIds));
        this.savedGroupTitle = savedGroupTitle;
        this.savedGroupTabCount = savedGroupTabCount;
    }

    /**
     * Returns a new snapshot without the specified tab identifiers, or this instance if no tabs
     * match.
     *
     * @param ids The tab identifiers to exclude.
     * @return The modified snapshot or this instance if nothing changes.
     */
    UndoClosureSnapshot withoutTabs(Collection<Integer> ids) {
        List<ClosedTab> newTabs = new ArrayList<>();
        for (ClosedTab tab : tabs) {
            if (!ids.contains(tab.id)) {
                newTabs.add(tab);
            }
        }
        if (newTabs.size() == tabs.size()) return this;
        return new UndoClosureSnapshot(
                kind,
                newTabs,
                groupTitles,
                hidingGroupIds,
                survivingGroupIds,
                isTabGroupSyncEnabled,
                isAllTabs,
                savedTabGroupSyncIds,
                savedGroupTitle,
                savedGroupTabCount);
    }

    /**
     * Returns a new snapshot reflecting that the given group has been restored, or this instance if
     * nothing changes.
     *
     * @param gid The identifier of the restored group.
     * @return The modified snapshot or this instance if nothing changes.
     */
    UndoClosureSnapshot withGroupRestored(Token gid) {
        if (!groupTitles.containsKey(gid)
                || (!hidingGroupIds.contains(gid) && survivingGroupIds.contains(gid))) {
            return this;
        }

        Set<Token> newHiding = new HashSet<>(hidingGroupIds);
        newHiding.remove(gid);
        Set<Token> newSurviving = new HashSet<>(survivingGroupIds);
        newSurviving.add(gid);

        return new UndoClosureSnapshot(
                kind,
                tabs,
                groupTitles,
                newHiding,
                newSurviving,
                isTabGroupSyncEnabled,
                isAllTabs,
                savedTabGroupSyncIds,
                savedGroupTitle,
                savedGroupTabCount);
    }

    /**
     * Returns whether this snapshot contains no tabs and no saved tab group sync identifiers.
     *
     * @return True if the snapshot is empty, false otherwise.
     */
    boolean isEmpty() {
        return tabs.isEmpty() && savedTabGroupSyncIds.isEmpty();
    }

    /**
     * Returns the array of tab identifiers captured in this snapshot.
     *
     * @return The array of tab identifiers.
     */
    int[] tabIds() {
        int[] ids = new int[tabs.size()];
        for (int i = 0; i < tabs.size(); i++) {
            ids[i] = tabs.get(i).id;
        }
        return ids;
    }
}
