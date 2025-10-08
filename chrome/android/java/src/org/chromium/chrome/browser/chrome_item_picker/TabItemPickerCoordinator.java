// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/** Provides access to, and management of, Tab data for the {@link ChromeItemPickerActivity}. */
@NullMarked
public class TabItemPickerCoordinator {
    private final int mWindowId;
    private final OneshotSupplier<Profile> mProfileSupplier;
    private final CallbackController mCallbackController;

    public TabItemPickerCoordinator(
            CallbackController callbackController,
            OneshotSupplier<Profile> profileSupplier,
            int windowId) {

        mCallbackController = callbackController;
        mProfileSupplier = profileSupplier;
        mWindowId = windowId;
    }

    /**
     * Initiates the asynchronous loading of the TabModel data and calls the core logic to acquire
     * the TabModelSelector.
     *
     * @param callback The callback to execute once the TabModelSelector is fully initialized (or
     *     null).
     */
    void requestTabModel(Callback<@Nullable TabModelSelector> callback) {
        mProfileSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (profile) -> {
                            if (mWindowId == TabWindowManager.INVALID_WINDOW_ID) {
                                callback.onResult(null);
                                return;
                            }
                            requestTabModelWithProfile(profile, mWindowId, callback);
                        }));
    }

    /**
     * Requests and initializes the headless TabModelSelector instance for a specific window using
     * {@link TabWindowManagerSingleton#requestSelectorWithoutActivity()}to access the list of tabs
     * without requiring a live {@code ChromeTabbedActivity}.
     *
     * @param profile The Profile instance required to scope the tab data.
     * @param windowId The ID of the Chrome window to load the selector for. This ID is used by
     *     {@code requestSelectorWithoutActivity()} to ensure the tab model is loaded and usable,
     *     with or without an activity holding the tab model loaded
     * @param callback The callback to execute once the TabModelSelector is fully initialized.
     */
    @VisibleForTesting
    void requestTabModelWithProfile(
            Profile profile, int windowId, Callback<@Nullable TabModelSelector> callback) {

        // Request the headless TabModelSelector instance.
        @Nullable TabModelSelector selector =
                TabWindowManagerSingleton.getInstance()
                        .requestSelectorWithoutActivity(windowId, profile);

        if (selector == null) {
            callback.onResult(null);
            return;
        }

        // Wait for tab data (state from disk) to be fully initialized.
        TabModelUtils.runOnTabStateInitialized(
                selector,
                mCallbackController.makeCancelable(
                        (@Nullable TabModelSelector s) -> {
                            callback.onResult(s);
                        }));
    }
}
