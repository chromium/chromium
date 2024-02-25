// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.password_check.PasswordCheckComponentUi.CustomTabIntentHelper;
import org.chromium.chrome.browser.password_check.PasswordCheckComponentUi.TrustedIntentHelper;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/** Use {@link #create()} to instantiate a {@link PasswordCheckComponentUi}. */
public class PasswordCheckComponentUiFactory {
    /** The factory used to create components that connect to this fragment and provide data. */
    interface CreationStrategy {
        /**
         * Returns a component that connects to the given fragment and manipulates its data.
         * @param fragmentView A {@link PasswordCheckFragmentView}.
         * @param helpAndFeedbackLauncher A {@link HelpAndFeedbackLauncher}.
         * @return A non-null {@link PasswordCheckComponentUi}.
         */
        PasswordCheckComponentUi create(
                PasswordCheckFragmentView fragmentView,
                HelpAndFeedbackLauncher helpAndFeedbackLauncher,
                SettingsLauncher settingsLauncher,
                CustomTabIntentHelper customTabIntentHelper,
                TrustedIntentHelper trustedIntentHelper);
    }

    private static CreationStrategy sCreationStrategy = PasswordCheckCoordinator::new;

    private PasswordCheckComponentUiFactory() {}

    /**
     * Creates a {@link PasswordCheckComponentUi}.
     * @param fragmentView the view which will be managed by the coordinator.
     * @return A {@link PasswordCheckComponentUi}.
     */
    public static PasswordCheckComponentUi create(
            PreferenceFragmentCompat fragmentView,
            HelpAndFeedbackLauncher helpAndFeedbackLauncher,
            SettingsLauncher settingsLauncher,
            CustomTabIntentHelper customTabIntentHelper,
            TrustedIntentHelper trustedIntentHelper) {
        return sCreationStrategy.create(
                (PasswordCheckFragmentView) fragmentView,
                helpAndFeedbackLauncher,
                settingsLauncher,
                customTabIntentHelper,
                trustedIntentHelper);
    }

    @VisibleForTesting
    static void setCreationStrategy(CreationStrategy creationStrategy) {
        sCreationStrategy = creationStrategy;
    }
}
