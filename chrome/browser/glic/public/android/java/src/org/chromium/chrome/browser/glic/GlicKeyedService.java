// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

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
        GlicInvocationSource.NUDGE,
        GlicInvocationSource.THREE_DOTS_MENU,
        GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU,
        GlicInvocationSource.TOOLBAR_BUTTON,
        GlicInvocationSource.MAX_VALUE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface GlicInvocationSource {
        int UNSUPPORTED = 8;
        int TOP_CHROME_BUTTON = 3;
        int NUDGE = 6;
        int THREE_DOTS_MENU = 7;
        int WEB_CONTENTS_CONTEXT_MENU = 23;
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

    // TODO(b/543136256): Consider consolidating these invoke variants into a single method that
    // takes an InvokeOptions param, so the public API scales as more options are added. The JNI
    // layer can keep multiple functions to avoid struct-conversion overhead.
    /**
     * Invokes the Glic service with auto-submit prompt.
     *
     * @param tab The {@link Tab} to target.
     * @param text The text prompt to submit.
     * @param invocationSource How the UI was triggered.
     * @return true if the service was successfully invoked.
     */
    boolean invokeWithAutoSubmit(Tab tab, String text, @GlicInvocationSource int invocationSource);

    /**
     * Invokes the Glic service with a prompt prepopulated in the input box.
     *
     * @param tab The {@link Tab} to target.
     * @param text The text prompt to populate.
     * @param invocationSource How the UI was triggered.
     */
    void invokeWithPrompt(Tab tab, String text, @GlicInvocationSource int invocationSource);

    /**
     * Invokes the Glic service, opening the panel attached to the given tab without
     * auto-submitting.
     *
     * @param tab The {@link Tab} to target.
     * @param invocationSource How the UI was triggered.
     */
    void invoke(Tab tab, @GlicInvocationSource int invocationSource);

    /**
     * Invokes the Glic service with a specific conversation ID.
     *
     * @param tab The {@link Tab} to target, or null if no specific tab.
     * @param glicConversationId The conversation ID to reconnect to.
     * @param invocationSource How the UI was triggered.
     */
    void invokeWithConversation(
            @Nullable Tab tab,
            String glicConversationId,
            @GlicInvocationSource int invocationSource);

    /**
     * Invokes Glic with the given conversation ID if present, switching away from incognito model
     * if needed, and guarding against activity and tab destruction during asynchronous profile
     * resolution.
     *
     * @param activity The host {@link Activity}.
     * @param selector The {@link TabModelSelector} to act on.
     * @param profileProviderSupplier Supplier for {@link ProfileProvider}.
     * @param tab The target {@link Tab}, or null to target the active tab.
     * @param glicConversationId The conversation ID to route to Glic.
     */
    static void maybeInvokeGlic(
            @Nullable Activity activity,
            @Nullable TabModelSelector selector,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @Nullable Tab tab,
            @Nullable String glicConversationId) {
        if (TextUtils.isEmpty(glicConversationId)) {
            return;
        }
        Tab targetTab = tab != null ? tab : (selector != null ? selector.getCurrentTab() : null);
        if (targetTab != null && targetTab.isIncognito() && selector != null) {
            selector.selectModel(/* incognito= */ false);
            targetTab = selector.getCurrentTab();
        }
        if (targetTab == null || targetTab.isIncognito()) {
            return;
        }
        // TODO(b/546096305): Add and use an InvocationSource for Notifications before launch to
        // ensure accurate metrics.
        final Tab finalTargetTab = targetTab;
        profileProviderSupplier.runSyncOrOnAvailable(
                profileProvider -> {
                    if (activity == null
                            || activity.isFinishing()
                            || activity.isDestroyed()
                            || finalTargetTab.isDestroyed()) {
                        return;
                    }
                    GlicKeyedService service =
                            GlicKeyedServiceFactory.getForProfile(
                                    profileProvider.getOriginalProfile());
                    if (service != null) {
                        service.invokeWithConversation(
                                finalTargetTab,
                                glicConversationId,
                                GlicInvocationSource.TOOLBAR_BUTTON);
                    }
                });
    }

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
