// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.TriggerContext.PARAMETER_REQUEST_TRIGGER_SCRIPT;
import static org.chromium.chrome.browser.autofill_assistant.TriggerContext.PARAMETER_STARTED_WITH_TRIGGER_SCRIPT;
import static org.chromium.chrome.browser.autofill_assistant.TriggerContext.PARAMETER_TRIGGER_SCRIPTS_BASE64;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;

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
    public AssistantDependencies createDependencies(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            Context context, @NonNull WebContents webContents,
            ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
            ApplicationViewportInsetSupplier bottomInsetProvider,
            ActivityTabProvider activityTabProvider) {
        return new AssistantDependenciesImpl(bottomSheetController, browserControls,
                compositorViewHolder, context, webContents, keyboardVisibilityDelegate,
                bottomInsetProvider, activityTabProvider);
    }

    @Override
    public void start(AssistantDependencies assistantDependencies, TriggerContext triggerContext) {
        AssistantDependenciesImpl dependencies = (AssistantDependenciesImpl) assistantDependencies;
        if (shouldStartTriggerScript(triggerContext.getParameters())) {
            if (!AutofillAssistantPreferencesUtil.isProactiveHelpOn()) {
                // Opt-out users who have disabled the proactive help Chrome setting.
                AutofillAssistantMetrics.recordLiteScriptStarted(dependencies.getWebContents(),
                        LiteScriptStarted.LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED);
                Log.v(TAG, "TriggerScript stopping: proactive help setting is turned off");
                return;
            }
            if (!TextUtils.isEmpty(
                        triggerContext.getParameters().get(PARAMETER_REQUEST_TRIGGER_SCRIPT))
                    && !UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                            AutofillAssistantUiController.getProfile())) {
                // Proactive help that requires communicating with a remote endpoint is tied to the
                // MSBB flag. Even though proactive help is toggled on, we need to stop if MSBB is
                // off. The proactive help setting will appear disabled to the user.
                AutofillAssistantMetrics.recordLiteScriptStarted(dependencies.getWebContents(),
                        LiteScriptStarted.LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED);
                Log.v(TAG,
                        "TriggerScript stopping: MSBB is turned off, but required by at least one"
                                + " REQUEST_TRIGGER_SCRIPT");
                return;
            }

            boolean isFirstTimeUser = AutofillAssistantPreferencesUtil
                                              .isAutofillAssistantFirstTimeTriggerScriptUser();
            AutofillAssistantMetrics.recordLiteScriptStarted(dependencies.getWebContents(),
                    isFirstTimeUser ? LiteScriptStarted.LITE_SCRIPT_FIRST_TIME_USER
                                    : LiteScriptStarted.LITE_SCRIPT_RETURNING_USER);

            // Start trigger script and transition to regular flow on success.
            if (TextUtils.equals(
                        triggerContext.getParameters().get(PARAMETER_REQUEST_TRIGGER_SCRIPT),
                        "true")
                    || !TextUtils.isEmpty(
                            triggerContext.getParameters().get(PARAMETER_TRIGGER_SCRIPTS_BASE64))) {
                dependencies.getTriggerScriptBridge().start(triggerContext, finishedState -> {
                    if (finishedState == LiteScriptFinishedState.LITE_SCRIPT_PROMPT_SUCCEEDED) {
                        triggerContext.getParameters().put(
                                PARAMETER_STARTED_WITH_TRIGGER_SCRIPT, "true");
                        AutofillAssistantClient.fromWebContents(dependencies.getWebContents())
                                .start(triggerContext,
                                        dependencies.transferOnboardingOverlayCoordinator());
                    }
                });
                return;
            }
        }

        // Regular flow for starting without dedicated trigger script.
        startAutofillAssistantRegular(dependencies, triggerContext);
    }

    /** Whether {@code parameters} indicate that a trigger script should be started. */
    private boolean shouldStartTriggerScript(Map<String, String> parameters) {
        return TextUtils.equals(parameters.get(PARAMETER_REQUEST_TRIGGER_SCRIPT), "true")
                || !TextUtils.isEmpty(parameters.get(PARAMETER_TRIGGER_SCRIPTS_BASE64));
    }

    /**
     * Starts a regular autofill assistant script. Shows the onboarding as necessary.
     */
    private void startAutofillAssistantRegular(
            AssistantDependenciesImpl startupDependencies, TriggerContext triggerContext) {
        String intent =
                triggerContext.getParameters().get(BaseOnboardingCoordinator.INTENT_IDENTFIER);
        if (!AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_NOT_SHOWN, intent);
            AutofillAssistantClient.fromWebContents(startupDependencies.getWebContents())
                    .start(triggerContext,
                            startupDependencies.transferOnboardingOverlayCoordinator());
            return;
        }

        // TODO(b/179648654): Consider to implement |onOnboardingUiChange| inside the coordinator.
        AutofillAssistantClient.onOnboardingUiChange(
                startupDependencies.getWebContents(), /* shown= */ true);
        startupDependencies.showOnboarding(
                /* useDialogOnboarding = */ false, triggerContext, result -> {
                    switch (result) {
                        case AssistantOnboardingResult.DISMISSED:
                            AutofillAssistantMetrics.recordOnBoarding(
                                    OnBoarding.OB_NO_ANSWER, intent);
                            AutofillAssistantMetrics.recordDropOut(
                                    DropOutReason.ONBOARDING_BACK_BUTTON_CLICKED, intent);
                            break;
                        case AssistantOnboardingResult.REJECTED:
                            AutofillAssistantMetrics.recordOnBoarding(
                                    OnBoarding.OB_CANCELLED, intent);
                            AutofillAssistantMetrics.recordDropOut(DropOutReason.DECLINED, intent);
                            break;
                        case AssistantOnboardingResult.NAVIGATION:
                            AutofillAssistantMetrics.recordOnBoarding(
                                    OnBoarding.OB_NO_ANSWER, intent);
                            AutofillAssistantMetrics.recordDropOut(
                                    DropOutReason.ONBOARDING_NAVIGATION, intent);
                            break;
                        case AssistantOnboardingResult.ACCEPTED:
                            AutofillAssistantMetrics.recordOnBoarding(
                                    OnBoarding.OB_ACCEPTED, intent);
                            break;
                    }
                    AutofillAssistantClient.onOnboardingUiChange(
                            startupDependencies.getWebContents(), /* shown= */ false);
                    if (result != AssistantOnboardingResult.ACCEPTED) {
                        return;
                    }
                    AutofillAssistantClient.fromWebContents(startupDependencies.getWebContents())
                            .start(triggerContext,
                                    startupDependencies.transferOnboardingOverlayCoordinator());
                });
    }

    @Override
    public AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            ActivityTabProvider activityTabProvider) {
        return new AutofillAssistantActionHandlerImpl(
                new OnboardingCoordinatorFactory(
                        context, bottomSheetController, browserControls, compositorViewHolder),
                activityTabProvider);
    }
}
