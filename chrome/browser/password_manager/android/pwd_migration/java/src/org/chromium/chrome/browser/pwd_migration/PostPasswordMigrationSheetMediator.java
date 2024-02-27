// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetProperties.VISIBLE;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the post password migration sheet. It sets the state of the model and
 * reacts to events.
 */
class PostPasswordMigrationSheetMediator {
    private PropertyModel mModel;
    private Profile mProfile;

    void initialize(Profile profile, PropertyModel model) {
        mProfile = profile;
        mModel = model;
    }

    void showSheet() {
        mModel.set(VISIBLE, true);
    }

    void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);

        // Prevent the sheet from reshowing if the user dismissed it or acknowledged it.
        if (shouldDisableShowingTheSheetAtStartup(reason)) {
            PrefService prefService = UserPrefs.get(mProfile);
            prefService.setBoolean(
                    Pref.SHOULD_SHOW_POST_PASSWORD_MIGRATION_SHEET_AT_STARTUP, false);
        }
    }

    PropertyModel getModel() {
        return mModel;
    }

    private boolean shouldDisableShowingTheSheetAtStartup(@StateChangeReason int reason) {
        switch (reason) {
                // The user dismissed the sheet.
            case StateChangeReason.SWIPE:
            case StateChangeReason.BACK_PRESS:
            case StateChangeReason.TAP_SCRIM:
            case StateChangeReason.OMNIBOX_FOCUS:
                // The user acknowledged the sheet.
            case StateChangeReason.NAVIGATION:
                return true;
        }
        return false;
    }
}
