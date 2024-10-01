// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Tab-strip specific wrapper for the {@link ActionConfirmationManager} */
public class ActionConfirmationDelegate {
    private ActionConfirmationManager mActionConfirmationManager;
    private View mToolbarContainerView;

    private ObservableSupplierImpl<Integer> mGroupIdToHideSupplier;
    private Profile mProfile;
    private PrefService mPrefService;

    /**
     * @param actionConfirmationManager The {@link ActionConfirmationManager} that is being handled.
     * @param toolbarContainerView The {@link View} that hosts the tab strip's drag and drop events.
     */
    ActionConfirmationDelegate(
            ActionConfirmationManager actionConfirmationManager, View toolbarContainerView) {
        mActionConfirmationManager = actionConfirmationManager;
        mToolbarContainerView = toolbarContainerView;
    }

    /**
     * @param profile The {@link Profile} tied to the given tab strip.
     * @param groupIdToHideSupplier A supplier for the interacting tab group ID. Observed by the tab
     *     strip to hide a given group indicatorwhen showing the action confirmation dialog.
     */
    void initialize(Profile profile, ObservableSupplierImpl<Integer> groupIdToHideSupplier) {
        mProfile = profile;
        mGroupIdToHideSupplier = groupIdToHideSupplier;
    }

    ActionConfirmationManager getActionConfirmationManager() {
        return mActionConfirmationManager;
    }

    /**
     * This method checks if the tab group delete dialog should be shown and temporarily hides the
     * tab group that may be deleted upon user confirmation.
     *
     * @param rootId The root id of the interacting tab.
     * @param draggingLastTabOffStrip Whether the last tab in group is being dragged off strip.
     * @param tabClosing Whether this method is triggered from a tab closure.
     * @param confirmationCallback The callback method to close the tab or move the tab out of group
     *     when user confirms the tab group deletion.
     */
    void handleDeleteGroupAction(
            int rootId,
            boolean draggingLastTabOffStrip,
            boolean tabClosing,
            Runnable confirmationCallback) {
        if (mGroupIdToHideSupplier.get() == Tab.INVALID_TAB_ID) {
            // Hide the tab group.
            mGroupIdToHideSupplier.set(rootId);

            // Show confirmation dialog and handle user response.
            showConfirmationDialogAndHandleResponse(
                    confirmationCallback, draggingLastTabOffStrip, tabClosing);
        }
    }

    /**
     * @return Whether or not we should show the confirmation dialog when a tab is removed from the
     *     group.
     */
    boolean isTabRemoveDialogSkipped() {
        if (mPrefService == null) mPrefService = UserPrefs.get(mProfile);
        return mPrefService.getBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE);
    }

    /**
     * This method prompts a confirmation dialog for deleting the tab group and handles the user
     * response.
     *
     * @param confirmationCallback The callback method to close the last tab or move the last tab
     *     out of the group when the user confirms the tab group deletion.
     * @param dragTabOffStrip Whether the tab is being dragged off tab strip.
     * @param tabClosing Whether this method is triggered from a tab closure.
     */
    private void showConfirmationDialogAndHandleResponse(
            Runnable confirmationCallback, boolean dragTabOffStrip, boolean tabClosing) {
        // Clear any drag and drop in progress to display the dialog.
        if (!isTabRemoveDialogSkipped() && mToolbarContainerView != null) {
            mToolbarContainerView.cancelDragAndDrop();
        }

        // Do not run callback if the call is from tab drag and drop, tab group will be restored
        // if drop is not handled. If the tab drop is handled, the tab group will be deleted
        // when the tab is re-parented, so no action is needed here.
        boolean shouldRunIfImmediateContinue = !dragTabOffStrip || tabClosing;
        Callback<Integer> onResult =
                (@ConfirmationResult Integer result) ->
                        handleUserConfirmation(
                                result, confirmationCallback, shouldRunIfImmediateContinue);

        // Show the delete group dialog for either removing or closing the last tab in the group.
        if (tabClosing) {
            mActionConfirmationManager.processCloseTabAttempt(onResult);
        } else {
            mActionConfirmationManager.processUngroupTabAttempt(onResult);
        }
    }

    /**
     * This method handles the user response for the tab group delete dialog.
     *
     * @param result The integer value representing the user's response on whether to proceed with
     *     deleting the group.
     * @param confirmationCallback The callback method to close the last tab or move the last tab
     *     out of the group when the user confirms the tab group deletion.
     * @param shouldRunIfImmediateContinue Whether to run the callback method when dialog is
     *     skipped.
     */
    private void handleUserConfirmation(
            @ConfirmationResult Integer result,
            Runnable confirmationCallback,
            boolean shouldRunIfImmediateContinue) {
        if ((result == ConfirmationResult.IMMEDIATE_CONTINUE && shouldRunIfImmediateContinue)
                || result == ConfirmationResult.CONFIRMATION_POSITIVE) {
            confirmationCallback.run();
        }
        // Reset the group ID to hide after the group deletion has occurred, so we rebuild the strip
        // with the correct tab state, meaning the indicator won't unexpectedly reappear.
        mGroupIdToHideSupplier.set(Tab.INVALID_TAB_ID);
    }

    void setPrefServiceForTesting(PrefService prefService) {
        mPrefService = prefService;
    }
}
