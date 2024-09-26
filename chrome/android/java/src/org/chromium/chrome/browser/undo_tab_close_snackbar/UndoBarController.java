// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import android.content.Context;
import android.content.res.Resources;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

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

import java.util.HashSet;
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
     * each tab in {@code closedTabs}. This will happen unless {@code
     * SnackbarManager#removeFromStackForData(Object)} is called.
     *
     * @param closedTabs A list of tabs that were closed.
     * @param isAllTabs Whether all tabs were closed.
     */
    private void showUndoBar(List<Tab> closedTabs, boolean isAllTabs) {
        if (closedTabs.isEmpty()) return;

        boolean singleTab = closedTabs.size() == 1;
        ClosureMetadata closureMetadata = buildClosureMetadata(closedTabs);
        int umaType = getUmaType(singleTab, closureMetadata.isDeletingTabGroups, isAllTabs);
        Pair<String, String> templateAndContent =
                getTemplateAndContentText(closureMetadata, closedTabs);

        Object actionData = singleTab ? closedTabs.get(0).getId() : closedTabs;

        mSnackbarManagable
                .getSnackbarManager()
                .showSnackbar(
                        Snackbar.make(
                                        templateAndContent.second,
                                        this,
                                        Snackbar.TYPE_ACTION,
                                        umaType)
                                .setDuration(
                                        isAllTabs
                                                ? SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS
                                                : SnackbarManager.DEFAULT_SNACKBAR_DURATION_MS)
                                .setTemplateText(templateAndContent.first)
                                .setAction(mContext.getString(R.string.undo), actionData)
                                .setActionAccessibilityAnnouncement(
                                        getUndoneAccessibilityAnnouncement(
                                                templateAndContent.second, false)));
    }

    private static class ClosureMetadata {
        public final boolean isDeletingTabGroups;
        public final boolean isTabGroupSyncEnabled;
        public final Set<Integer> fullyClosingRootIds;
        public final int ungroupedOrPartialGroupTabs;

        ClosureMetadata(
                boolean isDeletingTabGroups,
                boolean isTabGroupSyncEnabled,
                Set<Integer> fullyClosingRootIds,
                int ungroupedOrPartialGroupTabs) {
            this.isDeletingTabGroups = isDeletingTabGroups;
            this.isTabGroupSyncEnabled = isTabGroupSyncEnabled;
            this.fullyClosingRootIds = fullyClosingRootIds;
            this.ungroupedOrPartialGroupTabs = ungroupedOrPartialGroupTabs;
        }
    }

    private ClosureMetadata buildClosureMetadata(List<Tab> closedTabs) {
        if (closedTabs.isEmpty()) {
            return new ClosureMetadata(
                    /* isDeletingTabGroups= */ false,
                    /* isTabGroupSyncEnabled= */ false,
                    /* fullyClosingRootIds= */ new HashSet<>(),
                    /* ungroupedOrPartialGroupTabs= */ 0);
        }

        assert !closedTabs.get(0).isIncognito();

        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        mTabModelSelector
                                .getTabModelFilterProvider()
                                .getTabModelFilter(/* isIncognito= */ false);
        Profile profile = filter.getTabModel().getProfile();
        boolean tabGroupSyncEnabled =
                profile != null
                        && profile.isNativeInitialized()
                        && TabGroupSyncFeatures.isTabGroupSyncEnabled(profile);

        boolean isDeletingTabGroups = tabGroupSyncEnabled;
        Set<Integer> fullyClosingRootIds = new HashSet<>();
        int ungroupedOrPartialGroupTabs = 0;
        LazyOneshotSupplier<Set<Token>> tabGroupIdsInComprehensiveModel =
                filter.getLazyAllTabGroupIdsInComprehensiveModel(closedTabs);
        for (Tab tab : closedTabs) {
            // We are not deleting a tab group if:
            // 1. Any of the tabs are in a group that is hiding.
            // 2. The comprehensive model still contains tabs with that group ID meaning the tab
            //    group is not being fully deleted as a result of this event.
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) {
                ungroupedOrPartialGroupTabs++;
            } else if (tabGroupSyncEnabled && filter.isTabGroupHiding(tabGroupId)) {
                fullyClosingRootIds.add(tab.getRootId());
                isDeletingTabGroups = false;
            } else if (tabGroupIdsInComprehensiveModel.get().contains(tabGroupId)) {
                ungroupedOrPartialGroupTabs++;
                isDeletingTabGroups = false;
            } else {
                // We are fully deleting any tab group that reaches this point.
                fullyClosingRootIds.add(tab.getRootId());
            }
        }
        return new ClosureMetadata(
                isDeletingTabGroups,
                tabGroupSyncEnabled,
                fullyClosingRootIds,
                ungroupedOrPartialGroupTabs);
    }

    private Pair<String, String> getTemplateAndContentText(
            ClosureMetadata closureMetadata, List<Tab> closedTabs) {
        int totalTabsCount = closedTabs.size();
        int tabGroupsCount = closureMetadata.fullyClosingRootIds.size();
        if (tabGroupsCount == 0) {
            if (closureMetadata.ungroupedOrPartialGroupTabs == 1) {
                return Pair.create(
                        mContext.getString(R.string.undo_bar_close_message),
                        closedTabs.get(0).getTitle());
            } else if (closureMetadata.ungroupedOrPartialGroupTabs > 1) {
                return Pair.create(
                        mContext.getString(R.string.undo_bar_close_all_message),
                        Integer.toString(totalTabsCount));
            } else {
                assert false : "Not reached.";
                return Pair.create("", "");
            }
        } else if (tabGroupsCount == 1) {
            if (closureMetadata.ungroupedOrPartialGroupTabs == 0) {
                int rootId = closureMetadata.fullyClosingRootIds.iterator().next();
                TabGroupModelFilter filter =
                        (TabGroupModelFilter)
                                mTabModelSelector
                                        .getTabModelFilterProvider()
                                        .getTabModelFilter(false);
                @Nullable String tabGroupTitle = filter.getTabGroupTitle(rootId);
                if (tabGroupTitle == null) {
                    tabGroupTitle =
                            mContext.getResources()
                                    .getQuantityString(
                                            R.plurals.bottom_tab_grid_title_placeholder,
                                            totalTabsCount,
                                            totalTabsCount);
                }
                @StringRes int templateRes = Resources.ID_NULL;
                if (closureMetadata.isDeletingTabGroups) {
                    templateRes = R.string.undo_bar_tab_group_deleted_message;
                } else {
                    templateRes =
                            closureMetadata.isTabGroupSyncEnabled
                                    ? R.string.undo_bar_tab_group_closed_and_saved_message
                                    : R.string.undo_bar_tab_group_closed_message;
                }
                return Pair.create(mContext.getString(templateRes), tabGroupTitle);
            }
        }

        // All other strings are some combination of x tab group(s), y tab(s).
        Resources res = mContext.getResources();
        String tabGroupsPart =
                res.getQuantityString(
                        R.plurals.undo_bar_tab_groups_part, tabGroupsCount, tabGroupsCount);
        String tabGroupsAndTabsPart;
        if (closureMetadata.ungroupedOrPartialGroupTabs > 0) {
            tabGroupsAndTabsPart =
                    res.getQuantityString(
                            R.plurals.undo_bar_tab_groups_and_tabs_part,
                            closureMetadata.ungroupedOrPartialGroupTabs,
                            tabGroupsPart,
                            closureMetadata.ungroupedOrPartialGroupTabs);
        } else {
            tabGroupsAndTabsPart = tabGroupsPart;
        }
        @StringRes int templateRes = Resources.ID_NULL;
        if (closureMetadata.isDeletingTabGroups) {
            templateRes = R.string.undo_bar_deleted_message;
        } else {
            templateRes =
                    closureMetadata.isTabGroupSyncEnabled
                            ? R.string.undo_bar_closed_and_saved_message
                            : R.string.undo_bar_closed_message;
        }
        return Pair.create(mContext.getString(templateRes), tabGroupsAndTabsPart);
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
