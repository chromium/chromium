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
public class TabListEditorCloseArchivedTabsAction extends TabListEditorAction {
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
        return new TabListEditorCloseArchivedTabsAction(context, archiveDelegate);
    }

    @VisibleForTesting
    TabListEditorCloseArchivedTabsAction(
            @NonNull Context context,
            @NonNull ArchivedTabsDialogCoordinator.ArchiveDelegate archiveDelegate) {
        super(
                R.id.tab_list_editor_close_archived_tabs_menu_item,
                ShowMode.MENU_ONLY,
                ButtonType.TEXT,
                IconPosition.START,
                R.plurals.tab_selection_editor_close_tabs,
                R.plurals.accessibility_tab_selection_editor_close_tabs,
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
        setEnabledAndItemCount(tabIds.size() > 0, tabIds.size());
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        mArchiveDelegate.closeArchivedTabs(tabs);
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return false;
    }
}
