// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;

import java.util.List;
import java.util.Set;

/**
 * A controller that listens to and visually represents cancelable tab closures.
 *
 * <p>Each time a tab is undoably closed via {@link TabModelObserver#tabPendingClosure(Tab)}, this
 * controller saves that tab id and title to the stack of SnackbarManager. It will then let
 * SnackbarManager to show a snackbar representing the top entry in of stack. Each added entry
 * resets the timeout that tracks when to commit the undoable actions.
 *
 * <p>When the undo button is clicked, it will cancel the tab closure if any. all pending closing
 * will be committed.
 *
 * <p>This class also responds to external changes to the undo state by monitoring {@link
 * TabModelObserver#tabClosureUndone(Tab)} and {@link TabModelObserver#tabClosureCommitted(Tab)} to
 * properly keep it's internal state in sync with the model.
 */
public class UndoBarController implements SnackbarManager.SnackbarController {
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final SnackbarManager.SnackbarManageable mSnackbarManagable;
    private final Context mContext;

    /**
     * Creates an instance of a {@link UndoBarController}.
     * @param context The {@link Context} in which snackbar is shown.
     * @param selector The {@link TabModelSelector} that will be used to commit and undo tab
     *                 closures.
     * @param snackbarManageable The holder class to get the manager that helps to show up snackbar.
     * @param dialogVisibilitySupplier The {@link Supplier} to get the visibility of TabGridDialog.
     */
    public UndoBarController(
            Context context,
            TabModelSelector selector,
            SnackbarManageable snackbarManageable,
            @Nullable Supplier<Boolean> dialogVisibilitySupplier) {
        mSnackbarManagable = snackbarManageable;
        mTabModelSelector = selector;
        mContext = context;

        mTabModelObserver =
                new TabModelObserver() {
                    /**
                     * Decides whether we should disable an attempt to show/hide the undo bar.
                     *
                     * @param showingUndoBar indicates whether the expected behavior of the caller
                     *     is to show or dismiss the undo bar.
                     */
                    private boolean disableUndo(boolean showingUndoBar) {
                        // When closure(s) happen and we are trying to show the undo bar, check
                        // whether the TabGridDialog is showing. If so, don't show the undo bar
                        // because TabGridDialog has its own undo bar. See crbug.com/1119899. Note
                        // that we don't disable attempts to dismiss snack bar to make sure that
                        // snack bar state is in sync with tab model.
                        return showingUndoBar
                                && dialogVisibilitySupplier != null
                                && dialogVisibilitySupplier.get();
                    }

                    @Override
                    public void tabPendingClosure(Tab tab) {
                        if (disableUndo(true)) return;
                        showUndoBar(List.of(tab), /* isAllTabs= */ false);
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        if (disableUndo(false)) return;
                        mSnackbarManagable
                                .getSnackbarManager()
                                .dismissSnackbars(UndoBarController.this, tab.getId());
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        if (disableUndo(false)) return;
                        mSnackbarManagable
                                .getSnackbarManager()
                                .dismissSnackbars(UndoBarController.this, tab.getId());
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        if (disableUndo(false)) return;
                        mSnackbarManagable
                                .getSnackbarManager()
                                .dismissSnackbars(UndoBarController.this, tabs);
                    }

                    @Override
                    public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                        if (disableUndo(true)) return;
                        showUndoBar(tabs, isAllTabs);
                    }

                    @Override
                    public void allTabsClosureCommitted(boolean isIncognito) {
                        if (disableUndo(false)) return;
                        mSnackbarManagable
                                .getSnackbarManager()
                                .dismissSnackbars(UndoBarController.this);
                    }
                };
    }

    /**
     * Carry out native library dependent operations like registering observers and notifications.
     */
    public void initialize() {
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
    }

    /**
     * Cleans up this class, unregistering for application notifications from the
     * {@link TabModelSelector}.
     */
    public void destroy() {
        TabModel model = mTabModelSelector.getModel(false);
        if (model != null) model.removeObserver(mTabModelObserver);
    }

    /**
     * Shows an undo close all bar. Based on user actions, this will cause a call to either {@link
     * TabModel#commitTabClosure(int)} or {@link TabModel#cancelTabClosure(int)} to be called for
     * each tab in {@code closedTabIds}. This will happen unless {@code
     * SnackbarManager#removeFromStackForData(Object)} is called.
     *
     * @param closedTabs A list of tabs that were closed.
     * @param isAllTabs Whether all tabs were closed.
     */
    private void showUndoBar(List<Tab> closedTabs, boolean isAllTabs) {
        if (closedTabs.isEmpty()) return;

        boolean singleTab = closedTabs.size() == 1;
        boolean deletingTabGroup = isDeletingTabGroup(closedTabs);

        String templateText = getTemplateText(singleTab, deletingTabGroup);
        int umaType = getUmaType(singleTab, deletingTabGroup, isAllTabs);

        Object actionData = singleTab ? closedTabs.get(0).getId() : closedTabs;
        String content;
        if (singleTab && !deletingTabGroup) {
            content = closedTabs.get(0).getTitle();
        } else {
            content = Integer.toString(closedTabs.size());
        }

        mSnackbarManagable
                .getSnackbarManager()
                .showSnackbar(
                        Snackbar.make(content, this, Snackbar.TYPE_ACTION, umaType)
                                .setDuration(
                                        isAllTabs
                                                ? SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS
                                                : SnackbarManager.DEFAULT_SNACKBAR_DURATION_MS)
                                .setTemplateText(templateText)
                                .setAction(mContext.getString(R.string.undo), actionData)
                                .setActionAccessibilityAnnouncement(
                                        getUndoneAccessibilityAnnouncement(content, false)));
    }

    private boolean isDeletingTabGroup(List<Tab> closedTabs) {
        if (closedTabs.isEmpty()) return false;

        assert !closedTabs.get(0).isIncognito();

        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        mTabModelSelector
                                .getTabModelFilterProvider()
                                .getTabModelFilter(/* isIncognito= */ false);
        Profile profile = filter.getTabModel().getProfile();
        if (profile == null || !profile.isNativeInitialized()) return false;

        if (!TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) return false;

        LazyOneshotSupplier<Set<Token>> tabGroupIdsInComprehensiveModel =
                filter.getLazyAllTabGroupIdsInComprehensiveModel(closedTabs);
        for (Tab tab : closedTabs) {
            // We are not deleting a tab group if:
            // 1. Any tabs are not in a tab group.
            // 2. Any of the tabs are in a group that is hiding.
            // 3. The comprehensive model still contains tabs with that group ID meaning the tab
            //    group is not being fully deleted as a result of this event.
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null
                    || filter.isTabGroupHiding(tabGroupId)
                    || tabGroupIdsInComprehensiveModel.get().contains(tabGroupId)) {
                return false;
            }
        }
        return true;
    }

    private String getTemplateText(boolean singleTab, boolean deletingTabGroup) {
        if (deletingTabGroup) {
            return singleTab
                    ? mContext.getString(R.string.undo_bar_delete_single_tab_group_message)
                    : mContext.getString(R.string.undo_bar_delete_tab_group_message);
        }
        return singleTab
                ? mContext.getString(R.string.undo_bar_close_message)
                : mContext.getString(R.string.undo_bar_close_all_message);
    }

    private int getUmaType(boolean singleTab, boolean deletingTabGroup, boolean isAllTabs) {
        if (deletingTabGroup) {
            return singleTab
                    ? Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO
                    : Snackbar.UMA_TAB_GROUP_DELETE_UNDO;
        } else if (isAllTabs) {
            return Snackbar.UMA_TAB_CLOSE_ALL_UNDO;
        }
        return singleTab ? Snackbar.UMA_TAB_CLOSE_UNDO : Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO;
    }

    private String getUndoneAccessibilityAnnouncement(String content, boolean isMultiple) {
        return isMultiple
                ? mContext.getString(
                        R.string.accessibility_undo_multiple_closed_tabs_announcement_message,
                        content)
                : mContext.getString(
                        R.string.accessibility_undo_closed_tab_announcement_message, content);
    }

    /**
     * Calls {@link TabModel#cancelTabClosure(int)} for the tab or for each tab in
     * the list of closed tabs.
     */
    @SuppressWarnings("unchecked")
    @Override
    public void onAction(Object actionData) {
        if (actionData instanceof Integer) {
            cancelTabClosure((Integer) actionData);
        } else {
            for (Tab tab : (List<Tab>) actionData) {
                cancelTabClosure(tab.getId());
            }
            notifyAllTabsClosureUndone();
        }
    }

    private void notifyAllTabsClosureUndone() {
        TabModel model = mTabModelSelector.getCurrentModel();
        if (model != null) model.notifyAllTabsClosureUndone();
    }

    private void cancelTabClosure(int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.cancelTabClosure(tabId);
    }

    /**
     * Calls {@link TabModel#commitTabClosure(int)} for the tab or for each tab in
     * the list of closed tabs.
     */
    @SuppressWarnings("unchecked")
    @Override
    public void onDismissNoAction(Object actionData) {
        if (actionData instanceof Integer) {
            commitTabClosure((Integer) actionData);
        } else {
            for (Tab tab : (List<Tab>) actionData) {
                commitTabClosure(tab.getId());
            }
        }
    }

    private void commitTabClosure(int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.commitTabClosure(tabId);
    }
}
