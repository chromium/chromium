// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.base.Log;

import java.util.Set;

/**
 * Exposes the SafeModeActions for any setup or cleanup work in the non-embedded WebView service.
 *
 * Safemode is considered to be "on" when a non-empty set of Safemode actions is sent to {@link
 * SafeModeService} and at least one of the actions exists within the approved list of safemode
 * actions. Safemode is considered off when all safemode actions are considered off.
 *
 * On a more granular level, each action can be turned off when a new Safemode configuration
 * is sent to {@link SafeModeService} while others are kept on. When each {@link SafeModeAction}'s
 * state changes, this class performs any setup or cleanup required for the action to do its
 * necessary work.
 */
public final class NonEmbeddedSafeModeActionsSetupCleanup {
    // Do not instantiate this class.
    private NonEmbeddedSafeModeActionsSetupCleanup() {}

    private static final String TAG = "SafeModeActionsSetup";

    /**
     * Performs a set of actions to augment any embedded WebView safe mode actions that depend on
     * work in the WebView service.
     * This is to be invoked when safe mode is enabled or disabled in {@link SafeModeService}.
     *
     * @param newActions   A set of named safe mode actions to execute setup/cleanup for.
     *                     Note: The named action must exist in the static list of safe mode
     *                           actions in order for work to be done.
     * @return {@code true} if the actions succeeded cumulatively, {@code false} otherwise.
     */
    public static boolean executeNonEmbeddedActionsOnStateChange(
            Set<String> oldActions, Set<String> newActions) {
        boolean success = true;
        final SafeModeAction[] registeredActions =
                SafeModeController.getInstance().getRegisteredActions();
        if (registeredActions == null) {
            Log.w(
                    TAG,
                    "Must registerActions() before calling executeNonEmbeddedActionsOnStateChange()");
            return false;
        }
        // Activate the new actions that exist in our static list of webview process
        // safe mode actions.
        // Check to see if the old actions are in the list to
        // prevent activating twice.
        for (SafeModeAction currentAction : registeredActions) {
            if (currentAction instanceof NonEmbeddedSafeModeAction) {
                NonEmbeddedSafeModeAction action = (NonEmbeddedSafeModeAction) currentAction;
                boolean oldState = oldActions.contains(action.getId());
                boolean newState = newActions.contains(action.getId());
                if (newState && !oldState) {
                    success &= action.onActivate();
                } else if (!newState && oldState) {
                    success &= action.onDeactivate();
                }
            }
        }
        return success;
    }
}
