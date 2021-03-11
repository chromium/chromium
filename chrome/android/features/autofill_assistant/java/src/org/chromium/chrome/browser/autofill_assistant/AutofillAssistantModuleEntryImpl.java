// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_REQUEST_TRIGGER_SCRIPT;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_STARTED_WITH_TRIGGER_SCRIPT;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_TRIGGER_SCRIPTS_BASE64;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptStarted;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.autofill_assistant.onboarding.AssistantOnboardingResult;
import org.chromium.chrome.browser.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.chrome.browser.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.chrome.browser.autofill_assistant.trigger_scripts.AssistantTriggerScriptBridge;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

import java.util.Map;

/**
 * Implementation of {@link AutofillAssistantModuleEntry}. This is the entry point into the
 * assistant DFM.
 */
@UsedByReflection("AutofillAssistantModuleEntryProvider.java")
public class AutofillAssistantModuleEntryImpl implements AutofillAssistantModuleEntry {
    /** Used for logging. */
    private static final String TAG = "AutofillAssistant";

    @Override
    public void start(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            Context context, @NonNull WebContents webContents,
            ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
            ApplicationViewportInsetSupplier bottomInsetProvider,
            ActivityTabProvider activityTabProvider, boolean isChromeCustomTab,
            @NonNull String initialUrl, Map<String, String> parameters, String experimentIds,
            @Nullable String callerAccount, @Nullable String userName,
            @Nullable String originalDeeplink) {
        if (shouldStartTriggerScript(parameters)) {
            if (!AutofillAssistantPreferencesUtil.isProactiveHelpOn()) {
                // Opt-out users who have disabled the proactive help Chrome setting.
                AutofillAssistantMetrics.recordLiteScriptStarted(
                        webContents, LiteScriptStarted.LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED);
                Log.v(TAG, "TriggerScript stopping: proactive help setting is turned off");
                return;
            }
            if (!TextUtils.isEmpty(parameters.get(PARAMETER_REQUEST_TRIGGER_SCRIPT))
                    && !UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                            AutofillAssistantUiController.getProfile())) {
                // Proactive help that requires communicating with a remote endpoint is tied to the
                // MSBB flag. Even though proactive help is toggled on, we need to stop if MSBB is
                // off. The proactive help setting will appear disabled to the user.
                AutofillAssistantMetrics.recordLiteScriptStarted(
                        webContents, LiteScriptStarted.LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED);
                Log.v(TAG,
                        "TriggerScript stopping: MSBB is turned off, but required by at least one"
                                + " REQUEST_TRIGGER_SCRIPT");
                return;
            }

            boolean isFirstTimeUser =
                    AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeLiteScriptUser();
            AutofillAssistantMetrics.recordLiteScriptStarted(webContents,
                    isFirstTimeUser ? LiteScriptStarted.LITE_SCRIPT_FIRST_TIME_USER
                                    : LiteScriptStarted.LITE_SCRIPT_RETURNING_USER);

            // Start trigger script and transition to regular flow on success.
            if (TextUtils.equals(parameters.get(PARAMETER_REQUEST_TRIGGER_SCRIPT), "true")
                    || !TextUtils.isEmpty(parameters.get(PARAMETER_TRIGGER_SCRIPTS_BASE64))) {
                AssistantTriggerScriptBridge triggerScriptBridge =
                        new AssistantTriggerScriptBridge();
                triggerScriptBridge.start(bottomSheetController, browserControls,
                        compositorViewHolder, context, keyboardVisibilityDelegate,
                        bottomInsetProvider, activityTabProvider, webContents,
                        originalDeeplink != null ? originalDeeplink : initialUrl, parameters,
                        experimentIds, new AssistantTriggerScriptBridge.Delegate() {
                            @Override
                            public void onTriggerScriptFinished(
                                    @LiteScriptFinishedState int finishedState) {
                                if (finishedState
                                        == LiteScriptFinishedState.LITE_SCRIPT_PROMPT_SUCCEEDED) {
                                    parameters.put(PARAMETER_STARTED_WITH_TRIGGER_SCRIPT, "true");
                                    AutofillAssistantClient.fromWebContents(webContents)
                                            .start(initialUrl, parameters, experimentIds,
                                                    callerAccount, userName, isChromeCustomTab,
                                                    triggerScriptBridge.getOnboardingCoordinator());
                                }
                            }
                        });
                return;
            }
        }

