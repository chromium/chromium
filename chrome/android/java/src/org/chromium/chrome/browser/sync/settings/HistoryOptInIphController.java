// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller for the history opt-in help bubble. */
public class HistoryOptInIphController {
    private final UserEducationHelper mUserEducationHelper;
    @Nullable private static HistoryOptInIphController sInstance;

    public static HistoryOptInIphController getInstance(
            @NonNull Activity activity, @NonNull Profile profile) {
        if (sInstance != null) {
            return sInstance;
        }
        return new HistoryOptInIphController(
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())));
    }

    public static void setInstanceForTesting(HistoryOptInIphController historyOptInIphController) {
        var oldInstance = sInstance;
        sInstance = historyOptInIphController;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }

    @VisibleForTesting
    public HistoryOptInIphController(UserEducationHelper userEducationHelper) {
        mUserEducationHelper = userEducationHelper;
    }

    public void showIph(ChromeSwitchPreference preference, View view) {
        // TODO(crbug.com/388201776): Update the string and make it translatable.
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                preference.getContext().getResources(),
                                FeatureConstants.ACCOUNT_SETTINGS_HISTORY_SYNC,
                                R.string.account_section_history_toggle_help,
                                R.string.account_section_history_toggle_help)
                        .setAnchorView(view)
                        .setOnShowCallback(() -> turnOnBackgroundHighlight(preference))
                        .setOnDismissCallback(() -> turnOffBackgroundHighlight(preference))
                        .build());
    }

    private void turnOnBackgroundHighlight(ChromeSwitchPreference preference) {
        preference.setBackgroundColor(
                ChromeSemanticColorUtils.getIphHighlightColor(preference.getContext()));
    }

    private void turnOffBackgroundHighlight(ChromeSwitchPreference preference) {
        preference.clearBackgroundColor();
    }
}
