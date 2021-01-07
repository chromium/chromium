// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_REQUEST_TRIGGER_SCRIPT;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_STARTED_WITH_TRIGGER_SCRIPT;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_TRIGGER_FIRST_TIME_USER;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_TRIGGER_RETURNING_TIME_USER;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_TRIGGER_SCRIPTS_BASE64;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_TRIGGER_SCRIPT_USED;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptOnboarding;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptStarted;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
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
    @Override
    public void start(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            Context context, @NonNull WebContents webContents,
            ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
            ApplicationViewportInsetSupplier bottomInsetProvider,
            ActivityTabProvider activityTabProvider, boolean isChromeCustomTab,
            @NonNull String initialUrl, Map<String, String> parameters, String experimentIds,
            @Nullable String callerAccount, @Nullable String userName) {
        if (shouldStartTriggerScript(parameters)) {
            if (!AutofillAssistantPreferencesUtil.isProactiveHelpOn()) {
                // Opt-out users who have disabled the proactive help Chrome setting.
                AutofillAssistantMetrics.recordLiteScriptStarted(
                        webContents, LiteScriptStarted.LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED);
                return;
            }
            if ((!TextUtils.isEmpty(parameters.get(PARAMETER_TRIGGER_FIRST_TIME_USER))
                        || !TextUtils.isEmpty(parameters.get(PARAMETER_REQUEST_TRIGGER_SCRIPT)))
                    && !UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                            AutofillAssistantUiController.getProfile())) {
                // Proactive help that requires communicating with a remote endpoint is tied to the
                // MSBB flag. Even though proactive help is toggled on, we need to stop if MSBB is
                // off. The proactive help setting will appear disabled to the user.
                AutofillAssistantMetrics.recordLiteScriptStarted(
                        webContents, LiteScriptStarted.LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED);
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
                triggerScriptBridge.start(bottomSheetController, context,
                        keyboardVisibilityDelegate, bottomInsetProvider, activityTabProvider,
                        webContents, initialUrl, parameters, experimentIds,
                        new AssistantTriggerScriptBridge.Delegate() {
                            @Override
                            public void onTriggerScriptFinished(
                                    @LiteScriptFinishedState int finishedState) {
                                if (finishedState
                                        == LiteScriptFinishedState.LITE_SCRIPT_PROMPT_SUCCEEDED) {
                                    parameters.put(PARAMETER_STARTED_WITH_TRIGGER_SCRIPT, "true");
                                    startAutofillAssistantRegular(bottomSheetController,
                                            browserControls, compositorViewHolder, context,
                                            webContents, isChromeCustomTab, initialUrl, parameters,
                                            experimentIds, callerAccount, userName);
                                }
                            }
                        });
                return;
            }

            // Legacy lite scripts, remove as soon as possible.
            String firstTimeUserScriptPath = parameters.get(PARAMETER_TRIGGER_FIRST_TIME_USER);
            String returningUserScriptPath = parameters.get(PARAMETER_TRIGGER_RETURNING_TIME_USER);
            startAutofillAssistantLite(bottomSheetController, browserControls, compositorViewHolder,
                    webContents, firstTimeUserScriptPath, returningUserScriptPath, result -> {
                        if (result) {
                            parameters.put(PARAMETER_TRIGGER_SCRIPT_USED,
                                    isFirstTimeUser ? firstTimeUserScriptPath
                                                    : returningUserScriptPath);
                            startAutofillAssistantRegular(bottomSheetController, browserControls,
                                    compositorViewHolder, context, webContents, isChromeCustomTab,
                                    initialUrl, parameters, experimentIds, callerAccount, userName);
                        }
                    });
            return;
        }

        // Regular flow for starting without dedicated trigger script.
        startAutofillAssistantRegular(bottomSheetController, browserControls, compositorViewHolder,
                context, webContents, isChromeCustomTab, initialUrl, parameters, experimentIds,
                callerAccount, userName);
    }

    /** Whether {@code parameters} indicate that a trigger script should be started. */
    private boolean shouldStartTriggerScript(Map<String, String> parameters) {
        return !TextUtils.isEmpty(parameters.get(PARAMETER_TRIGGER_FIRST_TIME_USER))
                || TextUtils.equals(parameters.get(PARAMETER_REQUEST_TRIGGER_SCRIPT), "true")
                || !TextUtils.isEmpty(parameters.get(PARAMETER_TRIGGER_SCRIPTS_BASE64));
    }
    /**
     * Starts a 'lite' autofill assistant script in the background. Does not show the onboarding.
     * Does not have access to any information aside from the trigger script paths. Calls {@code
     * onFinishedCallback} when the lite script finishes.
     */
    private void startAutofillAssistantLite(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            @NonNull WebContents webContents, String firstTimeUserScriptPath,
            String returningUserScriptPath, Callback<Boolean> onFinishedCallback) {
        AutofillAssistantLiteScriptCoordinator liteScriptCoordinator =
                new AutofillAssistantLiteScriptCoordinator(
                        bottomSheetController, browserControls, compositorViewHolder, webContents);
        liteScriptCoordinator.startLiteScript(
                firstTimeUserScriptPath, returningUserScriptPath, onFinishedCallback);
    }

    /**
     * Starts a regular autofill assistant script. Shows the onboarding as necessary.
     */
    private void startAutofillAssistantRegular(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            Context context, @NonNull WebContents webContents, boolean isChromeCustomTab,
            @NonNull String initialUrl, Map<String, String> parameters, String experimentIds,
            @Nullable String callerAccount, @Nullable String userName) {
        if (!AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            if (parameters.containsKey(PARAMETER_TRIGGER_SCRIPT_USED)
                    || parameters.containsKey(PARAMETER_STARTED_WITH_TRIGGER_SCRIPT)) {
                AutofillAssistantMetrics.recordLiteScriptOnboarding(
                        webContents, LiteScriptOnboarding.LITE_SCRIPT_ONBOARDING_ALREADY_ACCEPTED);
            }
            AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_NOT_SHOWN);
            AutofillAssistantClient.fromWebContents(webContents)
                    .start(initialUrl, parameters, experimentIds, callerAccount, userName,
                            isChromeCustomTab,
                            /* onboardingCoordinator= */ null);
            return;
        }

        BottomSheetOnboardingCoordinator onboardingCoordinator =
                new BottomSheetOnboardingCoordinator(experimentIds, parameters, context,
                        bottomSheetController, browserControls, compositorViewHolder,
                        bottomSheetController.getScrimCoordinator());

        onboardingCoordinator.show(accepted -> {
            if (parameters.containsKey(PARAMETER_TRIGGER_SCRIPT_USED)
                    || parameters.containsKey(PARAMETER_STARTED_WITH_TRIGGER_SCRIPT)) {
                AutofillAssistantMetrics.recordLiteScriptOnboarding(webContents,
                        accepted ? LiteScriptOnboarding.LITE_SCRIPT_ONBOARDING_SEEN_AND_ACCEPTED
                                 : LiteScriptOnboarding.LITE_SCRIPT_ONBOARDING_SEEN_AND_REJECTED);
            }
            if (!accepted) {
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
