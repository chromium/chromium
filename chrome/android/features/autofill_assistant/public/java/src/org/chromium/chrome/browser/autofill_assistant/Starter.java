// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.metrics.FeatureModuleInstallation;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Connects to a native starter for which it acts as a platform delegate, providing the necessary
 * dependencies to start autofill-assistant flows.
 */
@JNINamespace("autofill_assistant")
public class Starter extends EmptyTabObserver implements UserData {
    /** The tab that this starter tracks. */
    private final Tab mTab;

    /**
     * The WebContents associated with the Tab which this starter is monitoring, unless detached.
     */
    private @Nullable WebContents mWebContents;

    /**
     * The pointer to the native C++ starter. Can be 0 while waiting for the web contents to be
     * available.
     */
    private long mNativeStarter;

    /**
     * Constructs a java-side starter.
     *
     * This will wait for dependencies to become available and then create the native-side starter.
     */
    public Starter(Tab tab) {
        mTab = tab;
        detectWebContentsChange(tab);
    }

    @Override
    public void destroy() {
        safeNativeDetach();
    }

    /**
     * Should be called whenever the Tab's WebContents may have changed. Disconnects from the
     * existing WebContents, if necessary, and then connects to the new WebContents.
     */
    private void detectWebContentsChange(Tab tab) {
        WebContents currentWebContents = tab.getWebContents();
        if (mWebContents != currentWebContents) {
            mWebContents = currentWebContents;
            safeNativeDetach();
            if (mWebContents != null) {
                // TODO(arbesser): retrieve dependencies.
                mNativeStarter = StarterJni.get().fromWebContents(Starter.this, mWebContents);
                // Note: This is intentionally split into two methods (fromWebContents, attach).
                // It ensures that at the time of attach, the native pointer is already set and
                // this instance is ready to serve requests from native.
                StarterJni.get().attach(mNativeStarter, Starter.this);
            }
        }
    }

    @Override
    public void onContentChanged(Tab tab) {
        detectWebContentsChange(tab);
    }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
        detectWebContentsChange(tab);
    }

    @Override
    public void onDestroyed(Tab tab) {
        safeNativeDetach();
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        detectWebContentsChange(tab);
    }

    @Override
    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
        safeNativeOnInteractabilityChanged(isInteractable);
    }

    private void safeNativeDetach() {
        if (mNativeStarter == 0) {
            return;
        }
        StarterJni.get().detach(mNativeStarter, Starter.this);
        mNativeStarter = 0;
    }

    private void safeNativeOnFeatureModuleInstalled(int result) {
        if (mNativeStarter == 0) {
            return;
        }
        StarterJni.get().onFeatureModuleInstalled(mNativeStarter, Starter.this, result);
    }

    private void safeNativeOnInteractabilityChanged(boolean isInteractable) {
        if (mNativeStarter == 0) {
            return;
        }

        StarterJni.get().onInteractabilityChanged(mNativeStarter, Starter.this, isInteractable);
    }

    @CalledByNative
    static boolean getFeatureModuleInstalled() {
        return AutofillAssistantModuleEntryProvider.INSTANCE.isInstalled();
    }

    @CalledByNative
    private void installFeatureModule(boolean showUi) {
        if (getFeatureModuleInstalled()) {
            safeNativeOnFeatureModuleInstalled(FeatureModuleInstallation.DFM_ALREADY_INSTALLED);
            return;
        }

        AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntry(mTab,
                (moduleEntry)
                        -> safeNativeOnFeatureModuleInstalled(moduleEntry != null
                                        ? FeatureModuleInstallation
                                                  .DFM_FOREGROUND_INSTALLATION_SUCCEEDED
                                        : FeatureModuleInstallation
                                                  .DFM_FOREGROUND_INSTALLATION_FAILED),
                showUi);
    }

    @CalledByNative
    private static boolean getIsFirstTimeUser() {
        return AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeTriggerScriptUser();
    }

    @CalledByNative
    private static void setIsFirstTimeUser(boolean firstTimeUser) {
        AutofillAssistantPreferencesUtil.setAutofillAssistantFirstTimeTriggerScriptUser(
                firstTimeUser);
    }

    @CalledByNative
    private static boolean getOnboardingAccepted() {
        return !AutofillAssistantPreferencesUtil.getShowOnboarding();
    }

    @CalledByNative
    private static void setOnboardingAccepted(boolean accepted) {
        AutofillAssistantPreferencesUtil.setInitialPreferences(accepted);
    }

    @CalledByNative
    private void showOnboarding(boolean useDialogOnboarding, String initialUrl,
            String experimentIds, String[] parameterKeys, String[] parameterValues) {
        if (!AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            safeNativeOnOnboardingFinished(
                    /* shown = */ false, 3 /* AssistantOnboardingResult.ACCEPTED*/);
            return;
        }

        // TODO(arbesser): implement this.
        safeNativeOnOnboardingFinished(false, 0 /* AssistantOnboardingResult.DISMISSED*/);
    }

    private void safeNativeOnOnboardingFinished(boolean shown, int result) {
        if (mNativeStarter == 0) {
            return;
        }
        StarterJni.get().onOnboardingFinished(mNativeStarter, Starter.this, shown, result);
    }

    @CalledByNative
    static boolean getProactiveHelpSettingEnabled() {
        return AutofillAssistantPreferencesUtil.isProactiveHelpOn();
    }

    @CalledByNative
    private static void setProactiveHelpSettingEnabled(boolean enabled) {
        AutofillAssistantPreferencesUtil.setProactiveHelpSwitch(enabled);
    }

    @CalledByNative
    static boolean getMakeSearchesAndBrowsingBetterSettingEnabled() {
        // TODO(arbesser): call this from native directly.
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }

    @NativeMethods
    interface Natives {
        long fromWebContents(Starter caller, WebContents webContents);
        void attach(long nativeStarterAndroid, Starter caller);
        void detach(long nativeStarterAndroid, Starter caller);
        void onFeatureModuleInstalled(long nativeStarterAndroid, Starter caller, int result);
        void onOnboardingFinished(
                long nativeStarterAndroid, Starter caller, boolean shown, int result);
        void onInteractabilityChanged(
                long nativeStarterAndroid, Starter caller, boolean isInteractable);
    }
}