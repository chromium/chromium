// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.Collections;
import java.util.List;

/**
 * A controller that receives direct events to fire snackbars representsing cancelable tab and
 * {@link SavedTabGroup} closures.
 *
 * <p>This controller mixes both tabs and SavedTabGroups for undo operations, but requires an
 * explicit event to queue a snackbar rather than listen for tab changes to the model.
 */
@NullMarked
public class SavedTabGroupUndoBarController extends UndoBarController
        implements UndoBarExplicitTrigger {
    private final @Nullable TabGroupSyncService mTabGroupSyncService;

    /**
     * Creates an instance of a {@link SavedTabGroupUndoBarController}.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param selector The {@link TabModelSelector} that will be used to commit and undo tab
     *     closures.
     * @param snackbarManageable The holder class to get the manager that helps to show up snackbar.
     * @param tabGroupSyncService The {@link TabGroupSyncService} to handle synced tab group
     *     actions.
     */
    public SavedTabGroupUndoBarController(
            Context context,
            TabModelSelector selector,
            SnackbarManageable snackbarManageable,
            @Nullable TabGroupSyncService tabGroupSyncService) {
        super(context, selector, snackbarManageable);
        mTabGroupSyncService = tabGroupSyncService;
    }

    /**
     * Explicitly queues an undo snackbar to show based on a triggered action.
     *
     * @param tabs The list of closed tabs to be shown by the snackbar.
     * @param savedtabGroupSyncIds The list of closed {@link SavedTabGroup} syncIds shown by the
     *     snackbar.
     */
    public void queueUndoBar(List<Tab> tabs, List<String> savedTabGroupSyncIds) {
        queueUndoBar(new TabClosureEvent(tabs, savedTabGroupSyncIds, /* isAllTabs= */ false));
    }

    @Override
    protected Pair<String, String> getTemplateAndContentText(
            List<Tab> closedTabs, List<String> closedSavedTabGroupSyncIds) {
        int totalTabsCount = closedTabs.size();
        int tabGroupsCount = closedSavedTabGroupSyncIds.size();

        if (tabGroupsCount == 0) {
            if (totalTabsCount == 1) {
                return Pair.create(
                        mContext.getString(R.string.undo_bar_close_message),
                        closedTabs.get(0).getTitle());
            } else if (totalTabsCount > 1) {
                return Pair.create(
                        mContext.getString(R.string.undo_bar_close_all_message),
                        Integer.toString(totalTabsCount));
            } else {
                assert false : "Not reached.";
                return Pair.create("", "");
            }
        } else if (tabGroupsCount == 1) {
            if (totalTabsCount == 0) {
                String tabGroupTitle = "";
                if (mTabGroupSyncService != null) {
                    SavedTabGroup savedTabGroup =
                            mTabGroupSyncService.getGroup(closedSavedTabGroupSyncIds.get(0));
                    if (savedTabGroup != null) {
                        tabGroupTitle = savedTabGroup.title;
                        if (TextUtils.isEmpty(tabGroupTitle)) {
                            tabGroupTitle =
                                    mContext.getResources()
                                            .getQuantityString(
                                                    R.plurals.bottom_tab_grid_title_placeholder,
                                                    savedTabGroup.savedTabs.size(),
                                                    savedTabGroup.savedTabs.size());
                        }
                    }
                }
                return Pair.create(
                        mContext.getString(R.string.undo_bar_tab_group_closed_message),
                        tabGroupTitle);
            }
        }

        // All other strings are some combination of x tab group(s), y tab(s).
        Resources res = mContext.getResources();
        String tabGroupsPart =
                res.getQuantityString(
                        R.plurals.undo_bar_tab_groups_part, tabGroupsCount, tabGroupsCount);
        String tabGroupsAndTabsPart;
        if (totalTabsCount > 0) {
            tabGroupsAndTabsPart =
                    res.getQuantityString(
                            R.plurals.undo_bar_tab_groups_and_tabs_part,
                            totalTabsCount,
                            tabGroupsPart,
                            totalTabsCount);
        } else {
            tabGroupsAndTabsPart = tabGroupsPart;
        }
        return Pair.create(
                mContext.getString(R.string.undo_bar_closed_message), tabGroupsAndTabsPart);
    }

    @Override
    protected boolean isDeletingTabGroups(List<String> savedTabGroupSyncIds) {
        return !savedTabGroupSyncIds.isEmpty();
    }

    // SnackbarManager.SnackbarController implementation.

    @SuppressWarnings("unchecked")
    @Override
    public void onAction(@Nullable Object actionData) {
        super.onAction(actionData);
        UndoActionData undoActionData = assumeNonNull((UndoActionData) actionData);
        List<String> closedSavedTabGroupSyncIds = undoActionData.closedSavedTabGroupSyncIds;
        if (!closedSavedTabGroupSyncIds.isEmpty()) {
            for (String syncId : closedSavedTabGroupSyncIds) {
                cancelSavedTabGroupClosure(syncId);
            }
        }
    }

    private void cancelSavedTabGroupClosure(String syncId) {
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.updateArchivalStatus(syncId, true);
        }
    }

    // UndoBarExplicitTrigger implementation.

    @Override
    public void triggerSnackbarForSavedTabGroup(String syncId) {
        queueUndoBar(Collections.emptyList(), List.of(syncId));
    }

    @Override
    public void triggerSnackbarForTab(Tab tab) {
        queueUndoBar(List.of(tab), Collections.emptyList());
    }
}
