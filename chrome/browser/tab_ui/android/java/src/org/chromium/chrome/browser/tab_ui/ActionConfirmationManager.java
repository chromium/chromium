// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.StringRes;
import androidx.core.util.Function;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogHandler;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DialogDismissType;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DismissHandler;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Many tab actions can now cause deletion of tab groups. This class helps orchestrates flows where
 * we might want to warn the user that they're about to delete a tab group. This is currently only
 * used for non-incognito tabs.
 */
@NullMarked
public class ActionConfirmationManager {
    private static final String TAB_GROUP_CONFIRMATION = "TabGroupConfirmation.";
    private static final String DELETE_GROUP_USER_ACTION = TAB_GROUP_CONFIRMATION + "DeleteGroup.";
    private static final String DELETE_SHARED_GROUP_USER_ACTION =
            TAB_GROUP_CONFIRMATION + "DeleteSharedGroup.";
    private static final String UNGROUP_USER_ACTION = TAB_GROUP_CONFIRMATION + "Ungroup.";
    private static final String REMOVE_TAB_FULL_GROUP_USER_ACTION =
            TAB_GROUP_CONFIRMATION + "RemoveTabFullGroup.";
    private static final String CLOSE_TAB_FULL_GROUP_USER_ACTION =
            TAB_GROUP_CONFIRMATION + "CloseTabFullGroup.";
    private static final String LEAVE_GROUP_USER_ACTION = TAB_GROUP_CONFIRMATION + "LeaveGroup.";
    private static final String COLLABORATION_OWNER_REMOVE_LAST_TAB =
            TAB_GROUP_CONFIRMATION + "CollaborationOwnerRemoveLastTab.";
    private static final String COLLABORATION_MEMBER_REMOVE_LAST_TAB =
            TAB_GROUP_CONFIRMATION + "CollaborationMemberRemoveLastTab.";

    private final Profile mProfile;
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;

    /** Holder for the return result from operations that show a blocking modal dialog. */
    public static class MaybeBlockingResult {
        /** The result of the modal dialog. */
        public final @ActionConfirmationResult int result;

        /**
         * A runnable that must be invoked if provided to stop showing the blocking UI when the
         * requisite operation is finished.
         */
        public final @Nullable Runnable finishBlocking;

        /**
         * Constructor is public in the event another API is expecting this type when the action
         * confirmation dialog is not shown. It should not be used widely.
         */
        public MaybeBlockingResult(
                @ActionConfirmationResult int result, @Nullable Runnable finishBlocking) {
            this.result = result;
            this.finishBlocking = finishBlocking;
        }
    }

    /**
     * @param profile The profile to access shared services with.
     * @param context Used to load android resources.
     * @param modalDialogManager Used to show dialogs.
     */
    public ActionConfirmationManager(
            Profile profile, Context context, ModalDialogManager modalDialogManager) {
        assert modalDialogManager != null;
        mProfile = profile;
        mContext = context;
        mModalDialogManager = modalDialogManager;
    }

    /**
     * Process a close tab group operation that would delete the group. This does not include
     * per-tab operations, for that {@see processCloseTabAttempt}.
     */
    public void processDeleteGroupAttempt(Callback<@ActionConfirmationResult Integer> onResult) {
        processMaybeSyncAndPrefAction(
                DELETE_GROUP_USER_ACTION,
                Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_CLOSE,
                R.string.delete_tab_group_dialog_title,
                R.string.delete_tab_group_description,
                R.string.delete_tab_group_no_sync_description,
                R.string.delete_tab_group_action,
                onResult);
    }

    /** Processes deleting a shared group, the user should be the owner. */
    public void processDeleteSharedGroupAttempt(
            String groupTitle, Callback<MaybeBlockingResult> onResult) {
        processExitSharedGroupAction(
                DELETE_SHARED_GROUP_USER_ACTION,
                R.string.delete_shared_tab_group_dialog_title,
                R.string.delete_shared_tab_group_description,
                groupTitle,
                R.string.delete_tab_group_action,
                onResult);
    }

