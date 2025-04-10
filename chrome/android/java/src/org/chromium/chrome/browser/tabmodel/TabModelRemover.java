// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.components.browser_ui.widget.ActionConfirmationResult.CONFIRMATION_NEGATIVE;
import static org.chromium.components.browser_ui.widget.ActionConfirmationResult.CONFIRMATION_POSITIVE;
import static org.chromium.components.browser_ui.widget.ActionConfirmationResult.IMMEDIATE_CONTINUE;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils.GroupsPendingDestroy;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager.MaybeBlockingResult;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUtils;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/**
 * Common class for performing actions on a {@link TabModel} that will remove tabs or tab groups. If
 * an operation would result in the destruction of a tab group, this class may coordinate with a
 * supplied handler to intervene with a warning dialog and/or the creation of placeholder tabs to
 * ensure the tab group is not unintentionally destroyed.
 */
class TabModelRemover {
    /** Handler implemented by subclasses to provide functions to perform actions. */
    /*package*/ interface TabModelRemoverFlowHandler {
        /** Returns lists of synced and collaboration tab groups destroyed by an operation. */
        @NonNull
        GroupsPendingDestroy computeGroupsPendingDestroy();

        /**
         * Called by {@link TabModelRemover} if it attempted to create any placeholder tabs.
         *
         * @param placeholderTabs The list of created placeholder tabs. This may be an empty list.
         */
        void onPlaceholderTabsCreated(@NonNull List<Tab> placeholderTabs);

        /**
         * Requests to show a dialog to confirm whether tab group deletion is intended. The dialog
         * may be skipped due to user preferences.
         *
         * @param onResult A callback invoked with the {@link ActionConfirmationResult} of showing
         *     the dialog. May be invoked synchronously in some cases.
         */
        void showTabGroupDeletionConfirmationDialog(
                @NonNull Callback<@ActionConfirmationResult Integer> onResult);

        /**
         * Requests to show a dialog asking the user whether to keep the collaboration.
         *
         * @param memberRole The role of the member.
         * @param title The title of the tab group.
         * @param onResult A callback invoked with the {@link MaybeBlockingResult} of showing the
         *     dialog.
         */
        void showCollaborationKeepDialog(
                @MemberRole int memberRole,
                @NonNull String title,
                @NonNull Callback<MaybeBlockingResult> onResult);

        /** Perform the action. */
        void performAction();
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    // Lazily created objects use corresponding getters.
    private @Nullable ActionConfirmationManager mActionConfirmationManager;
    private @Nullable TabGroupSyncService mTabGroupSyncService;
    private @Nullable CollaborationService mCollaborationService;

    /**
     * @param context The activity context.
     * @param modalDialogManager The manager to use for warning dialogs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    /*package*/ TabModelRemover(
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
    }

    /** Returns an {@link ActionConfirmationManager}. */
    /*package*/ @NonNull
    ActionConfirmationManager getActionConfirmationManager() {
        if (mActionConfirmationManager == null) {
            TabGroupModelFilter filter = getTabGroupModelFilter();
            mActionConfirmationManager =
                    new ActionConfirmationManager(
                            filter.getTabModel().getProfile(), mContext, mModalDialogManager);
        }
        return mActionConfirmationManager;
    }

    /** Returns the {@link TabGroupModelFilterInternal} for the regular tab model. */
    /*package*/ @NonNull
    TabGroupModelFilterInternal getTabGroupModelFilter() {
        TabGroupModelFilterInternal filter =
                (TabGroupModelFilterInternal) mTabGroupModelFilterSupplier.get();
        assert filter != null && !filter.getTabModel().isIncognitoBranded();
        return filter;
    }

    /**
     * Performs a tab removal flow using the provided handler.
     *
     * @param handler The {@link TabModelRemoverFlowHandler} to conduct aspects of the removal
     *     operation.
     * @param allowDialg Whether to show dialogs.
     */
    /*package*/ void doTabRemovalFlow(
            @NonNull TabModelRemoverFlowHandler handler, boolean allowDialog) {
        @NonNull GroupsPendingDestroy destroyedGroups = handler.computeGroupsPendingDestroy();

        @NonNull
        List<LocalTabGroupId> collaborationGroupsDestroyed =
                destroyedGroups.collaborationGroupsDestroyed;
        boolean collaborationsDestroyed = !collaborationGroupsDestroyed.isEmpty();
        boolean syncedDestroyed = !destroyedGroups.syncedGroupsDestroyed.isEmpty();

        if (collaborationsDestroyed) {
            // The collaboration dialog specifically makes reference to a single group and the leave
            // or delete group logic is per-group. If more than one group is being destroyed we need
            // to skip the dialog.
            boolean showDialog = allowDialog && collaborationGroupsDestroyed.size() == 1;
            if (showDialog) {
                @NonNull
                CollaborationInfo collaborationInfo =
                        getCollaborationInfo(collaborationGroupsDestroyed.get(0));
                doCollaborationDialogFlow(handler, collaborationInfo, collaborationGroupsDestroyed);
                return;
            } else {
                doCreatePlaceholderTabsInGroups(
                        handler, destroyedGroups.collaborationGroupsDestroyed);
            }
        } else if (syncedDestroyed && allowDialog) {
            handler.showTabGroupDeletionConfirmationDialog(
                    createTabGroupDeletionConfirmationCallback(handler));
            return;
        }

        handler.performAction();
    }

    private List<Tab> doCreatePlaceholderTabsInGroups(
            @NonNull TabModelRemoverFlowHandler handler, @NonNull List<LocalTabGroupId> tabGroups) {
        TabModel model = getTabGroupModelFilter().getTabModel();
        List<Tab> newTabs = DataSharingTabGroupUtils.createPlaceholderTabInGroups(model, tabGroups);
        handler.onPlaceholderTabsCreated(newTabs);
        return newTabs;
    }

    private @NonNull Callback<MaybeBlockingResult> createCollaborationKeepCallback(
            @NonNull CollaborationInfo collaborationInfo) {
        assert collaborationInfo.isValid();
        return (MaybeBlockingResult maybeBlockingResult) -> {
            switch (maybeBlockingResult.result) {
                case CONFIRMATION_POSITIVE:
                    if (maybeBlockingResult.finishBlocking != null) {
                        assert false : "Should not be reachable.";
                        // Do the safe thing and run the runnable anyway.
                        maybeBlockingResult.finishBlocking.run();
                    }
                    return;
                case CONFIRMATION_NEGATIVE:
                    assert maybeBlockingResult.finishBlocking != null;
                    getTabGroupModelFilter().getTabModel().commitAllTabClosures();
                    leaveOrDeleteCollaboration(
                            collaborationInfo, maybeBlockingResult.finishBlocking);
                    return;
                case IMMEDIATE_CONTINUE: // fallthrough
                default:
                    assert false : "Not reached.";
            }
        };
    }

    private @NonNull Callback<Integer> createTabGroupDeletionConfirmationCallback(
            @NonNull TabModelRemoverFlowHandler handler) {
        return (confirmationResult) -> {
            switch (confirmationResult) {
                case IMMEDIATE_CONTINUE: // fallthrough
                case CONFIRMATION_POSITIVE:
                    handler.performAction();
                    return;
                case CONFIRMATION_NEGATIVE:
                    // Intentional no-op.
                    return;
                default:
                    assert false : "Not reached.";
            }
        };
    }

    private void leaveOrDeleteCollaboration(
            @NonNull CollaborationInfo collaborationInfo, @NonNull Runnable finishBlocking) {
        assert collaborationInfo.isValid();

        String collaborationId = collaborationInfo.collaborationId;
        @MemberRole int memberRole = collaborationInfo.memberRole;
        @Nullable CollaborationService collaborationService = getCollaborationService();
        if (collaborationService == null) {
            finishBlocking.run();
            TabUiUtils.showGenericErrorDialog(mContext, mModalDialogManager);
        } else {
            TabUiUtils.exitCollaborationWithoutWarning(
                    mContext,
                    mModalDialogManager,
                    collaborationService,
                    collaborationId,
                    memberRole,
                    finishBlocking);
        }
    }

    /** Contains info about a collaboration. */
    private static class CollaborationInfo {
        public final @MemberRole int memberRole;
        public final String collaborationId;
        public final @NonNull String title;

        CollaborationInfo() {
            this(MemberRole.UNKNOWN, /* collaborationId= */ null, /* title= */ "");
        }

        CollaborationInfo(
                @MemberRole int memberRole, String collaborationId, @NonNull String title) {
            this.memberRole = memberRole;
            this.collaborationId = collaborationId;
            this.title = title;
        }

        boolean isValid() {
            return TabShareUtils.isCollaborationIdValid(this.collaborationId)
                    && memberRole != MemberRole.UNKNOWN;
        }
    }

    private @NonNull CollaborationInfo getCollaborationInfo(
            @NonNull LocalTabGroupId localTabGroupId) {
        @Nullable TabGroupSyncService tabGroupSyncService = getTabGroupSyncService();
        if (tabGroupSyncService == null) {
            return new CollaborationInfo();
        }

        @Nullable SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(localTabGroupId);
        String collaborationId = savedTabGroup != null ? savedTabGroup.collaborationId : null;
        if (!TabShareUtils.isCollaborationIdValid(collaborationId)
                || savedTabGroup.localId == null
                || savedTabGroup.localId.tabGroupId == null) {
            return new CollaborationInfo();
        }

        TabGroupModelFilter filter = getTabGroupModelFilter();
        Token tabGroupId = savedTabGroup.localId.tabGroupId;
        if (!filter.tabGroupExists(tabGroupId)) {
            return new CollaborationInfo();
        }
        String title = TabGroupTitleUtils.getDisplayableTitle(mContext, filter, tabGroupId);

        CollaborationService collaborationService = getCollaborationService();
        @MemberRole
        int memberRole = collaborationService.getCurrentUserRoleForGroup(collaborationId);
        return new CollaborationInfo(memberRole, collaborationId, title);
    }

    private void doCollaborationDialogFlow(
            @NonNull TabModelRemoverFlowHandler handler,
            @NonNull CollaborationInfo collaborationInfo,
            @NonNull List<LocalTabGroupId> collaborationGroupsDestroyed) {
        if (collaborationInfo.isValid()) {
            handler.showCollaborationKeepDialog(
                    collaborationInfo.memberRole,
                    collaborationInfo.title,
                    createCollaborationKeepCallback(collaborationInfo));
        }
        List<Tab> placeholderTabs =
                doCreatePlaceholderTabsInGroups(handler, collaborationGroupsDestroyed);
        // TODO(crbug.com/383509750): Stale data in TabGroupSyncService may cause this assertion to
        // not hold. Restore this assert once fixed.
        // assert placeholderTabs.size() == 1;
        if (!placeholderTabs.isEmpty()) {
            maybeSelectPlaceholderTab(placeholderTabs.get(0));
        }

        handler.performAction();
    }

    /**
     * Selects the placeholder tab if applicable. This requires the placeholder tab to be: 1) in the
     * active tab model, and 2) in a group with the currently selected tab.
     *
     * @param placeholderTab The newly created placeholder tab.
     */
    private void maybeSelectPlaceholderTab(Tab placeholderTab) {
        assert placeholderTab.getTabGroupId() != null;

        TabModel tabModel = getTabGroupModelFilter().getTabModel();
        if (!tabModel.isActiveModel()) return;

        @Nullable Tab currentTab = tabModel.getTabAt(tabModel.index());
        if (currentTab == null) return;

        if (placeholderTab.getTabGroupId().equals(currentTab.getTabGroupId())) {
            // Use FROM_CLOSE since we are going to close/remove the rest of the tabs in the group
            // momentarily and this helps mitigate cases where tab switcher UI may be dismissed.
            tabModel.setIndex(tabModel.indexOf(placeholderTab), TabSelectionType.FROM_CLOSE);
        }
    }

    private @NonNull Profile getProfile() {
        return getTabGroupModelFilter().getTabModel().getProfile();
    }

    private @Nullable TabGroupSyncService getTabGroupSyncService() {
        if (mTabGroupSyncService == null) {
            Profile profile = getProfile();
            mTabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        }
        return mTabGroupSyncService;
    }

    private @NonNull CollaborationService getCollaborationService() {
        if (mCollaborationService == null) {
            Profile profile = getProfile();
            mCollaborationService = CollaborationServiceFactory.getForProfile(profile);
        }
        return mCollaborationService;
    }
}
