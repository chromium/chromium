// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils.GroupsPendingDestroy;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabModelRemover.TabModelRemoverFlowHandler;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/**
 * Implementation of {@link TabRemover} for the regular tab model. Uses a {@link TabModelRemover}
 * with appropriate handlers to ensure tab closure and tab removal (for reparenting) do not result
 * in the unintentional destruction of tab groups (particularly for collaborations). See {@link
 * TabModelRemover} for additional details.
 */
public class TabRemoverImpl implements TabRemover {
    private final TabModelRemover mTabModelRemover;

    /**
     * @param context The activity context.
     * @param modalDialogManager The manager to use for warning dialogs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabRemoverImpl(
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        this(new TabModelRemover(context, modalDialogManager, tabGroupModelFilterSupplier));
    }

    @VisibleForTesting
    TabRemoverImpl(@NonNull TabModelRemover tabModelRemover) {
        mTabModelRemover = tabModelRemover;
    }

    @Override
    public void closeTabs(
            @NonNull TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        CloseTabsHandler closeTabsHandler =
                new CloseTabsHandler(
                        mTabModelRemover.getTabGroupModelFilter(),
                        mTabModelRemover.getActionConfirmationManager(),
                        tabClosureParams,
                        listener);
        mTabModelRemover.doTabRemovalFlow(closeTabsHandler, allowDialog);
    }

    @Override
    public void forceCloseTabs(@NonNull TabClosureParams tabClosureParams) {
        PassthroughTabRemover.doCloseTabs(
                mTabModelRemover.getTabGroupModelFilter(), tabClosureParams);
    }

    @Override
    public void removeTab(
            @NonNull Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {
        assert !allowDialog : "removeTab does not support allowDialog.";

        RemoveTabHandler removeTabHandler =
                new RemoveTabHandler(mTabModelRemover.getTabGroupModelFilter(), tab, listener);
        mTabModelRemover.doTabRemovalFlow(removeTabHandler, /* allowDialog= */ false);
    }

    private static class CloseTabsHandler implements TabModelRemoverFlowHandler {
        private final TabGroupModelFilter mTabGroupModelFilter;
        private final ActionConfirmationManager mActionConfirmationManager;
        private final TabClosureParams mOriginalTabClosureParams;
        private @Nullable TabModelActionListener mListener;
        private @Nullable List<Tab> mPlaceholderTabs;
        private boolean mPreventUndo;

        CloseTabsHandler(
                @NonNull TabGroupModelFilter tabGroupModelFilter,
                @NonNull ActionConfirmationManager actionConfirmationManager,
                @NonNull TabClosureParams originalTabClosureParams,
                @Nullable TabModelActionListener listener) {
            mTabGroupModelFilter = tabGroupModelFilter;
            mActionConfirmationManager = actionConfirmationManager;
            mOriginalTabClosureParams = originalTabClosureParams;
            mListener = listener;
        }

        @Override
        public @NonNull GroupsPendingDestroy computeGroupsPendingDestroy() {
            return DataSharingTabGroupUtils.getSyncedGroupsDestroyedByTabClosure(
                    mTabGroupModelFilter.getTabModel(), mOriginalTabClosureParams);
        }

        @Override
        public void onPlaceholderTabsCreated(@NonNull List<Tab> placeholderTabs) {
            mPlaceholderTabs = placeholderTabs;
        }

        @Override
        public void showTabGroupDeletionConfirmationDialog(@NonNull Callback<Integer> onResult) {
            var adaptedCallback = adaptOnResultCallback(onResult, DialogType.SYNC, takeListener());
            if (mOriginalTabClosureParams.isTabGroup) {
                mActionConfirmationManager.processDeleteGroupAttempt(adaptedCallback);
            } else {
                mActionConfirmationManager.processCloseTabAttempt(adaptedCallback);
            }
        }

        @Override
        public void showCollaborationKeepDialog(
                @MemberRole int memberRole, @NonNull String title, Callback<Integer> onResult) {
            var adaptedCallback =
                    adaptOnResultCallback(onResult, DialogType.COLLABORATION, takeListener());
            if (memberRole == MemberRole.OWNER) {
                mActionConfirmationManager.processCollaborationOwnerRemoveLastTab(
                        title, adaptedCallback);
            } else if (memberRole == MemberRole.MEMBER) {
                mActionConfirmationManager.processCollaborationMemberRemoveLastTab(
                        title, adaptedCallback);
            } else {
                assert false : "Not reached";
            }
        }

        @Override
        public void performAction() {
            @Nullable
            TabClosureParams newTabClosureParams =
                    fixupTabClosureParams(
                            mTabGroupModelFilter.getTabModel(),
                            mOriginalTabClosureParams,
                            mPlaceholderTabs,
                            mPreventUndo);
            if (newTabClosureParams == null) return;

            PassthroughTabRemover.doCloseTabs(mTabGroupModelFilter, newTabClosureParams);
            if (mListener != null) {
                mListener.onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
            }
        }

        private @Nullable TabModelActionListener takeListener() {
            TabModelActionListener listener = mListener;
            mListener = null;
            return listener;
        }

        private @NonNull Callback<Integer> adaptOnResultCallback(
                @NonNull Callback<Integer> callback,
                @DialogType int plannedDialogType,
                @Nullable TabModelActionListener listener) {
            return (result) -> {
                boolean isImmediateContinue = result == ActionConfirmationResult.IMMEDIATE_CONTINUE;
                // Sync dialogs interrupt the flow and as such undo operations after the dialog is
                // shown should be suppressed as the user already had an opportunity to abort.
                if (plannedDialogType == DialogType.SYNC) {
                    mPreventUndo = !isImmediateContinue;
                }
                callback.onResult(result);
                if (listener != null) {
                    @DialogType
                    int dialogType = isImmediateContinue ? DialogType.NONE : plannedDialogType;
                    listener.onConfirmationDialogResult(dialogType, result);
                }
            };
        }
    }

    private static class RemoveTabHandler implements TabModelRemoverFlowHandler {
        private final TabGroupModelFilter mTabGroupModelFilter;
        private final Tab mTabToRemove;
        private @Nullable TabModelActionListener mListener;

        RemoveTabHandler(
                @NonNull TabGroupModelFilter tabGroupModelFilter,
                @NonNull Tab tabToRemove,
                @Nullable TabModelActionListener listener) {
            mTabGroupModelFilter = tabGroupModelFilter;
            mTabToRemove = tabToRemove;
            mListener = listener;
        }

        @Override
        public @NonNull GroupsPendingDestroy computeGroupsPendingDestroy() {
            return DataSharingTabGroupUtils.getSyncedGroupsDestroyedByTabRemoval(
                    mTabGroupModelFilter.getTabModel(), Collections.singletonList(mTabToRemove));
        }

        @Override
        public void onPlaceholderTabsCreated(@NonNull List<Tab> placeholderTabs) {
            // Intentional no-op as there is no possibility to undo this operation so the tabs do
            // not need to be tracked.
        }

        @Override
        public void showTabGroupDeletionConfirmationDialog(@NonNull Callback<Integer> onResult) {
            assert false : "removeTab does not support tab group deletion dialogs.";

            // This behavior is a safe default even if the assert trips.
            onResult.onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }

        @Override
        public void showCollaborationKeepDialog(
                @MemberRole int memberRole, @NonNull String title, Callback<Integer> onResult) {
            assert false : "removeTab does not support collaboration keep dialogs.";

            // This behavior is a safe default even if the assert trips.
            onResult.onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        }

        @Override
        public void performAction() {
            TabModel tabModel = mTabGroupModelFilter.getTabModel();
            if (tabModel.getTabById(mTabToRemove.getId()) == null || mTabToRemove.isClosing()) {
                return;
            }
            PassthroughTabRemover.doRemoveTab(tabModel, mTabToRemove);
            if (mListener != null) {
                mListener.onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
            }
        }
    }

    @VisibleForTesting
    protected static @Nullable TabClosureParams fixupTabClosureParams(
            @NonNull TabModel tabModel,
            @NonNull TabClosureParams params,
            @Nullable List<Tab> placeholderTabs,
            boolean preventUndo) {
        boolean createdPlaceholders = placeholderTabs != null && !placeholderTabs.isEmpty();

        boolean isAllTabs = params.tabCloseType == TabCloseType.ALL;
        // If we did not create placeholder tabs and are closing all tabs it is safe to just
        // proceed. There are protections to prevent double closure already in place in TabModel
        // for the all tabs case. It is also safe to ignore `preventUndo` as these operations should
        // always have an undo option.
        if (!createdPlaceholders && isAllTabs) {
            return params;
        }

        List<Tab> tabsToClose = params.tabs;
        if (isAllTabs) {
            // In the case all tabs are closing we need to modify the tab closure params to just
            // close the non-placeholder tabs.
            tabsToClose = TabModelUtils.convertTabListToListOfTabs(tabModel);
            tabsToClose.removeAll(placeholderTabs);
        }
        // Before proceeding we need to ensure tabs are not being double closed to avoid crashes
        // and asserts. Any tabs that are closing or cannot be found in the tab model need to be
        // skipped.
        tabsToClose =
                tabsToClose.stream()
                        .map((tab) -> tabModel.getTabById(tab.getId()))
                        .filter(Objects::nonNull)
                        .filter(tab -> !tab.isClosing())
                        .collect(Collectors.toList());

        // If no tabs remain we will just no-op. This may leave placeholder tabs behind; however,
        // those placeholder tabs may be the only tabs left in the group so do not pre-emptively
        // close them.
        if (tabsToClose.isEmpty()) return null;

        // Respect the undo operation settings of the passed-in caller. However, also ensure that
        // if the operation is undone the placeholder tabs are removed.
        Runnable undoRunnable = params.undoRunnable;
        if (createdPlaceholders) {
            undoRunnable =
                    () -> {
                        params.undoRunnable.run();
                        tabModel.getTabRemover()
                                .forceCloseTabs(
                                        TabClosureParams.closeTabs(placeholderTabs)
                                                .allowUndo(false)
                                                .build());
                    };
        }

        // These calls should be kept up-to-date with any params in TabClosureParams.
        if (params.tabCloseType == TabCloseType.SINGLE) {
            assert tabsToClose.size() == 1;
            return TabClosureParams.closeTab(tabsToClose.get(0))
                    .recommendedNextTab(params.recommendedNextTab)
                    .uponExit(params.uponExit && !createdPlaceholders)
                    .allowUndo(params.allowUndo && !preventUndo)
                    .withUndoRunnable(undoRunnable)
                    .build();
        }
        return TabClosureParams.closeTabs(tabsToClose)
                .allowUndo(params.allowUndo && !preventUndo)
                .hideTabGroups(params.hideTabGroups)
                .saveToTabRestoreService(params.saveToTabRestoreService)
                .withUndoRunnable(undoRunnable)
                .build();
    }
}
