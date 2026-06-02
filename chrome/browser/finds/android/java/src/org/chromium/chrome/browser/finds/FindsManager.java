// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Manages the Finds flow on startup. Listens to {@link FindsService} and shows the UI when criteria
 * are fulfilled.
 */
@NullMarked
public class FindsManager implements FindsService.Observer {
    private final Context mContext;
    private final Profile mProfile;
    private final BottomSheetController mBottomSheetController;
    private final SnackbarManager mSnackbarManager;
    private final FindsService mFindsService;

    /**
     * @param context The Activity context.
     * @param profile The {@link Profile} of the current user.
     * @param bottomSheetController The system BottomSheetController.
     * @param snackbarManager The system SnackbarManager.
     * @param findsService The FindsService to observe.
     */
    public FindsManager(
            Context context,
            Profile profile,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager,
            FindsService findsService) {
        mContext = context;
        mProfile = profile;
        mBottomSheetController = bottomSheetController;
        mSnackbarManager = snackbarManager;
        mFindsService = findsService;
        mFindsService.addObserver(this);
        mFindsService.maybeRescheduleNotifications();

        if (FindsFeatures.sAlwaysShowOptInPromo.getValue()) {
            if (!UserPrefs.get(mProfile)
                    .getBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED)) {
                onOptInCriteriaFulfilled();
            }
        }
    }

    /** Unregisters the observer to prevent memory leaks. */
    public void destroy() {
        mFindsService.removeObserver(this);
    }

    @Override
    public void onOptInCriteriaFulfilled() {
        FindsOptInCoordinator coordinator =
                new FindsOptInCoordinator(
                        mContext, mProfile, mBottomSheetController, mSnackbarManager);
        coordinator.showBottomSheet();
    }
}
