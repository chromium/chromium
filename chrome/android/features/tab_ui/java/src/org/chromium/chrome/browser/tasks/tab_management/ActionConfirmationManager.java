// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.util.Function;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationDialog.ConfirmationDialogResult;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Many tab actions can now cause deletion of tab groups. This class helps orchestrates flows where
 * we might want to warn the user that they're about to delete a tab group.
 */
public class ActionConfirmationManager {
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
     * @param tabGroupModelFilter Used to read tab data.
     * @param modalDialogManager Used to show dialogs.
     */
    public ActionConfirmationManager(
            Profile profile,
            Context context,
            TabGroupModelFilter tabGroupModelFilter,
            ModalDialogManager modalDialogManager) {
        mProfile = profile;
        mContext = context;
        mTabGroupModelFilter = tabGroupModelFilter;
        mModalDialogManager = modalDialogManager;
    }

    /**
     * A close group is an operation on tab group(s), and while it may contain non-grouped tabs, it
     * is not an action on individual tabs within a group.
     */
    public void processDeleteGroupAttempt(Callback<Integer> onResult) {
        processGenericAction(
                Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_CLOSE,
                R.string.delete_tab_group_dialog_title,
                R.string.delete_tab_group_description,
                R.string.delete_tab_group_no_sync_description,
                R.string.delete_tab_group_action,
                onResult);
    }

    /** Ungroup is an action taken on tab groups that ungroups every tab within then, */
    public void processUngroupAttempt(Callback<Integer> onResult) {
        processGenericAction(
                Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_UNGROUP,
                R.string.ungroup_tab_group_dialog_title,
                R.string.ungroup_tab_group_description,
                R.string.ungroup_tab_group_no_sync_description,
                R.string.ungroup_tab_group_action,
                onResult);
    }

    /**
     * Removing tabs is ungrouping through the dialog bottom bar, selecting tabs and ungrouping, or
     * by dragging out of the strip. The list of tabs should all be in the same group.
     */
    public void processRemoveTabAttempt(List<Integer> tabIdList, Callback<Integer> onResult) {
        if (isFullGroup(tabIdList)) {
            processGenericAction(
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
     * This processes closing tabs within groups. Warns when the last tab(s) are being closed. The
     * list of tabs should all be in the same group.
     */
    public void processCloseTabAttempt(List<Integer> tabIdList, Callback<Integer> onResult) {
        if (isFullGroup(tabIdList)) {
            processGenericAction(
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

    private boolean isFullGroup(List<Integer> tabIdList) {
        return tabIdList.size() >= mTabGroupModelFilter.getRelatedTabList(tabIdList.get(0)).size();
    }

    private void processGenericAction(
            String stopShowingPref,
            @StringRes int titleRes,
            @StringRes int withSyncDescriptionRes,
            @StringRes int noSyncDescriptionRes,
            @StringRes int actionRes,
            Callback<Integer> onResult) {

        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        boolean syncingTabGroups =
                syncService.getActiveDataTypes().contains(ModelType.SAVED_TAB_GROUP);
        IdentityServicesProvider identityServicesProvider = IdentityServicesProvider.get();
        IdentityManager identityManager = identityServicesProvider.getIdentityManager(mProfile);
        @Nullable
        CoreAccountInfo coreAccountInfo =
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        final Function<Resources, String> descriptionResolver;
        if (syncingTabGroups && coreAccountInfo != null) {
            descriptionResolver =
                    (resources ->
                            resources.getString(
                                    withSyncDescriptionRes, coreAccountInfo.getEmail()));
        } else {
            descriptionResolver = (resources -> resources.getString(noSyncDescriptionRes));
        }

        PrefService prefService = UserPrefs.get(mProfile);
        if (prefService.getBoolean(stopShowingPref)) {
            onResult.onResult(ConfirmationResult.IMMEDIATE_CONTINUE);
            return;
        }

        ConfirmationDialogResult onDialogResult =
                (shouldCloseTab, resultStopShowing) -> {
                    @ConfirmationResult
                    int result =
                            shouldCloseTab
                                    ? ConfirmationResult.CONFIRMATION_POSITIVE
                                    : ConfirmationResult.CONFIRMATION_NEGATIVE;
                    onResult.onResult(result);
                    if (resultStopShowing) {
                        prefService.setBoolean(stopShowingPref, true);
                    }
                };

        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(mProfile, mContext, mModalDialogManager);
        dialog.show(titleRes, descriptionResolver, actionRes, onDialogResult);
    }

    public static void clearStopShowingPrefsForTesting(PrefService prefService) {
        prefService.clearPref(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_CLOSE);
        prefService.clearPref(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_UNGROUP);
        prefService.clearPref(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE);
        prefService.clearPref(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_CLOSE);
    }
}
