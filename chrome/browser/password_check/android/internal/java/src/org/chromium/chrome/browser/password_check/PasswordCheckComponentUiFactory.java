// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.password_check.PasswordCheckComponentUi.CustomTabIntentHelper;
import org.chromium.chrome.browser.password_check.PasswordCheckComponentUi.TrustedIntentHelper;
import org.chromium.chrome.browser.profiles.Profile;

/** Use {@link #create()} to instantiate a {@link PasswordCheckComponentUi}. */
public class PasswordCheckComponentUiFactory {
    /** The factory used to create components that connect to this fragment and provide data. */
    interface CreationStrategy {
        /**
         * Returns a component that connects to the given fragment and manipulates its data.
         *
         * @param fragmentView A {@link PasswordCheckFragmentView}.
         * @param profile The {link Profile} associated with the current session.
         * @return A non-null {@link PasswordCheckComponentUi}.
         */
        PasswordCheckComponentUi create(
                PasswordCheckFragmentView fragmentView,
                CustomTabIntentHelper customTabIntentHelper,
                TrustedIntentHelper trustedIntentHelper,
                Profile profile);
    }

    private static CreationStrategy sCreationStrategy = PasswordCheckCoordinator::new;

    private PasswordCheckComponentUiFactory() {}

    /**
     * Creates a {@link PasswordCheckComponentUi}.
     *
     * @param fragmentView the view which will be managed by the coordinator.
     * @param profile The {link Profile} associated with the current session.
     * @return A {@link PasswordCheckComponentUi}.
     */
    public static PasswordCheckComponentUi create(
            PreferenceFragmentCompat fragmentView,
            CustomTabIntentHelper customTabIntentHelper,
            TrustedIntentHelper trustedIntentHelper,
            Profile profile) {
        return sCreationStrategy.create(
                (PasswordCheckFragmentView) fragmentView,
                customTabIntentHelper,
                trustedIntentHelper,
                profile);
    }

    @VisibleForTesting
    static void setCreationStrategy(CreationStrategy creationStrategy) {
        sCreationStrategy = creationStrategy;
    }
}
