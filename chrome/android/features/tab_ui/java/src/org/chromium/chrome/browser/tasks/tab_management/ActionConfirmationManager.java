// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.util.Function;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationDialog.ConfirmationDialogResult;
import org.chromium.chrome.browser.tasks.tab_management.StrictButtonPressController.ButtonClickResult;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Many tab actions can now cause deletion of tab groups. This class helps orchestrates flows where
 * we might want to warn the user that they're about to delete a tab group. This is currently only
 * used for non-incognito tabs.
 */
public class ActionConfirmationManager {
    private static final String TAB_GROUP_CONFIRMATION = "TabGroupConfirmation.";
    private static final String DELETE_GROUP_USER_ACTION = TAB_GROUP_CONFIRMATION + "DeleteGroup.";
    private static final String DELETE_SHARED_GROUP_USER_ACTION =
            TAB_GROUP_CONFIRMATION + "DeleteSharedGroup.";
    private static final String UNGROUP_USER_ACTION = TAB_GROUP_CONFIRMATION + "Ungroup.";
    private static final String REMOVE_TAB_USER_ACTION = TAB_GROUP_CONFIRMATION + "RemoveTab.";
    private static final String REMOVE_TAB_FULL_GROUP_USER_ACTION =
            TAB_GROUP_CONFIRMATION + "RemoveTabFullGroup.";
    private static final String CLOSE_TAB_USER_ACTION = TAB_GROUP_CONFIRMATION + "CloseTab.";
    private static final String CLOSE_TAB_FULL_GROUP_USER_ACTION =
            TAB_GROUP_CONFIRMATION + "CloseTabFullGroup.";
    private static final String LEAVE_GROUP_USER_ACTION = TAB_GROUP_CONFIRMATION + "LeaveGroup.";
    private static final String COLLABORATION_OWNER_REMOVE_LAST_TAB =
            TAB_GROUP_CONFIRMATION + "CollaborationOwnerRemoveLastTab.";
    private static final String COLLABORATION_MEMBER_REMOVE_LAST_TAB =
            TAB_GROUP_CONFIRMATION + "CollaborationMemberRemoveLastTab.";

