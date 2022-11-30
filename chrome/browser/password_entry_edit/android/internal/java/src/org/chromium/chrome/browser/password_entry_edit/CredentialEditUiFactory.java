// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;

/**
 * Use {@link #create()} to instantiate a {@link CredentialEditCoordinator}.
 */
public class CredentialEditUiFactory {
    /**
     * The factory used to create components that connect to the {@link CredentialEditFragmentView}
     * and provide data.
     */
    interface CreationStrategy {
        /**
         * Creates a component that connects to the given fragment and manipulates its data.
         *
         * @param helpAndFeedbackLauncher launcher for the help center page.
         */
        void create(CredentialEntryFragmentViewBase fragmentView,
                HelpAndFeedbackLauncher helpAndFeedbackLauncher);
    }

    private CredentialEditUiFactory() {}

    private static CreationStrategy sCreationStrategy = (fragmentView, helpAndFeedbackLauncher) -> {
        CredentialEditBridge bridge = CredentialEditBridge.get();
        if (bridge == null) {
            // There is no backend to talk to, so the UI shouldn't be shown.
            fragmentView.dismiss();
            return;
        }

        bridge.initialize(new CredentialEditCoordinator(
                fragmentView, bridge, bridge, helpAndFeedbackLauncher));
    };

    /**
     * Creates the credential edit UI
     *
     * @param fragmentView the view which will be managed by the coordinator.
     */
    public static void create(CredentialEntryFragmentViewBase fragmentView,
            HelpAndFeedbackLauncher helpAndFeedbackLauncher) {
        sCreationStrategy.create(fragmentView, helpAndFeedbackLauncher);
    }

    @VisibleForTesting
    static void setCreationStrategy(CreationStrategy creationStrategy) {
        sCreationStrategy = creationStrategy;
    }
}
