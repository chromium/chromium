// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.StringRes;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/**
 * A controller that listens to and visually represents cancelable tab closures.
 *
 * <p>Each time a tab is undoably closed via {@link TabModelObserver#onTabClosurePending()},' this
 * controller saves the tab ids and title to the stack of SnackbarManager. It will then let
 * SnackbarManager to show a snackbar representing the top entry in of stack. Each added entry
 * resets the timeout that tracks when to commit the undoable actions.
 *
 * <p>When the undo button is clicked, it will cancel the tab closure if any. All pending closing
 * will be committed.
 *
 * <p>This class also responds to external changes to the undo state by monitoring {@link
 * TabModelObserver#tabClosureUndone(Tab)} and {@link TabModelObserver#tabClosureCommitted(Tab)} to
 * properly keep it's internal state in sync with the model.
 */
@NullMarked
public class TabUndoBarController extends UndoBarController {
    private final TabModelObserver mTabModelObserver;
    private boolean mIsDeletingTabGroups;

    /**
     * Creates an instance of a {@link TabUndoBarController}.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param selector The {@link TabModelSelector} that will be used to commit and undo tab
     *     closures.
     * @param snackbarManageable The holder class to get the manager that helps to show up snackbar.
     * @param dialogVisibilitySupplier The {@link Supplier} to get the visibility of TabGridDialog.
     */
    public TabUndoBarController(
            Context context,
            TabModelSelector selector,
            SnackbarManageable snackbarManageable,
            @Nullable Supplier<Boolean> dialogVisibilitySupplier) {
        super(context, selector, snackbarManageable);
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
                    public void tabClosureUndone(Tab tab) {
                        if (disableUndo(false)) return;
                        dropFromQueue(List.of(tab));
                        mSnackbarManageable
                                .getSnackbarManager()
                                .dismissSnackbars(TabUndoBarController.this, List.of(tab.getId()));
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        if (disableUndo(false)) return;
                        dropFromQueue(List.of(tab));
                        mSnackbarManageable
                                .getSnackbarManager()
                                .dismissSnackbars(TabUndoBarController.this, List.of(tab.getId()));
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        if (disableUndo(false)) return;
                        dropFromQueue(tabs);
                        mSnackbarManageable
                                .getSnackbarManager()
                                .dismissSnackbars(TabUndoBarController.this, tabs);
                    }

                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs,
                            boolean isAllTabs,
                            @TabClosingSource int closingSource) {
                        if (disableUndo(true)) return;
                        queueUndoBar(new TabClosureEvent(tabs, isAllTabs));
                    }

                    @Override
                    public void allTabsClosureCommitted(boolean isIncognito) {
                        if (disableUndo(false)) return;
                        mEventQueue.clear();
                        mSnackbarManageable
                                .getSnackbarManager()
                                .dismissSnackbars(TabUndoBarController.this);
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
     * Cleans up this class, unregistering for application notifications from the {@link
     * TabModelSelector}.
     */
    public void destroy() {
        TabModel model = mTabModelSelector.getModel(false);
        if (model != null) model.removeObserver(mTabModelObserver);
    }

    private static class ClosureMetadata {
        public final boolean isDeletingTabGroups;
        public final boolean isTabGroupSyncEnabled;
        public final Set<Token> fullyClosingTabGroupIds;
        public final int ungroupedOrPartialGroupTabs;

        ClosureMetadata(
                boolean isDeletingTabGroups,
                boolean isTabGroupSyncEnabled,
                Set<Token> fullyClosingTabGroupIds,
                int ungroupedOrPartialGroupTabs) {
            this.isDeletingTabGroups = isDeletingTabGroups;
            this.isTabGroupSyncEnabled = isTabGroupSyncEnabled;
            this.fullyClosingTabGroupIds = fullyClosingTabGroupIds;
            this.ungroupedOrPartialGroupTabs = ungroupedOrPartialGroupTabs;
        }
    }

    private ClosureMetadata buildClosureMetadata(List<Tab> closedTabs) {
        if (closedTabs.isEmpty()) {
            return new ClosureMetadata(
                    /* isDeletingTabGroups= */ false,
                    /* isTabGroupSyncEnabled= */ false,
                    /* fullyClosingTabGroupIds= */ new HashSet<>(),
                    /* ungroupedOrPartialGroupTabs= */ 0);
        }

        assert !closedTabs.get(0).isIncognito();

        TabGroupModelFilter filter =
                assumeNonNull(
                        mTabModelSelector
                                .getTabGroupModelFilterProvider()
                                .getTabGroupModelFilter(/* isIncognito= */ false));
        Profile profile = filter.getTabModel().getProfile();
        boolean tabGroupSyncEnabled =
                profile != null
                        && profile.isNativeInitialized()
                        && TabGroupSyncFeatures.isTabGroupSyncEnabled(profile);

        boolean isDeletingTabGroups = tabGroupSyncEnabled;
        Set<Token> fullyClosingTabGroupIds = new HashSet<>();
        int ungroupedOrPartialGroupTabs = 0;
        LazyOneshotSupplier<Set<Token>> tabGroupIdsInComprehensiveModel =
                filter.getLazyAllTabGroupIds(closedTabs, /* includePendingClosures= */ true);
        for (Tab tab : closedTabs) {
            // We are not deleting a tab group if:
            // 1. Any of the tabs are in a group that is hiding.
            // 2. The comprehensive model still contains tabs with that group ID meaning the tab
            //    group is not being fully deleted as a result of this event.
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) {
                ungroupedOrPartialGroupTabs++;
            } else if (tabGroupSyncEnabled && filter.isTabGroupHiding(tabGroupId)) {
                fullyClosingTabGroupIds.add(tabGroupId);
                isDeletingTabGroups = false;
            } else if (tabGroupIdsInComprehensiveModel.get() != null
                    && tabGroupIdsInComprehensiveModel.get().contains(tabGroupId)) {
                ungroupedOrPartialGroupTabs++;
                isDeletingTabGroups = false;
            } else {
                // We are fully deleting any tab group that reaches this point.
                fullyClosingTabGroupIds.add(tabGroupId);
            }
        }
        return new ClosureMetadata(
                isDeletingTabGroups,
                tabGroupSyncEnabled,
                fullyClosingTabGroupIds,
                ungroupedOrPartialGroupTabs);
    }

    // UndoBarController implementation.

    @Override
    protected Pair<String, String> getTemplateAndContentText(
            List<Tab> closedTabs, List<String> savedTabGroupSyncIds) {
        ClosureMetadata closureMetadata = buildClosureMetadata(closedTabs);
        mIsDeletingTabGroups = closureMetadata.isDeletingTabGroups;

        int totalTabsCount = closedTabs.size();
        int tabGroupsCount = closureMetadata.fullyClosingTabGroupIds.size();
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
                Token tabGroupId = closureMetadata.fullyClosingTabGroupIds.iterator().next();
                Tab groupedTab = null;
                for (Tab tab : closedTabs) {
                    if (tabGroupId.equals(tab.getTabGroupId())) {
                        groupedTab = tab;
                        break;
                    }
                }
                assert groupedTab != null;
                TabGroupModelFilter filter =
                        assumeNonNull(
                                mTabModelSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(false));
                @Nullable String tabGroupTitle = filter.getTabGroupTitle(groupedTab);
                if (TextUtils.isEmpty(tabGroupTitle)) {
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

    @Override
    protected boolean isDeletingTabGroups(List<String> savedTabGroupSyncIds) {
        return mIsDeletingTabGroups;
    }
}