    /** Processing leaving a shared group. */
    public void processLeaveGroupAttempt(
            String groupTitle, Callback<MaybeBlockingResult> onResult) {
        processExitSharedGroupAction(
                LEAVE_GROUP_USER_ACTION,
                R.string.leave_tab_group_dialog_title,
                R.string.leave_tab_group_description,
                groupTitle,
                R.string.keep_tab_group_dialog_leave_action,
                onResult);
    }

    // TODO(crbug.com/362818090): Ensure this is unreachable if the group is shared.
    /** Ungroup is an action taken on tab groups that ungroups every tab within them. */
    public void processUngroupAttempt(Callback<@ActionConfirmationResult Integer> onResult) {
        processMaybeSyncAndPrefAction(
                UNGROUP_USER_ACTION,
                Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_UNGROUP,
                R.string.ungroup_tab_group_dialog_title,
                R.string.ungroup_tab_group_description,
                R.string.ungroup_tab_group_no_sync_description,
                R.string.ungroup_tab_group_action,
                onResult);
    }

    /**
     * Removing tabs either moving to no group or to a different group. The caller needs to ensure
     * this action will delete the group.
     */
    public void processUngroupTabAttempt(Callback<@ActionConfirmationResult Integer> onResult) {
        processMaybeSyncAndPrefAction(
                REMOVE_TAB_FULL_GROUP_USER_ACTION,
                Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE,
                R.string.remove_from_group_dialog_message,
                R.string.remove_from_group_description,
                R.string.delete_tab_group_no_sync_description,
                R.string.delete_tab_group_action,
                onResult);
    }

    /**
     * This processes closing tabs within groups. The caller needs to ensure this action will delete
     * the group.
     */
    public void processCloseTabAttempt(Callback<@ActionConfirmationResult Integer> onResult) {
        processMaybeSyncAndPrefAction(
                CLOSE_TAB_FULL_GROUP_USER_ACTION,
                Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_CLOSE,
                R.string.close_from_group_dialog_title,
                R.string.close_from_group_description,
                R.string.delete_tab_group_no_sync_description,
                R.string.delete_tab_group_action,
                onResult);
    }

    /**
     * Process a remove last tab action (ungroup, close, etc.) for collaboration owner. The caller
     * is responsible for deciding this.
     */
    public void processCollaborationOwnerRemoveLastTab(
            String groupTitle, Callback<MaybeBlockingResult> onResult) {
        processCollaborationTabRemoval(
                COLLABORATION_OWNER_REMOVE_LAST_TAB,
                R.string.keep_tab_group_dialog_title,
                R.string.keep_tab_group_dialog_description_owner,
                groupTitle,
                R.string.keep_tab_group_dialog_keep_action,
                R.string.keep_tab_group_dialog_delete_action,
                onResult);
    }

    /**
     * Process a remove last tab action (ungroup, close, etc.) for collaboration member. The caller
     * is responsible for deciding this.
     */
    public void processCollaborationMemberRemoveLastTab(
            String groupTitle, Callback<MaybeBlockingResult> onResult) {
        processCollaborationTabRemoval(
                COLLABORATION_MEMBER_REMOVE_LAST_TAB,
                R.string.keep_tab_group_dialog_title,
                R.string.keep_tab_group_dialog_description_member,
                groupTitle,
                R.string.keep_tab_group_dialog_keep_action,
                R.string.keep_tab_group_dialog_leave_action,
                onResult);
    }

    private void processMaybeSyncAndPrefAction(
            String userActionBaseString,
            String stopShowingPref,
            @StringRes int titleRes,
            @StringRes int withSyncDescriptionRes,
            @StringRes int noSyncDescriptionRes,
            @StringRes int actionRes,
            Callback<@ActionConfirmationResult Integer> onResult) {

        boolean syncingTabGroups = false;
        @Nullable SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService != null) {
            syncingTabGroups = syncService.getActiveDataTypes().contains(DataType.SAVED_TAB_GROUP);
        }

