// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.metrics.FeatureModuleInstallation;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * Connects to a native starter for which it acts as a platform delegate, providing the necessary
 * dependencies to start autofill-assistant flows.
 */
@JNINamespace("autofill_assistant")
public class Starter extends EmptyTabObserver implements UserData {
    /** The tab that this starter tracks. */
    private final Tab mTab;

    private final AssistantIsGsaFunction mIsGsaFunction;
    private final AssistantIsMsbbEnabledFunction mIsMsbbEnabledFunction;

    /**
     * The WebContents associated with the Tab which this starter is monitoring, unless detached.
     */
    private @Nullable WebContents mWebContents;

    /**
     * The pointer to the native C++ starter. Can be 0 while waiting for the web contents to be
     * available.
     */
    private long mNativeStarter;

    /** The dependencies required to start a flow. */
    @Nullable
    private AssistantDependencies mDependencies;

    /** A helper to show and hide the onboarding. */
    @Nullable
    private AssistantOnboardingHelper mOnboardingHelper;

    /**
     * A field to temporarily hold a startup request's trigger context while the tab is
     * being initialized.
     */
    @Nullable
    private TriggerContext mPendingTriggerContext;

    /**
     * Constructs a java-side starter.
     *
     * This will wait for dependencies to become available and then create the native-side starter.
     */
    public Starter(Tab tab, AssistantIsGsaFunction isGsaFunction,
            AssistantIsMsbbEnabledFunction isMsbbEnabledFunction) {
        mTab = tab;
        mIsGsaFunction = isGsaFunction;
        mIsMsbbEnabledFunction = isMsbbEnabledFunction;
        detectWebContentsChange(tab);
    }

    @Override
    public void destroy() {
        safeNativeDetach();
    }

    /**
     * Attempts to start a new flow for {@code triggerContext}. This will wait for the necessary
     * dependencies (such as the web-contents) to be available before attempting the startup. New
     * calls to this method will supersede earlier invocations, potentially cancelling the previous
     * flow (as there can be only one flow maximum per tab).
     */
    public void start(TriggerContext triggerContext) {
        // Starter is not yet ready, we need to wait for the web-contents to be available.
        if (mNativeStarter == 0) {
            mPendingTriggerContext = triggerContext;
            return;
        }

        StarterJni.get().start(mNativeStarter, Starter.this, triggerContext.getExperimentIds(),
                triggerContext.getParameters().keySet().toArray(new String[0]),
                triggerContext.getParameters().values().toArray(new String[0]),
                triggerContext.getDeviceOnlyParameters().keySet().toArray(new String[0]),
                triggerContext.getDeviceOnlyParameters().values().toArray(new String[0]),
                triggerContext.getInitialUrl());
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
                // Some dependencies are tied to the web-contents and need to be fetched again.
                mDependencies = null;
                mNativeStarter = StarterJni.get().fromWebContents(mWebContents);
                // Note: This is intentionally split into two methods (fromWebContents, attach).
                // It ensures that at the time of attach, the native pointer is already set and
                // this instance is ready to serve requests from native.
                StarterJni.get().attach(mNativeStarter, Starter.this);

                if (mPendingTriggerContext != null) {
                    start(mPendingTriggerContext);
                    mPendingTriggerContext = null;
                }
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
        safeNativeOnActivityAttachmentChanged();
    }

    @Override
    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
        safeNativeOnInteractabilityChanged(isInteractable);
    }

    /**
     * Forces native to re-evaluate the Chrome settings. Integration tests may need to call this to
     * ensure that programmatic updates to the Chrome settings are received by the native starter.
     */
    @VisibleForTesting
    public void forceSettingsChangeNotificationForTesting() {
        safeNativeOnInteractabilityChanged(true);
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

    private void safeNativeOnActivityAttachmentChanged() {
        if (mNativeStarter == 0) {
            return;
        }

        StarterJni.get().onActivityAttachmentChanged(mNativeStarter, Starter.this);
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
        AutofillAssistantPreferencesUtil.setFirstTimeTriggerScriptUserPreference(firstTimeUser);
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
    private void showOnboarding(AssistantOnboardingHelper onboardingHelper,
            boolean useDialogOnboarding, String experimentIds, String[] parameterKeys,
            String[] parameterValues) {
        if (!AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            safeNativeOnOnboardingFinished(
                    /* shown = */ false, 3 /* AssistantOnboardingResult.ACCEPTED*/);
            return;
        }

        assert parameterKeys.length == parameterValues.length;
        Map<String, String> parameters = new HashMap<>();
        for (int i = 0; i < parameterKeys.length; i++) {
            parameters.put(parameterKeys[i], parameterValues[i]);
        }
        onboardingHelper.showOnboarding(useDialogOnboarding, experimentIds, parameters,
                result -> safeNativeOnOnboardingFinished(true, result));
    }

    @CalledByNative
    private void hideOnboarding(AssistantOnboardingHelper onboardingHelper) {
        onboardingHelper.hideOnboarding();
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
        AutofillAssistantPreferencesUtil.setProactiveHelpPreference(enabled);
    }

    @CalledByNative
    private boolean getMakeSearchesAndBrowsingBetterSettingEnabled() {
        return mIsMsbbEnabledFunction.getAsBoolean();
    }

    private AutofillAssistantModuleEntry getModuleOrThrow() {
        if (!getFeatureModuleInstalled()) {
            throw new RuntimeException(
                    "Failed to create dependencies: Feature module not installed");
        }

        return AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntryIfInstalled();
    }

    /**
     * Returns and optionally refreshes the dependencies and the onboarding helper. Since the
     * onboarding helper gets invalidated when the dependencies are invalidated we use the same
     * method to refresh them.
     * */
    @CalledByNative
    private Object[] getOrCreateDependenciesAndOnboardingHelper() {
        if (mDependencies == null) {
            AutofillAssistantModuleEntry module = getModuleOrThrow();
            mDependencies = module.createDependenciesFactory().createDependencies(
                    TabUtils.getActivity(mTab));
            mOnboardingHelper = module.createOnboardingHelper(mWebContents, mDependencies);
        }

        return new Object[] {mDependencies, mOnboardingHelper};
    }

    @CalledByNative
    private boolean getIsTabCreatedByGSA() {
        return mIsGsaFunction.apply(TabUtils.getActivity(mTab));
    }

    @NativeMethods
    interface Natives {
        long fromWebContents(WebContents webContents);
        void attach(long nativeStarterAndroid, Starter caller);
        void detach(long nativeStarterAndroid, Starter caller);
        void onFeatureModuleInstalled(long nativeStarterAndroid, Starter caller, int result);
        void onOnboardingFinished(
                long nativeStarterAndroid, Starter caller, boolean shown, int result);
        void onInteractabilityChanged(
                long nativeStarterAndroid, Starter caller, boolean isInteractable);
        void onActivityAttachmentChanged(long nativeStarterAndroid, Starter caller);
        void start(long nativeStarterAndroid, Starter caller, String experimentIds,
                String[] parameterNames, String[] parameterValues,
                String[] deviceOnlyParameterNames, String[] deviceOnlyParameterValues,
                String initialUrl);
    }
}