    // The result of processing an action.
    @IntDef({
        ConfirmationResult.IMMEDIATE_CONTINUE,
        ConfirmationResult.CONFIRMATION_POSITIVE,
        ConfirmationResult.CONFIRMATION_NEGATIVE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ConfirmationResult {
        // Did not show any confirmation, the action should immediately continue. Resulting action
        // should likely be undoable.
        int IMMEDIATE_CONTINUE = 0;
        // Confirmation was received from the user to continue the action. Do not make resulting
        // action undoable.
        int CONFIRMATION_POSITIVE = 1;
        // The user wants to cancel the action.
        int CONFIRMATION_NEGATIVE = 2;
    }

    private final Profile mProfile;
    private final Context mContext;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final ModalDialogManager mModalDialogManager;

    /**
     * @param profile The profile to access shared services with.
     * @param context Used to load android resources.
     * @param regularTabGroupModelFilter Used to read tab data.
     * @param modalDialogManager Used to show dialogs.
     */
    public ActionConfirmationManager(
            Profile profile,
            Context context,
            TabGroupModelFilter regularTabGroupModelFilter,
            @NonNull ModalDialogManager modalDialogManager) {
        assert modalDialogManager != null;
        mProfile = profile;
        mContext = context;
        assert regularTabGroupModelFilter == null
                || !regularTabGroupModelFilter.isIncognitoBranded();
        mTabGroupModelFilter = regularTabGroupModelFilter;
        mModalDialogManager = modalDialogManager;
    }

    /**
     * A close group is an operation on tab group(s), and while it may contain non-grouped tabs, it
     * is not an action on individual tabs within a group.
     */
    public void processDeleteGroupAttempt(Callback<Integer> onResult) {
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
    public void processDeleteSharedGroupAttempt(String groupTitle, Callback<Integer> onResult) {
        processGroupNameAction(
                DELETE_SHARED_GROUP_USER_ACTION,
                R.string.delete_tab_group_dialog_title,
                R.string.delete_shared_tab_group_description,
                groupTitle,
                R.string.delete_tab_group_menu_item,
                onResult);
    }

    /** Processing leaving a shared group. */
    public void processLeaveGroupAttempt(String groupTitle, Callback<Integer> onResult) {
        processGroupNameAction(
                LEAVE_GROUP_USER_ACTION,
                R.string.leave_tab_group_dialog_title,
                R.string.leave_tab_group_description,
                groupTitle,
                R.string.leave_tab_group_menu_item,
                onResult);
    }

    // TODO(crbug.com/362818090): Ensure this is unreachable if the group is shared.
    /** Ungroup is an action taken on tab groups that ungroups every tab within them. */
    public void processUngroupAttempt(Callback<Integer> onResult) {
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
    public void processUngroupTabAttempt(Callback<Integer> onResult) {
        processMaybeSyncAndPrefAction(
                REMOVE_TAB_USER_ACTION,
                Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE,
                R.string.remove_from_group_dialog_message,
                R.string.remove_from_group_description,
                R.string.delete_tab_group_no_sync_description,
                R.string.delete_tab_group_action,
                onResult);
    }

    // TODO(crbug.com/345854441): Remove this function and create a new helper class that wraps all
    // removal behaviors.
    /**
     * Removing tabs is ungrouping through the dialog bottom bar, selecting tabs and ungrouping, or
     * by dragging out of the strip. The list of tabs should all be in the same group.
     */
    public void processUngroupTabAttempt(List<Integer> tabIdList, Callback<Integer> onResult) {
        if (isFullGroup(tabIdList)) {
            processMaybeSyncAndPrefAction(
                    REMOVE_TAB_FULL_GROUP_USER_ACTION,
                    Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE,
                    R.string.remove_from_group_dialog_message,
                    R.string.remove_from_group_description,
                    R.string.delete_tab_group_no_sync_description,
                    R.string.delete_tab_group_action,
                    onResult);
        } else {
            onResult.onResult(ConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    /**
     * This processes closing tabs within groups. The caller needs to ensure this action will delete
     * the group.
     */
    public void processCloseTabAttempt(Callback<Integer> onResult) {
        processMaybeSyncAndPrefAction(
                CLOSE_TAB_USER_ACTION,
                Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_CLOSE,
                R.string.close_from_group_dialog_title,
                R.string.close_from_group_description,
                R.string.delete_tab_group_no_sync_description,
                R.string.delete_tab_group_action,
                onResult);
    }

    // TODO(crbug.com/345854441): Remove this function and create a new helper class that wraps all
    // removal behaviors.
    /**
     * This processes closing tabs within groups. Warns when the last tab(s) are being closed. The
     * list of tabs should all be in the same group.
     */
    public void processCloseTabAttempt(List<Integer> tabIdList, Callback<Integer> onResult) {
        if (isFullGroup(tabIdList)) {
            processMaybeSyncAndPrefAction(
                    CLOSE_TAB_FULL_GROUP_USER_ACTION,
                    Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_CLOSE,
                    R.string.close_from_group_dialog_title,
                    R.string.close_from_group_description,
                    R.string.delete_tab_group_no_sync_description,
                    R.string.delete_tab_group_action,
                    onResult);
        } else {
            onResult.onResult(ConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    /**
     * Process a remove last tab action (ungroup, close, etc.) for collaboration owner. The caller
     * is responsible for deciding this.
     */
    public void processCollaborationOwnerRemoveLastTab(
            String groupTitle, Callback<Integer> onResult) {
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
            String groupTitle, Callback<Integer> onResult) {
        processCollaborationTabRemoval(
                COLLABORATION_MEMBER_REMOVE_LAST_TAB,
                R.string.keep_tab_group_dialog_title,
                R.string.keep_tab_group_dialog_description_member,
                groupTitle,
                R.string.keep_tab_group_dialog_keep_action,
                R.string.keep_tab_group_dialog_leave_action,
                onResult);
    }

    private boolean isFullGroup(List<Integer> tabIdList) {
        assert mTabGroupModelFilter != null : "TabGroupModelFilter has not been set";
        return tabIdList.size() >= mTabGroupModelFilter.getRelatedTabList(tabIdList.get(0)).size();
    }

    private void processMaybeSyncAndPrefAction(
            String userActionBaseString,
            @Nullable String stopShowingPref,
            @StringRes int titleRes,
            @StringRes int withSyncDescriptionRes,
            @StringRes int noSyncDescriptionRes,
            @StringRes int actionRes,
            Callback<Integer> onResult) {

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

        PrefService prefService = UserPrefs.get(mProfile);
        if (prefService.getBoolean(stopShowingPref)) {
            onResult.onResult(ConfirmationResult.IMMEDIATE_CONTINUE);
            return;
        }

        ConfirmationDialogResult onDialogResult =
                (buttonClickResult, resultStopShowing) -> {
                    if (resultStopShowing) {
                        RecordUserAction.record(userActionBaseString + "StopShowing");
                        prefService.setBoolean(stopShowingPref, true);
                    }
                    handleDialogResult(buttonClickResult, userActionBaseString, onResult);
                };
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                titleResolver,
                descriptionResolver,
                actionRes,
                R.string.cancel,
                /* supportStopShowing= */ true,
                onDialogResult);
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

    private void processGroupNameAction(
            String userActionBaseString,
            @StringRes int titleRes,
            @StringRes int descriptionRes,
            String formatArg,
            @StringRes int actionRes,
            Callback<Integer> onResult) {
        final Function<Resources, String> titleResolver = (res) -> res.getString(titleRes);
        final Function<Resources, String> descriptionResolver =
                resources -> resources.getString(descriptionRes, formatArg);
        ConfirmationDialogResult onDialogResult =
                (buttonClickResult, resultStopShowing) ->
                        handleDialogResult(buttonClickResult, userActionBaseString, onResult);
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                titleResolver,
                descriptionResolver,
                actionRes,
                R.string.cancel,
                /* supportStopShowing= */ false,
                onDialogResult);
    }

    private void handleDialogResult(
            @ButtonClickResult int buttonClickResult,
            String userActionBaseString,
            Callback<Integer> onResult) {
        boolean takePositiveAction = buttonClickResult == ButtonClickResult.POSITIVE;
        RecordUserAction.record(userActionBaseString + (takePositiveAction ? "Proceed" : "Abort"));
        onResult.onResult(
                takePositiveAction
                        ? ConfirmationResult.CONFIRMATION_POSITIVE
                        : ConfirmationResult.CONFIRMATION_NEGATIVE);
    }

    private void processCollaborationTabRemoval(
            String userActionBaseString,
            @StringRes int titleRes,
            @StringRes int descriptionRes,
            String formatArg,
            @StringRes int positiveButtonRes,
            @StringRes int negativeButtonRes,
            Callback<Integer> onResult) {
        final Function<Resources, String> titleResolver = (res) -> res.getString(titleRes);
        final Function<Resources, String> descriptionResolver =
                resources -> resources.getString(descriptionRes, formatArg);
        ConfirmationDialogResult onDialogResult =
                (buttonClickResult, resultStopShowing) ->
                        handleCollaborationDialogResult(
                                buttonClickResult, userActionBaseString, onResult);
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mContext, mModalDialogManager);
        dialog.show(
                titleResolver,
                descriptionResolver,
                positiveButtonRes,
                negativeButtonRes,
                /* supportStopShowing= */ false,
                onDialogResult);
    }

    private void handleCollaborationDialogResult(
            @ButtonClickResult int buttonClickResult,
            String userActionBaseString,
            Callback<Integer> onResult) {
        boolean takePositiveAction =
                shouldTakePositiveActionForCollaborationButtonClick(
                        buttonClickResult, userActionBaseString);
        onResult.onResult(
                takePositiveAction
                        ? ConfirmationResult.CONFIRMATION_POSITIVE
                        : ConfirmationResult.CONFIRMATION_NEGATIVE);
    }

    /**
     * Returns whether to take the positive action for the button click result. Also emits a user
     * action.
     */
    private boolean shouldTakePositiveActionForCollaborationButtonClick(
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