        @Nullable CoreAccountInfo coreAccountInfo = getCoreAccountInfo();
        final Function<Resources, String> titleResolver = (res) -> res.getString(titleRes);
        final Function<Resources, String> descriptionResolver;
        if (syncingTabGroups && coreAccountInfo != null) {
            descriptionResolver =
                    resources ->
                            resources.getString(withSyncDescriptionRes, coreAccountInfo.getEmail());
        } else {
            descriptionResolver = resources -> resources.getString(noSyncDescriptionRes);
        }

        if (shouldSkipDialog(stopShowingPref)) {
            onResult.onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
            return;
        }

        ConfirmationDialogHandler onDialogInteracted =
                (dismissHandler, buttonClickResult, resultStopShowing) -> {
                    if (resultStopShowing) {
                        RecordUserAction.record(userActionBaseString + "StopShowing");
                        PrefService prefService = UserPrefs.get(mProfile);
                        prefService.setBoolean(stopShowingPref, true);
                    }
                    handleDialogResult(buttonClickResult, userActionBaseString, onResult);
                    return DialogDismissType.DISMISS_IMMEDIATELY;
                };
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                titleResolver,
                descriptionResolver,
                actionRes,
                R.string.cancel,
                /* supportStopShowing= */ true,
                onDialogInteracted);
    }

    private @Nullable CoreAccountInfo getCoreAccountInfo() {
        IdentityServicesProvider identityServicesProvider = IdentityServicesProvider.get();
        @Nullable
        IdentityManager identityManager = identityServicesProvider.getIdentityManager(mProfile);
        if (identityManager != null) {
            return identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        } else {
            return null;
        }
    }

    private void processExitSharedGroupAction(
            String userActionBaseString,
            @StringRes int titleRes,
            @StringRes int descriptionRes,
            String formatArg,
            @StringRes int actionRes,
            Callback<MaybeBlockingResult> onResult) {
        final Function<Resources, String> titleResolver = (res) -> res.getString(titleRes);
        final Function<Resources, String> descriptionResolver =
                resources -> resources.getString(descriptionRes, formatArg);
        ConfirmationDialogHandler onDialogInteracted =
                (dismissHandler, buttonClickResult, resultStopShowing) -> {
                    boolean takePositiveAction = buttonClickResult == ButtonClickResult.POSITIVE;
                    recordProceedOrAbort(userActionBaseString, takePositiveAction);
                    return handleExitCollaborationResult(
                            getActionConfirmationResult(takePositiveAction),
                            takePositiveAction,
                            dismissHandler,
                            buttonClickResult,
                            onResult);
                };
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                titleResolver,
                descriptionResolver,
                actionRes,
                R.string.cancel,
                /* supportStopShowing= */ false,
                onDialogInteracted);
    }

    private static void handleDialogResult(
            @ButtonClickResult int buttonClickResult,
            String userActionBaseString,
            Callback<@ActionConfirmationResult Integer> onResult) {
        boolean takePositiveAction = buttonClickResult == ButtonClickResult.POSITIVE;
        recordProceedOrAbort(userActionBaseString, takePositiveAction);
        onResult.onResult(
                takePositiveAction
                        ? ActionConfirmationResult.CONFIRMATION_POSITIVE
                        : ActionConfirmationResult.CONFIRMATION_NEGATIVE);
    }

    private static @DialogDismissType int handleExitCollaborationResult(
            @ActionConfirmationResult int result,
            boolean blocking,
            DismissHandler dismissHandler,
            @ButtonClickResult int buttonClickResult,
            Callback<MaybeBlockingResult> onResult) {
        if (!blocking) {
            onResult.onResult(new MaybeBlockingResult(result, /* finishBlocking= */ null));
            return DialogDismissType.DISMISS_IMMEDIATELY;
        }

        Runnable finishBlocking = dismissHandler.dismissBlocking(buttonClickResult);
        onResult.onResult(new MaybeBlockingResult(result, finishBlocking));
        return DialogDismissType.DISMISS_LATER;
    }

    private static void recordProceedOrAbort(
            String userActionBaseString, boolean takePositiveAction) {
        RecordUserAction.record(userActionBaseString + (takePositiveAction ? "Proceed" : "Abort"));
    }

    private static @ActionConfirmationResult int getActionConfirmationResult(
            boolean takePositiveAction) {
        return takePositiveAction
                ? ActionConfirmationResult.CONFIRMATION_POSITIVE
                : ActionConfirmationResult.CONFIRMATION_NEGATIVE;
    }

    private void processCollaborationTabRemoval(
            String userActionBaseString,
            @StringRes int titleRes,
            @StringRes int descriptionRes,
            String formatArg,
            @StringRes int positiveButtonRes,
            @StringRes int negativeButtonRes,
            Callback<MaybeBlockingResult> onResult) {
        final Function<Resources, String> titleResolver = (res) -> res.getString(titleRes);
        final Function<Resources, String> descriptionResolver =
                resources -> resources.getString(descriptionRes, formatArg);
        ConfirmationDialogHandler onDialogInteracted =
                (dismissHandler, buttonClickResult, resultStopShowing) -> {
                    boolean takePositiveAction =
                            shouldTakePositiveActionForKeepOrRemoveGroupButtonClick(
                                    buttonClickResult, userActionBaseString);

                    // Invert take positive action for blocking as we want to block on the negative
                    // action (leave/delete). Rather than the positive action (keep).
                    return handleExitCollaborationResult(
                            getActionConfirmationResult(takePositiveAction),
                            !takePositiveAction,
                            dismissHandler,
                            buttonClickResult,
                            onResult);
                };
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                titleResolver,
                descriptionResolver,
                positiveButtonRes,
                negativeButtonRes,
                /* supportStopShowing= */ false,
                onDialogInteracted);
    }

    /**
     * Returns whether to take the positive action for the button click result. Also emits a user
     * action.
     */
    private boolean shouldTakePositiveActionForKeepOrRemoveGroupButtonClick(
            @ButtonClickResult int buttonClickResult, String userActionBaseString) {
        switch (buttonClickResult) {
            case ButtonClickResult.POSITIVE:
                RecordUserAction.record(userActionBaseString + "KeepGroupButton");
                return true;
            case ButtonClickResult.NO_CLICK:
                RecordUserAction.record(userActionBaseString + "KeepGroupImplicit");
                return true;
            case ButtonClickResult.NEGATIVE:
                RecordUserAction.record(userActionBaseString + "RemoveGroup");
                return false;
            default:
                assert false : "Not reached";
                return true;
        }
    }

    /** Returns whether the dialog will be skipped for the close tab attempt. */
    public boolean willSkipCloseTabAttempt() {
        return shouldSkipDialog(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_CLOSE);
    }

    /** Returns whether the dialog will be skipped for the delete group attempt. */
    public boolean willSkipDeleteGroupAttempt() {
        return shouldSkipDialog(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_CLOSE);
    }

    /** Returns whether the dialog will be skipped for the ungroup tab attempt. */
    public boolean willSkipUngroupAttempt() {
        return shouldSkipDialog(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_UNGROUP);
    }

    /** Returns whether the dialog will be skipped for the ungroup attempt. */
    public boolean willSkipUngroupTabAttempt() {
        return shouldSkipDialog(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE);
    }

    private boolean shouldSkipDialog(String stopShowingPref) {
        PrefService prefService = UserPrefs.get(mProfile);
        return prefService.getBoolean(stopShowingPref);
    }

    public static void clearStopShowingPrefsForTesting(PrefService prefService) {
        prefService.clearPref(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_CLOSE);
        prefService.clearPref(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_UNGROUP);
        prefService.clearPref(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE);
        prefService.clearPref(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_CLOSE);
    }

    public static void setAllStopShowingPrefsForTesting(PrefService prefService) {
        prefService.setBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_CLOSE, true);
        prefService.setBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_UNGROUP, true);
        prefService.setBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE, true);
        prefService.setBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_CLOSE, true);
    }
}
