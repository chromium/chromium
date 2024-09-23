// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/** Restore all archived tabs action for the {@link TabListEditorMenu}. */
public class TabListEditorRestoreAllArchivedTabsAction extends TabListEditorAction {
    private final @NonNull Context mContext;
    private final @NonNull ArchivedTabsDialogCoordinator.ArchiveDelegate mArchiveDelegate;

    /**
     * Create an action for restoring archived tabs.
     *
     * @param context to load drawable from.
     * @param archiveDelegate delegate which supports archive operations.
     */
    public static TabListEditorAction createAction(
            @NonNull Context context,
            @NonNull ArchivedTabsDialogCoordinator.ArchiveDelegate archiveDelegate) {
        return new TabListEditorRestoreAllArchivedTabsAction(context, archiveDelegate);
    }

    @VisibleForTesting
    TabListEditorRestoreAllArchivedTabsAction(
            @NonNull Context context,
            @NonNull ArchivedTabsDialogCoordinator.ArchiveDelegate archiveDelegate) {
        super(
                R.id.tab_list_editor_restore_all_archived_tabs_menu_item,
                ShowMode.MENU_ONLY,
                ButtonType.TEXT,
                IconPosition.START,
                R.string.archived_tabs_dialog_restore_all_action,
                null,
                null);

        mContext = context;
        mArchiveDelegate = archiveDelegate;
    }

    @Override
    public boolean shouldNotifyObserversOfAction() {
        return false;
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        setEnabledAndItemCount(true, tabIds.size());
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        mArchiveDelegate.restoreAllArchivedTabs();
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return false;
    }
}
