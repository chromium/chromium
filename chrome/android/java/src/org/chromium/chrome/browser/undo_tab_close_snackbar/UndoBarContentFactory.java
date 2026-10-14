// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoClosureSnapshot.ClosureKind;

import java.util.HashSet;
import java.util.Set;

/** Builds the {@link UndoBarContent} of the undo snackbar for an {@link UndoClosureSnapshot}. */
@NullMarked
final class UndoBarContentFactory {
    /** How the tabs of a {@link ClosureKind#TAB_MODEL} snapshot break down into groups and tabs. */
    private static final class TabModelBreakdown {
        public final Set<Token> fullyClosingTabGroupIds = new HashSet<>();
        public final int ungroupedOrPartialGroupTabs;
        public final boolean isDeletingTabGroups;

        TabModelBreakdown(UndoClosureSnapshot snapshot) {
            boolean isDeleting = snapshot.isTabGroupSyncEnabled;
            int ungroupedOrPartialTabs = 0;

            for (UndoClosureSnapshot.ClosedTab tab : snapshot.tabs) {
                Token tabGroupId = tab.groupId;
                if (tabGroupId == null) {
                    ungroupedOrPartialTabs++;
                } else if (snapshot.isTabGroupSyncEnabled
                        && snapshot.hidingGroupIds.contains(tabGroupId)) {
                    fullyClosingTabGroupIds.add(tabGroupId);
                    isDeleting = false;
                } else if (snapshot.survivingGroupIds.contains(tabGroupId)) {
                    ungroupedOrPartialTabs++;
                    isDeleting = false;
                } else {
                    fullyClosingTabGroupIds.add(tabGroupId);
                }
            }
            isDeletingTabGroups = isDeleting && !fullyClosingTabGroupIds.isEmpty();
            ungroupedOrPartialGroupTabs = ungroupedOrPartialTabs;
        }
    }

    private UndoBarContentFactory() {}

    /**
     * Computes the undo snackbar content purely from a snapshot.
     *
     * @param context The context used to resolve string resources.
     * @param snapshot The immutable snapshot of the closed tabs and groups.
     * @return The snapshot, template text, content text and UMA type for the undo snackbar.
     */
    static UndoBarContent create(Context context, UndoClosureSnapshot snapshot) {
        switch (snapshot.kind) {
            case ClosureKind.TAB_MODEL:
                return getTabModelContent(context, snapshot);
            case ClosureKind.SAVED_GROUPS:
                return getSavedGroupsContent(context, snapshot);
            default:
                throw new IllegalStateException("Unknown closure kind: " + snapshot.kind);
        }
    }

    private static UndoBarContent getTabModelContent(
            Context context, UndoClosureSnapshot snapshot) {
        TabModelBreakdown breakdown = new TabModelBreakdown(snapshot);
        int umaType = computeUmaType(snapshot, breakdown.isDeletingTabGroups);
        Set<Token> fullyClosingTabGroupIds = breakdown.fullyClosingTabGroupIds;
        int ungroupedOrPartialGroupTabs = breakdown.ungroupedOrPartialGroupTabs;
        boolean isDeletingTabGroups = breakdown.isDeletingTabGroups;

        int totalTabsCount = snapshot.tabs.size();
        int tabGroupsCount = fullyClosingTabGroupIds.size();
        if (tabGroupsCount == 0) {
            if (ungroupedOrPartialGroupTabs == 1) {
                return new UndoBarContent(
                        snapshot,
                        context.getString(R.string.undo_bar_close_message),
                        snapshot.tabs.get(0).title,
                        umaType);
            } else if (ungroupedOrPartialGroupTabs > 1) {
                return new UndoBarContent(
                        snapshot,
                        context.getString(R.string.undo_bar_close_all_message),
                        Integer.toString(totalTabsCount),
                        umaType);
            } else {
                assert false : "Not reached.";
                return new UndoBarContent(snapshot, "", "", umaType);
            }
        } else if (tabGroupsCount == 1) {
            if (ungroupedOrPartialGroupTabs == 0) {
                Token tabGroupId = fullyClosingTabGroupIds.iterator().next();
                @Nullable String tabGroupTitle = snapshot.groupTitles.get(tabGroupId);
                if (TextUtils.isEmpty(tabGroupTitle)) {
                    tabGroupTitle =
                            context.getResources()
                                    .getQuantityString(
                                            R.plurals.bottom_tab_grid_title_placeholder,
                                            totalTabsCount,
                                            totalTabsCount);
                }
                int templateRes;
                if (isDeletingTabGroups) {
                    templateRes = R.string.undo_bar_tab_group_deleted_message;
                } else {
                    templateRes =
                            snapshot.isTabGroupSyncEnabled
                                    ? R.string.undo_bar_tab_group_closed_and_saved_message
                                    : R.string.undo_bar_tab_group_closed_message;
                }
                return new UndoBarContent(
                        snapshot,
                        context.getString(templateRes),
                        tabGroupTitle != null ? tabGroupTitle : "",
                        umaType);
            }
        }

        // All other strings are some combination of x tab group(s), y tab(s).
        int templateRes;
        if (isDeletingTabGroups) {
            templateRes = R.string.undo_bar_deleted_message;
        } else {
            templateRes =
                    snapshot.isTabGroupSyncEnabled
                            ? R.string.undo_bar_closed_and_saved_message
                            : R.string.undo_bar_closed_message;
        }
        return new UndoBarContent(
                snapshot,
                context.getString(templateRes),
                formatGroupsAndTabs(
                        context.getResources(), tabGroupsCount, ungroupedOrPartialGroupTabs),
                umaType);
    }

