// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Token;
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

import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;

/** Implementation of the {@link TabUngrouper} interface. */
@NullMarked
public class TabUngrouperImpl implements TabUngrouper {
    private final TabModelRemover mTabModelRemover;

    /**
     * @param context The activity context.
     * @param modalDialogManager The manager to use for warning dialogs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabUngrouperImpl(
            Context context,
            ModalDialogManager modalDialogManager,
            Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        this(new TabModelRemover(context, modalDialogManager, tabGroupModelFilterSupplier));
    }

    @VisibleForTesting
    TabUngrouperImpl(TabModelRemover tabModelRemover) {
        mTabModelRemover = tabModelRemover;
    }

    @Override
    public void ungroupTabs(
            List<Tab> tabs,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        ungroupTabsInternal(
                (unused) -> tabs, trailing, /* isTabGroup= */ false, allowDialog, listener);
    }

    @Override
    public void ungroupTabGroup(
            Token tabGroupId,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        Function<TabGroupModelFilter, List<Tab>> tabsFetcher =
                (filter) -> PassthroughTabUngrouper.getTabsToUngroup(filter, tabGroupId);

        ungroupTabsInternal(tabsFetcher, trailing, /* isTabGroup= */ true, allowDialog, listener);
    }

    private void ungroupTabsInternal(
            Function<TabGroupModelFilter, List<Tab>> tabsFetcher,
            boolean trailing,
            boolean isTabGroup,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        TabGroupModelFilterInternal filter = mTabModelRemover.getTabGroupModelFilter();
        List<Tab> tabs = tabsFetcher.apply(filter);
        if (tabs == null || tabs.isEmpty()) return;

        UngroupTabsHandler ungroupTabsHandler =
                new UngroupTabsHandler(
                        filter,
                        mTabModelRemover.getActionConfirmationManager(),
                        tabs,
                        trailing,
                        isTabGroup,
                        listener);
        mTabModelRemover.doTabRemovalFlow(ungroupTabsHandler, allowDialog);
    }

    private static class UngroupTabsHandler implements TabModelRemoverFlowHandler {
        private final TabGroupModelFilterInternal mTabGroupModelFilter;
        private final ActionConfirmationManager mActionConfirmationManager;
        private final List<Tab> mTabsToUngroup;
        private final boolean mTrailing;
        private final boolean mIsTabGroup;
        private @Nullable TabModelActionListener mListener;

        UngroupTabsHandler(
                TabGroupModelFilterInternal tabGroupModelFilter,
                ActionConfirmationManager actionConfirmationManager,
                List<Tab> tabsToUngroup,
                boolean trailing,
                boolean isTabGroup,
                @Nullable TabModelActionListener listener) {
            mTabGroupModelFilter = tabGroupModelFilter;
            mActionConfirmationManager = actionConfirmationManager;
            mTabsToUngroup = tabsToUngroup;
            mTrailing = trailing;
            mIsTabGroup = isTabGroup;
            mListener = listener;
        }

        @Override
        public GroupsPendingDestroy computeGroupsPendingDestroy() {
            return DataSharingTabGroupUtils.getSyncedGroupsDestroyedByTabRemoval(
                    mTabGroupModelFilter.getTabModel(), mTabsToUngroup);
        }

        @Override
        public void onPlaceholderTabsCreated(List<Tab> placeholderTabs) {
            // Intentional no-op as there is no possibility to undo this operation so the tabs do
            // not need to be tracked.
        }

        @Override
        public void showTabGroupDeletionConfirmationDialog(
                Callback<@ActionConfirmationResult Integer> onResult) {
            @Nullable TabModelActionListener listener = takeListener();
            if (listener != null) {
                boolean willSkipDialog =
                        mIsTabGroup
                                ? mActionConfirmationManager.willSkipUngroupAttempt()
                                : mActionConfirmationManager.willSkipUngroupTabAttempt();
                listener.willPerformActionOrShowDialog(DialogType.SYNC, willSkipDialog);
            }
            var adaptedCallback = adaptSyncOnResultCallback(onResult, listener);
            if (mIsTabGroup) {
                mActionConfirmationManager.processUngroupAttempt(adaptedCallback);
            } else {
                mActionConfirmationManager.processUngroupTabAttempt(adaptedCallback);
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
            TabGroupModelFilterInternal filter = mTabGroupModelFilter;
            TabModel tabModel = filter.getTabModel();
            List<Tab> newTabsToUngroup =
                    TabModelUtils.getTabsById(
                            TabModelUtils.getTabIds(mTabsToUngroup),
                            tabModel,
                            /* allowClosing= */ false,
                            filter::isTabInTabGroup);

            @Nullable TabModelActionListener listener = takeListener();
            if (listener != null) {
                listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
            }
            PassthroughTabUngrouper.doUngroupTabs(filter, newTabsToUngroup, mTrailing);
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
    }

    private static Callback<MaybeBlockingResult> adaptCollaborationOnResultCallback(
            Callback<MaybeBlockingResult> callback, @Nullable TabModelActionListener listener) {
        return (MaybeBlockingResult maybeBlockingResult) -> {
            callback.onResult(maybeBlockingResult);
            if (listener != null) {
                listener.onConfirmationDialogResult(
                        DialogType.COLLABORATION, maybeBlockingResult.result);
            }
        };
    }

    private static Callback<@ActionConfirmationResult Integer> adaptSyncOnResultCallback(
            Callback<@ActionConfirmationResult Integer> callback,
            @Nullable TabModelActionListener listener) {
        return (@ActionConfirmationResult Integer result) -> {
            callback.onResult(result);
            if (listener != null) {
                @DialogType
                int dialogType =
                        result == ActionConfirmationResult.IMMEDIATE_CONTINUE
                                ? DialogType.NONE
                                : DialogType.SYNC;
                listener.onConfirmationDialogResult(dialogType, result);
            }
        };
    }
}
