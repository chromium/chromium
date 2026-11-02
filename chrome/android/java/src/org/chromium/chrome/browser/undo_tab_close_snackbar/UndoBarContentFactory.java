// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosureKind;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.GroupData;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;

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

        TabModelBreakdown(
                UndoClosureSnapshot snapshot,
                Set<@TabId Integer> remainingTabIds,
                Set<Token> restoredGroupIds) {
            boolean isDeleting = snapshot.isTabGroupSyncEnabled;
            int ungroupedOrPartialTabs = 0;

            for (ClosedTab tab : snapshot.tabs) {
                if (!remainingTabIds.contains(tab.id)) continue;
                Token tabGroupId = tab.groupId;
                if (tabGroupId == null) {
                    ungroupedOrPartialTabs++;
                } else {
                    GroupData groupData = snapshot.groups.get(tabGroupId);
                    // A group is no longer hiding and now has live tabs in the model if one of its
                    // tabs was restored externally while this bar was queued.
                    boolean isHiding =
                            groupData != null
                                    && groupData.isHiding
                                    && !restoredGroupIds.contains(tabGroupId);
                    boolean survivesInModel =
                            groupData != null
                                    && (groupData.survivesInModel
                                            || restoredGroupIds.contains(tabGroupId));

                    if (snapshot.isTabGroupSyncEnabled && isHiding) {
                        // A hiding synced group is closed as a whole group in the tab strip, but
                        // saved in sync rather than deleted.
                        fullyClosingTabGroupIds.add(tabGroupId);
                        isDeleting = false;
                    } else if (survivesInModel) {
                        // Other tabs remain in this group, so count this tab individually.
                        ungroupedOrPartialTabs++;
                        isDeleting = false;
                    } else {
                        fullyClosingTabGroupIds.add(tabGroupId);
                    }
                }
            }
            isDeletingTabGroups = isDeleting && !fullyClosingTabGroupIds.isEmpty();
            ungroupedOrPartialGroupTabs = ungroupedOrPartialTabs;
        }
    }

    private UndoBarContentFactory() {}

    /**
     * Computes the undo snackbar content from a snapshot and its remaining unresolved state.
     *
     * @param context The context used to resolve string resources.
     * @param snapshot The immutable snapshot of the closed tabs and groups.
     * @param remainingTabIds The {@link TabId}s of tabs in the snapshot that are still closed and
     *     not yet restored or committed.
     * @param restoredGroupIds The {@link Token} group ids of groups in the snapshot that have been
     *     restored while this bar was queued.
     * @return The snapshot, template text, content text and UMA type for the undo snackbar.
     */
    static UndoBarContent create(
            Context context,
            UndoClosureSnapshot snapshot,
            Set<@TabId Integer> remainingTabIds,
            Set<Token> restoredGroupIds) {
        switch (snapshot.kind) {
            case ClosureKind.TAB_MODEL:
                return getTabModelContent(context, snapshot, remainingTabIds, restoredGroupIds);
            case ClosureKind.SAVED_GROUPS:
                return getSavedGroupsContent(context, snapshot, remainingTabIds);
            default:
                throw new IllegalStateException("Unknown closure kind: " + snapshot.kind);
        }
    }

    private static String getSingleTabTitle(
            UndoClosureSnapshot snapshot, Set<@TabId Integer> remainingTabIds) {
        assert remainingTabIds.size() == 1;
        for (ClosedTab tab : snapshot.tabs) {
            if (remainingTabIds.contains(tab.id)) return tab.title;
        }
        return "";
    }

    private static UndoBarContent getTabModelContent(
            Context context,
            UndoClosureSnapshot snapshot,
            Set<@TabId Integer> remainingTabIds,
            Set<Token> restoredGroupIds) {
        TabModelBreakdown breakdown =
                new TabModelBreakdown(snapshot, remainingTabIds, restoredGroupIds);
        int totalTabsCount = remainingTabIds.size();
        int umaType = computeUmaType(snapshot, breakdown.isDeletingTabGroups, totalTabsCount);
        Set<Token> fullyClosingTabGroupIds = breakdown.fullyClosingTabGroupIds;
        int ungroupedOrPartialGroupTabs = breakdown.ungroupedOrPartialGroupTabs;
        boolean isDeletingTabGroups = breakdown.isDeletingTabGroups;

        int tabGroupsCount = fullyClosingTabGroupIds.size();
        if (tabGroupsCount == 0) {
            if (ungroupedOrPartialGroupTabs == 1) {
                return new UndoBarContent(
                        snapshot,
                        context.getString(R.string.undo_bar_close_message),
                        getSingleTabTitle(snapshot, remainingTabIds),
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
                GroupData groupData = snapshot.groups.get(tabGroupId);
                String tabGroupTitle =
                        groupData != null && groupData.title != null ? groupData.title : "";
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
                        snapshot, context.getString(templateRes), tabGroupTitle, umaType);
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
            Context context, UndoClosureSnapshot snapshot, Set<@TabId Integer> remainingTabIds) {
        int totalTabsCount = remainingTabIds.size();
        // Closing any saved group records the delete UMA, even though the text says "closed".
        int umaType =
                computeUmaType(
                        snapshot,
                        /* isDeletingTabGroups= */ !snapshot.savedTabGroupSyncIds.isEmpty(),
                        totalTabsCount);
        int tabGroupsCount = snapshot.savedTabGroupSyncIds.size();
        if (tabGroupsCount == 0) {
            if (totalTabsCount == 1) {
                return new UndoBarContent(
                        snapshot,
                        context.getString(R.string.undo_bar_close_message),
                        getSingleTabTitle(snapshot, remainingTabIds),
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

    private static int computeUmaType(
            UndoClosureSnapshot snapshot, boolean isDeletingTabGroups, int remainingTabsCount) {
        boolean singleTab = remainingTabsCount == 1;
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
