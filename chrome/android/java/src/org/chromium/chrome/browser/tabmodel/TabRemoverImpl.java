// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils.GroupsPendingDestroy;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager.MaybeBlockingResult;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabModelRemover.TabModelRemoverFlowHandler;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/**
 * Implementation of {@link TabRemover} for the regular tab model. Uses a {@link TabModelRemover}
 * with appropriate handlers to ensure tab closure and tab removal (for reparenting) do not result
 * in the unintentional destruction of tab groups (particularly for collaborations). See {@link
 * TabModelRemover} for additional details.
 */
@NullMarked
public class TabRemoverImpl implements TabRemover {
    private final TabModelRemover mTabModelRemover;

    /**
     * @param context The activity context.
     * @param modalDialogManager The manager to use for warning dialogs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabRemoverImpl(
            Context context,
            ModalDialogManager modalDialogManager,
            Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        this(new TabModelRemover(context, modalDialogManager, tabGroupModelFilterSupplier));
    }

    @VisibleForTesting
    TabRemoverImpl(TabModelRemover tabModelRemover) {
        mTabModelRemover = tabModelRemover;
    }

    @Override
    public void closeTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        prepareCloseTabs(tabClosureParams, allowDialog, listener, this::forceCloseTabs);
    }

    @Override
    public void prepareCloseTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener,
            Callback<TabClosureParams> onPreparedCallback) {
        CloseTabsHandler closeTabsHandler =
                new CloseTabsHandler(
                        mTabModelRemover.getTabGroupModelFilter(),
                        mTabModelRemover.getActionConfirmationManager(),
                        tabClosureParams,
                        listener,
                        onPreparedCallback);
        mTabModelRemover.doTabRemovalFlow(closeTabsHandler, allowDialog);
    }

    @Override
    public void forceCloseTabs(TabClosureParams tabClosureParams) {
        PassthroughTabRemover.doCloseTabs(
                mTabModelRemover.getTabGroupModelFilter(), tabClosureParams);
    }

    @Override
    public void removeTab(Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {
        assert !allowDialog : "removeTab does not support allowDialog.";

        RemoveTabHandler removeTabHandler =
                new RemoveTabHandler(mTabModelRemover.getTabGroupModelFilter(), tab, listener);
        mTabModelRemover.doTabRemovalFlow(removeTabHandler, /* allowDialog= */ false);
    }

    private static class CloseTabsHandler implements TabModelRemoverFlowHandler {
        private final TabGroupModelFilterInternal mTabGroupModelFilter;
        private final ActionConfirmationManager mActionConfirmationManager;
        private final TabClosureParams mOriginalTabClosureParams;
        private final Callback<TabClosureParams> mCloseTabsCallback;
        private @Nullable TabModelActionListener mListener;
        private @Nullable List<Tab> mPlaceholderTabs;
        private boolean mPreventUndo;

        CloseTabsHandler(
                TabGroupModelFilterInternal tabGroupModelFilter,
                ActionConfirmationManager actionConfirmationManager,
                TabClosureParams originalTabClosureParams,
                @Nullable TabModelActionListener listener,
                Callback<TabClosureParams> closeTabsCallback) {
            mTabGroupModelFilter = tabGroupModelFilter;
            mActionConfirmationManager = actionConfirmationManager;
            mOriginalTabClosureParams = originalTabClosureParams;
            mListener = listener;
            mCloseTabsCallback = closeTabsCallback;
        }

        @Override
        public GroupsPendingDestroy computeGroupsPendingDestroy() {
            return DataSharingTabGroupUtils.getSyncedGroupsDestroyedByTabClosure(
                    mTabGroupModelFilter.getTabModel(), mOriginalTabClosureParams);
        }

        @Override
        public void onPlaceholderTabsCreated(List<Tab> placeholderTabs) {
            mPlaceholderTabs = placeholderTabs;
        }

        @Override
        public void showTabGroupDeletionConfirmationDialog(
                Callback<@ActionConfirmationResult Integer> onResult) {
            boolean isTabGroup = mOriginalTabClosureParams.isTabGroup;
            @Nullable TabModelActionListener listener = takeListener();
            if (listener != null) {
                boolean willSkipDialog =
                        isTabGroup
                                ? mActionConfirmationManager.willSkipDeleteGroupAttempt()
                                : mActionConfirmationManager.willSkipCloseTabAttempt();
                listener.willPerformActionOrShowDialog(DialogType.SYNC, willSkipDialog);
            }
            var adaptedCallback = adaptSyncOnResultCallback(onResult, listener);
            if (isTabGroup) {
                mActionConfirmationManager.processDeleteGroupAttempt(adaptedCallback);
            } else {
                mActionConfirmationManager.processCloseTabAttempt(adaptedCallback);
            }
        }

        @Override
        public void showCollaborationKeepDialog(
                @MemberRole int memberRole, String title, Callback<MaybeBlockingResult> onResult) {
            @Nullable TabModelActionListener listener = takeListener();
            if (listener != null) {
                listener.willPerformActionOrShowDialog(
                        DialogType.COLLABORATION, /* willSkipDialog= */ false);
            }
            var adaptedCallback = adaptCollaborationOnResultCallback(onResult, listener);
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

            @Nullable TabModelActionListener listener = takeListener();
            if (listener != null) {
                listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
            }
            mCloseTabsCallback.onResult(newTabClosureParams);
            if (listener != null) {
                listener.onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
            }
        }

        private @Nullable TabModelActionListener takeListener() {
            TabModelActionListener listener = mListener;
            mListener = null;
            return listener;
        }

        private Callback<MaybeBlockingResult> adaptCollaborationOnResultCallback(
                Callback<MaybeBlockingResult> callback, @Nullable TabModelActionListener listener) {
            return (MaybeBlockingResult maybeBlockingResult) -> {
                callback.onResult(maybeBlockingResult);
                if (listener != null) {
                    listener.onConfirmationDialogResult(
                            DialogType.COLLABORATION, maybeBlockingResult.result);
                }
            };
        }

        private Callback<@ActionConfirmationResult Integer> adaptSyncOnResultCallback(
                Callback<@ActionConfirmationResult Integer> callback,
                @Nullable TabModelActionListener listener) {
            return (@ActionConfirmationResult Integer result) -> {
                boolean isImmediateContinue = result == ActionConfirmationResult.IMMEDIATE_CONTINUE;
                // Sync dialogs interrupt the flow and as such undo operations after the dialog is
                // shown should be suppressed as the user already had an opportunity to abort.
                mPreventUndo = !isImmediateContinue;
                callback.onResult(result);
                if (listener != null) {
                    @DialogType
                    int dialogType = isImmediateContinue ? DialogType.NONE : DialogType.SYNC;
                    listener.onConfirmationDialogResult(dialogType, result);
                }
            };
        }
    }

    private static class RemoveTabHandler implements TabModelRemoverFlowHandler {
        private final TabGroupModelFilter mTabGroupModelFilter;
        private final Tab mTabToRemove;
        private final @Nullable TabModelActionListener mListener;

        RemoveTabHandler(
                TabGroupModelFilter tabGroupModelFilter,
                Tab tabToRemove,
                @Nullable TabModelActionListener listener) {
            mTabGroupModelFilter = tabGroupModelFilter;
            mTabToRemove = tabToRemove;
            mListener = listener;
        }

        @Override
        public GroupsPendingDestroy computeGroupsPendingDestroy() {
            return DataSharingTabGroupUtils.getSyncedGroupsDestroyedByTabRemoval(
                    mTabGroupModelFilter.getTabModel(), Collections.singletonList(mTabToRemove));
        }

        @Override
        public void onPlaceholderTabsCreated(List<Tab> placeholderTabs) {
            // Intentional no-op as there is no possibility to undo this operation so the tabs do
            // not need to be tracked.
        }

        @Override
        public void showTabGroupDeletionConfirmationDialog(Callback<Integer> onResult) {
            assert false : "removeTab does not support tab group deletion dialogs.";

            // This behavior is a safe default even if the assert trips.
            onResult.onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }

        @Override
        public void showCollaborationKeepDialog(
                @MemberRole int memberRole, String title, Callback<MaybeBlockingResult> onResult) {
            assert false : "removeTab does not support collaboration keep dialogs.";

            // This behavior is a safe default even if the assert trips.
            onResult.onResult(
                    new MaybeBlockingResult(ActionConfirmationResult.CONFIRMATION_POSITIVE, null));
        }

        @Override
        public void performAction() {
            TabModel tabModel = mTabGroupModelFilter.getTabModel();
            if (tabModel.getTabById(mTabToRemove.getId()) == null || mTabToRemove.isClosing()) {
                return;
            }
            @Nullable TabModelActionListener listener = mListener;
            if (listener != null) {
                listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
            }
            PassthroughTabRemover.doRemoveTab(tabModel, mTabToRemove);
            if (listener != null) {
                listener.onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
            }
        }
    }

    @VisibleForTesting
    protected static @Nullable TabClosureParams fixupTabClosureParams(
            TabModel tabModel,
            TabClosureParams params,
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
        assumeNonNull(tabsToClose);
        // Before proceeding we need to ensure tabs are not being double closed to avoid crashes
        // and asserts. Any tabs that are closing or cannot be found in the tab model need to be
        // skipped.
        tabsToClose =
                TabModelUtils.getTabsById(
                        TabModelUtils.getTabIds(tabsToClose), tabModel, /* allowClosing= */ false);

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
                        assumeNonNull(placeholderTabs);
                        if (params.undoRunnable != null) {
                            params.undoRunnable.run();
                        }
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
                    .tabClosingSource(params.tabClosingSource)
                    .withUndoRunnable(undoRunnable)
                    .build();
        }
        return TabClosureParams.closeTabs(tabsToClose)
                .allowUndo(params.allowUndo && !preventUndo)
                .hideTabGroups(params.hideTabGroups)
                .saveToTabRestoreService(params.saveToTabRestoreService)
                .tabClosingSource(params.tabClosingSource)
                .withUndoRunnable(undoRunnable)
                .build();
    }
}