        // Regular flow for starting without dedicated trigger script.
        startAutofillAssistantRegular(bottomSheetController, browserControls, compositorViewHolder,
                context, webContents, isChromeCustomTab, initialUrl, parameters, experimentIds,
                callerAccount, userName);
    }

    /** Whether {@code parameters} indicate that a trigger script should be started. */
    private boolean shouldStartTriggerScript(Map<String, String> parameters) {
        return TextUtils.equals(parameters.get(PARAMETER_REQUEST_TRIGGER_SCRIPT), "true")
                || !TextUtils.isEmpty(parameters.get(PARAMETER_TRIGGER_SCRIPTS_BASE64));
    }

    /**
     * Starts a regular autofill assistant script. Shows the onboarding as necessary.
     */
    private void startAutofillAssistantRegular(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            Context context, @NonNull WebContents webContents, boolean isChromeCustomTab,
            @NonNull String initialUrl, Map<String, String> parameters, String experimentIds,
            @Nullable String callerAccount, @Nullable String userName) {
        String intent = parameters.get(BaseOnboardingCoordinator.INTENT_IDENTFIER);
        if (!AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_NOT_SHOWN, intent);
            AutofillAssistantClient.fromWebContents(webContents)
                    .start(initialUrl, parameters, experimentIds, callerAccount, userName,
                            isChromeCustomTab, /* onboardingCoordinator= */ null);
            return;
        }

        BaseOnboardingCoordinator onboardingCoordinator =
                OnboardingCoordinatorFactory.createBottomSheetOnboardingCoordinator(experimentIds,
                        parameters, context, bottomSheetController, browserControls,
                        compositorViewHolder);

        // TODO(b/179648654): Consider to implement |onOnboardingUiChange| inside the coordinator.
        AutofillAssistantClient.onOnboardingUiChange(webContents, /* shown= */ true);
        onboardingCoordinator.show(result -> {
            switch (result) {
                case AssistantOnboardingResult.DISMISSED:
                    AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_NO_ANSWER, intent);
                    AutofillAssistantMetrics.recordDropOut(
                            DropOutReason.ONBOARDING_BACK_BUTTON_CLICKED, intent);
                    break;
                case AssistantOnboardingResult.REJECTED:
                    AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_CANCELLED, intent);
                    AutofillAssistantMetrics.recordDropOut(DropOutReason.DECLINED, intent);
                    break;
                case AssistantOnboardingResult.NAVIGATION:
                    AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_NO_ANSWER, intent);
                    AutofillAssistantMetrics.recordDropOut(
                            DropOutReason.ONBOARDING_NAVIGATION, intent);
                    break;
                case AssistantOnboardingResult.ACCEPTED:
                    AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_ACCEPTED, intent);
                    break;
            }
            AutofillAssistantClient.onOnboardingUiChange(webContents, /* shown= */ false);
            if (result != AssistantOnboardingResult.ACCEPTED) {
                return;
            }
            AutofillAssistantClient.fromWebContents(webContents)
                    .start(initialUrl, parameters, experimentIds, callerAccount, userName,
                            isChromeCustomTab, onboardingCoordinator);
        }, webContents, initialUrl);
    }

    @Override
    public AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            ActivityTabProvider activityTabProvider) {
        return new AutofillAssistantActionHandlerImpl(context, bottomSheetController,
                browserControls, compositorViewHolder, activityTabProvider,
                bottomSheetController.getScrimCoordinator());
    }
}
