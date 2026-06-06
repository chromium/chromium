// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * GlicKeyedService is the core class for managing Glic flows. It represents a native
 * GlicKeyedService object in Java.
 */
@NullMarked
public interface GlicKeyedService {
    // LINT.IfChange(GlicInvocationSource)
    // TODO(crbug.com/479863299): Consider using the mojo enum with Java code generation for mojo.
    @IntDef({
        GlicInvocationSource.UNSUPPORTED,
        GlicInvocationSource.TOP_CHROME_BUTTON,
        GlicInvocationSource.THREE_DOTS_MENU,
        GlicInvocationSource.TOOLBAR_BUTTON,
        GlicInvocationSource.MAX_VALUE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface GlicInvocationSource {
        int UNSUPPORTED = 8;
        int TOP_CHROME_BUTTON = 3;
        int THREE_DOTS_MENU = 7;
        int TOOLBAR_BUTTON = 31;
        int MAX_VALUE = 34;
    }

    // LINT.ThenChange(//chrome/browser/glic/host/glic.mojom:InvocationSource)

    /**
     * Toggles the Glic user interface.
     *
     * @param browserWindowPtr The native pointer (long) to the BrowserWindowInterface.
     * @param preventClose Whether to prevent closing the UI if it's already open.
     * @param profile The {@link Profile} associated with this service instance.
     * @param invocationSource How the UI was triggered.
     */
    void toggleUI(
            long browserWindowPtr,
            boolean preventClose,
            Profile profile,
            @GlicInvocationSource int invocationSource);

    /**
     * Invokes the Glic service with auto-submit prompt.
     *
     * @param tab The {@link Tab} to target.
     * @param text The text prompt to submit.
     * @param invocationSource How the UI was triggered.
     * @return true if the service was successfully invoked.
     */
    boolean invokeWithAutoSubmit(Tab tab, String text, @GlicInvocationSource int invocationSource);

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

    /** Observer for allowed changes. */
    interface AllowedChangedObserver {
        void onAllowedStateChanged();
    }

    /** Adds an observer for allowed changes. */
    void addAllowedChangedObserver(AllowedChangedObserver observer);

    /** Removes an observer for allowed changes. */
    void removeAllowedChangedObserver(AllowedChangedObserver observer);

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

    /**
     * Checks if the Glic toolbar button is currently active/pinned.
     *
     * @param profile The current profile.
     * @return true if the Glic toolbar button is active.
     */
    boolean isGlicShortcutActive(Profile profile);

    /**
     * Checks if the bottom bar is enabled.
     *
     * @return true if the bottom bar is enabled.
     */
    boolean isBottomBarEnabled();
}
