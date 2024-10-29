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
import org.chromium.chrome.browser.tabmodel.TabModelRemover.TabModelRemoverFlowHandler;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/** Implementation of the {@link TabUngrouper} interface. */
public class TabUngrouperImpl implements TabUngrouper {
    private final TabModelRemover mTabModelRemover;

    /**
     * @param context The activity context.
     * @param modalDialogManager The manager to use for warning dialogs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabUngrouperImpl(
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        this(new TabModelRemover(context, modalDialogManager, tabGroupModelFilterSupplier));
    }

    @VisibleForTesting
    TabUngrouperImpl(@NonNull TabModelRemover tabModelRemover) {
        mTabModelRemover = tabModelRemover;
    }

    @Override
    public void ungroupTabs(
            @NonNull List<Tab> tabs,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        UngroupTabsHandler ungroupTabsHandler =
                new UngroupTabsHandler(
                        mTabModelRemover.getTabGroupModelFilter(),
                        mTabModelRemover.getActionConfirmationManager(),
                        tabs,
                        trailing,
                        listener);
        mTabModelRemover.doTabRemovalFlow(ungroupTabsHandler, allowDialog);
    }

    private static class UngroupTabsHandler implements TabModelRemoverFlowHandler {
        private final TabGroupModelFilter mTabGroupModelFilter;
        private final ActionConfirmationManager mActionConfirmationManager;
        private final List<Tab> mTabsToUngroup;
        private final boolean mTrailing;
        private @Nullable TabModelActionListener mListener;

        UngroupTabsHandler(
                @NonNull TabGroupModelFilter tabGroupModelFilter,
                @NonNull ActionConfirmationManager actionConfirmationManager,
                @NonNull List<Tab> tabsToUngroup,
                boolean trailing,
                @Nullable TabModelActionListener listener) {
            mTabGroupModelFilter = tabGroupModelFilter;
            mActionConfirmationManager = actionConfirmationManager;
            mTabsToUngroup = tabsToUngroup;
            mTrailing = trailing;
            mListener = listener;
        }

        @Override
        public @NonNull GroupsPendingDestroy computeGroupsPendingDestroy() {
            return DataSharingTabGroupUtils.getSyncedGroupsDestroyedByTabRemoval(
                    mTabGroupModelFilter.getTabModel(), mTabsToUngroup);
        }

        @Override
        public void onPlaceholderTabsCreated(@NonNull List<Tab> placeholderTabs) {
            // Intentional no-op as there is no possibility to undo this operation so the tabs do
            // not need to be tracked.
        }

        @Override
        public void showTabGroupDeletionConfirmationDialog(@NonNull Callback<Integer> onResult) {
            mActionConfirmationManager.processUngroupAttempt(
                    adaptOnResultCallback(onResult, takeListener()));
        }

        @Override
        public void showCollaborationKeepDialog(
                @MemberRole int memberRole, @NonNull String title, Callback<Integer> onResult) {
            if (memberRole == MemberRole.OWNER) {
                mActionConfirmationManager.processCollaborationOwnerRemoveLastTab(
                        title, adaptOnResultCallback(onResult, takeListener()));
            } else if (memberRole == MemberRole.MEMBER) {
                mActionConfirmationManager.processCollaborationMemberRemoveLastTab(
                        title, adaptOnResultCallback(onResult, takeListener()));
            } else {
                assert false : "Not reached";
            }
        }

        @Override
        public void performAction() {
            TabGroupModelFilter filter = mTabGroupModelFilter;
            TabModel tabModel = filter.getTabModel();
            List<Tab> newTabsToUngroup =
                    mTabsToUngroup.stream()
                            .map(tab -> tabModel.getTabById(tab.getId()))
                            .filter(Objects::nonNull)
                            .filter(tab -> !tab.isClosing() && filter.isTabInTabGroup(tab))
                            .collect(Collectors.toList());

            PassthroughTabUngrouper.doUngroupTabs(filter, newTabsToUngroup, mTrailing);
            if (mListener != null) {
                mListener.onConfirmationDialogResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
            }
        }

        private @Nullable TabModelActionListener takeListener() {
            TabModelActionListener listener = mListener;
            mListener = null;
            return listener;
        }
    }

    private static @NonNull Callback<Integer> adaptOnResultCallback(
            @NonNull Callback<Integer> callback, @Nullable TabModelActionListener listener) {
        return (result) -> {
            callback.onResult(result);
            if (listener != null) {
                listener.onConfirmationDialogResult(result);
            }
        };
    }
}
