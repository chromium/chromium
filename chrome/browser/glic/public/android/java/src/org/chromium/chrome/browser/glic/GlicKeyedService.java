// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * GlicKeyedService is the core class for managing Glic flows. It represents a native
 * GlicKeyedService object in Java.
 */
@NullMarked
public interface GlicKeyedService {
    /**
     * Toggles the Glic user interface.
     *
     * @param browserWindowPtr The native pointer (long) to the BrowserWindowInterface.
     * @param preventClose Whether to prevent closing the UI if it's already open.
     * @param profile The {@link Profile} associated with this service instance.
     * @param invocationSource An integer representing the {@code mojom::InvocationSource} mapping
     *     to how the UI was triggered.
     */
    void toggleUI(
            long browserWindowPtr, boolean preventClose, Profile profile, int invocationSource);

    /** Observer for global show/hide events. */
    interface GlobalShowHideObserver {
        /** Called when any Glic instance opens or closes. */
        void onGlobalShowHide();
    }

    /** Adds an observer for global show/hide events. */
    void addGlobalShowHideObserver(GlobalShowHideObserver observer);

    /** Removes an observer for global show/hide events. */
    void removeGlobalShowHideObserver(GlobalShowHideObserver observer);

    /** Observer for user enabled actuation on web changes. */
    interface UserEnabledActuationOnWebObserver {
        void onUserEnabledActuationOnWebChanged(boolean enabled);
    }

    /** Adds an observer for user enabled actuation on web changes. */
    void addUserEnabledActuationOnWebObserver(UserEnabledActuationOnWebObserver observer);

    /** Removes an observer for user enabled actuation on web changes. */
    void removeUserEnabledActuationOnWebObserver(UserEnabledActuationOnWebObserver observer);

    /**
     * Checks if the panel is showing for a specific browser window.
     *
     * @param browserWindowPtr The native pointer (long) to the BrowserWindowInterface.
     * @return true if the panel is showing for the specified browser window.
     */
    boolean isPanelShowingForBrowser(long browserWindowPtr);

    /**
     * Checks if the user has enabled actuation on web.
     *
     * @return true if actuation on web is enabled.
     */
    boolean getUserEnabledActuationOnWeb();

    /**
     * Sets whether the user has enabled actuation on web.
     *
     * @param enabled true to enable actuation on web.
     */
    void setUserEnabledActuationOnWeb(boolean enabled);
}
