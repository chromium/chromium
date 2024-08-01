// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;

/** Use {@link #create()} to instantiate a {@link CredentialEditCoordinator}. */
public class CredentialEditUiFactory {
    /**
     * The factory used to create components that connect to the {@link CredentialEditFragmentView}
     * and provide data.
     */
    interface CreationStrategy {
        /** Creates a component that connects to the given fragment and manipulates its data. */
        void create(CredentialEntryFragmentViewBase fragmentView, Profile profile);
    }

    private CredentialEditUiFactory() {}

    private static CreationStrategy sCreationStrategy =
            (fragmentView, profile) -> {
                CredentialEditBridge bridge = CredentialEditBridge.get();
                if (bridge == null) {
                    // There is no backend to talk to, so the UI shouldn't be shown.
                    fragmentView.dismiss();
                    return;
                }

                bridge.initialize(
                        new CredentialEditCoordinator(profile, fragmentView, bridge, bridge));
            };

    /**
     * Creates the credential edit UI
     *
     * @param fragmentView the view which will be managed by the coordinator.
     */
    public static void create(CredentialEntryFragmentViewBase fragmentView, Profile profile) {
        sCreationStrategy.create(fragmentView, profile);
    }

    @VisibleForTesting
    static void setCreationStrategy(CreationStrategy creationStrategy) {
        sCreationStrategy = creationStrategy;
    }
}