    private static UndoBarContent getSavedGroupsContent(
            Context context, UndoClosureSnapshot snapshot) {
        // Closing any saved group records the delete UMA, even though the text says "closed".
        int umaType =
                computeUmaType(
                        snapshot,
                        /* isDeletingTabGroups= */ !snapshot.savedTabGroupSyncIds.isEmpty());
        int totalTabsCount = snapshot.tabs.size();
        int tabGroupsCount = snapshot.savedTabGroupSyncIds.size();
        if (tabGroupsCount == 0) {
            if (totalTabsCount == 1) {
                return new UndoBarContent(
                        snapshot,
                        context.getString(R.string.undo_bar_close_message),
                        snapshot.tabs.get(0).title,
                        umaType);
            } else if (totalTabsCount > 1) {
                return new UndoBarContent(
                        snapshot,
                        context.getString(R.string.undo_bar_close_all_message),
                        Integer.toString(totalTabsCount),
                        umaType);
            } else {
                assert false : "Not reached.";
                return new UndoBarContent(snapshot, "", "", umaType);
            }
        } else if (tabGroupsCount == 1 && totalTabsCount == 0) {
            String tabGroupTitle = snapshot.savedGroupTitle;
            // A saved group that was not found (or had no sync service) has no name to show; fall
            // through to the generic "1 tab group closed" text below instead of a blank name.
            if (tabGroupTitle != null) {
                if (tabGroupTitle.isEmpty()) {
                    tabGroupTitle =
                            context.getResources()
                                    .getQuantityString(
                                            R.plurals.bottom_tab_grid_title_placeholder,
                                            snapshot.savedGroupTabCount,
                                            snapshot.savedGroupTabCount);
                }
                return new UndoBarContent(
                        snapshot,
                        context.getString(R.string.undo_bar_tab_group_closed_message),
                        tabGroupTitle,
                        umaType);
            }
        }

        // All other strings are some combination of x tab group(s), y tab(s).
        return new UndoBarContent(
                snapshot,
                context.getString(R.string.undo_bar_closed_message),
                formatGroupsAndTabs(context.getResources(), tabGroupsCount, totalTabsCount),
                umaType);
    }

    /**
     * Formats the "x tab group(s)" or "x tab group(s), y tab(s)" part of the undo snackbar.
     *
     * @param res The resources used to resolve plural strings.
     * @param groupsCount The number of closed tab groups.
     * @param tabsCount The number of closed tabs to list in addition to the groups; omitted if 0.
     * @return The formatted groups and tabs text.
     */
    private static String formatGroupsAndTabs(Resources res, int groupsCount, int tabsCount) {
        String groupsPart =
                res.getQuantityString(R.plurals.undo_bar_tab_groups_part, groupsCount, groupsCount);
        if (tabsCount <= 0) return groupsPart;
        return res.getQuantityString(
                R.plurals.undo_bar_tab_groups_and_tabs_part, tabsCount, groupsPart, tabsCount);
    }

    private static int computeUmaType(UndoClosureSnapshot snapshot, boolean isDeletingTabGroups) {
        boolean singleTab = snapshot.tabs.size() == 1;
        if (isDeletingTabGroups) {
            return singleTab
                    ? Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO
                    : Snackbar.UMA_TAB_GROUP_DELETE_UNDO;
        } else if (snapshot.isAllTabs) {
            return Snackbar.UMA_TAB_CLOSE_ALL_UNDO;
        }
        return singleTab ? Snackbar.UMA_TAB_CLOSE_UNDO : Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO;
    }
}
