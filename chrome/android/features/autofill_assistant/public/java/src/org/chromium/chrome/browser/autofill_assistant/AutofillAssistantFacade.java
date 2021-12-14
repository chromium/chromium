// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Function;
import org.chromium.base.Log;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.directactions.DirectActionHandler;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.external_intents.ExternalNavigationDelegate.IntentToAutofillAllowingAppResult;

/** Facade for starting Autofill Assistant on a tab. */
public class AutofillAssistantFacade {
    /** Used for logging. */
    private static final String TAG = "AutofillAssistant";

    /**
     * Synthetic field trial names and group names should match those specified in
     * google3/analysis/uma/dashboards/
     * .../variations/generate_server_hashes.py and
     * .../website/components/variations_dash/variations_histogram_entry.js.
     */
    private static final String TRIGGERED_SYNTHETIC_TRIAL = "AutofillAssistantTriggered";
    private static final String ENABLED_GROUP = "Enabled";

    private static final String EXPERIMENTS_SYNTHETIC_TRIAL = "AutofillAssistantExperimentsTrial";

    /** Returns true if conditions are satisfied to attempt to start Autofill Assistant. */
    private static boolean isConfigured(TriggerContext arguments) {
        return arguments.isEnabled();
    }

    /**
     * Starts Autofill Assistant.
     * @param activity {@link ChromeActivity} the activity on which the Autofill Assistant is being
     *         started. This must be a launch activity holding the correct intent for starting.
     */
    public static void start(ChromeActivity activity) {
        start(activity,
                TriggerContext.newBuilder()
                        .fromBundle(activity.getInitialIntent().getExtras())
                        .withInitialUrl(activity.getInitialIntent().getDataString())
                        .build());
    }

    /**
     * Starts Autofill Assistant.
     * @param activity {@link Activity} the activity on which the Autofill Assistant is being
     *         started.
     * @param bundleExtras {@link Bundle} the extras which were used to start the Autofill
     *         Assistant.
     * @param initialUrl the initial URL the Autofill Assistant should be started on.
     */
    public static void start(Activity activity, @Nullable Bundle bundleExtras, String initialUrl) {
        // TODO(crbug.com/1155809): Remove ChromeActivity reference.
        assert activity instanceof ChromeActivity;
        ChromeActivity chromeActivity = (ChromeActivity) activity;
        start(chromeActivity,
                TriggerContext.newBuilder()
                        .fromBundle(bundleExtras)
                        .withInitialUrl(initialUrl)
                        .build());
    }

    /**
     * Starts Autofill Assistant.
     * @param activity {@link Activity} the activity on which the Autofill Assistant is being
     *         started.
     * @param triggerContext {@link TriggerContext} the trigger context, containing startup
     *         parameters and information.
     */
    public static void start(@Nullable Activity activity, TriggerContext triggerContext) {
        if (!(activity instanceof ChromeActivity)) {
            Log.v(TAG, "Failed to retrieve ChromeActivity.");
            return;
        }
        // Register synthetic trial as soon as possible.
        UmaSessionStats.registerSyntheticFieldTrial(TRIGGERED_SYNTHETIC_TRIAL, ENABLED_GROUP);
        // Synthetic trial for experiments.
        String experimentIds = triggerContext.getExperimentIds();
        if (!experimentIds.isEmpty()) {
            for (String experimentId : experimentIds.split(",")) {
                UmaSessionStats.registerSyntheticFieldTrial(
                        EXPERIMENTS_SYNTHETIC_TRIAL, experimentId);
            }
        }

        String intent = triggerContext.getIntent();
        // Have an "attempted starts" baseline for the drop out histogram.
        AutofillAssistantMetrics.recordDropOut(DropOutReason.AA_START, intent);
        waitForTab((ChromeActivity) activity,
                tab -> { AutofillAssistantTabHelper.get(tab).start(triggerContext); });
    }

    /**
     * Returns a {@link DirectActionHandler} for making dynamic actions available under Android Q.
     *
     * <p>This should only be called if {@link
     * AssistantDependencyUtilsChrome#areDirectActionsAvailable} returns true. This method can also
     * return null if autofill assistant is not available for some other reasons.
     */
    public static DirectActionHandler createDirectActionHandler(Context context,
            BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, View rootView,
            ActivityTabProvider activityTabProvider) {
        return new AutofillAssistantDirectActionHandler(context, bottomSheetController,
                browserControls, rootView, activityTabProvider,
                AutofillAssistantModuleEntryProvider.INSTANCE);
    }

    /** Provides the callback with a tab, waits if necessary. */
    private static void waitForTab(ChromeActivity activity, Callback<Tab> callback) {
        if (activity.getActivityTab() != null) {
            callback.onResult(activity.getActivityTab());
            return;
        }

        // The tab is not yet available. We need to register as listener and wait for it.
        activity.getActivityTabProvider().addObserver(new Callback<Tab>() {
            @Override
            public void onResult(Tab tab) {
                if (tab == null) return;
                activity.getActivityTabProvider().removeObserver(this);
                assert tab.getWebContents() != null;
                callback.onResult(tab);
            }
        });
    }

    public static boolean isAutofillAssistantEnabled(Intent intent) {
        return AssistantFeatures.AUTOFILL_ASSISTANT.isEnabled()
                && AutofillAssistantFacade.isConfigured(
                        TriggerContext.newBuilder().fromBundle(intent.getExtras()).build());
    }

    public static boolean isAutofillAssistantByIntentTriggeringEnabled(Intent intent) {
        return AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY.isEnabled()
                && AutofillAssistantFacade.isAutofillAssistantEnabled(intent);
    }

    public static @IntentToAutofillAllowingAppResult int shouldAllowOverrideWithApp(
            Intent intent, Function<Intent, Boolean> canExternalAppHandleIntent) {
        TriggerContext triggerContext =
                TriggerContext.newBuilder().fromBundle(intent.getExtras()).build();
        if (!triggerContext.allowAppOverride()) {
            return IntentToAutofillAllowingAppResult.NONE;
        }
        if (canExternalAppHandleIntent.apply(intent)) {
            return IntentToAutofillAllowingAppResult.DEFER_TO_APP_NOW;
        }

        String originalDeeplink = triggerContext.getOriginalDeeplink();
        if (TextUtils.isEmpty(originalDeeplink)) {
            return IntentToAutofillAllowingAppResult.NONE;
        }
        Intent originalDeeplinkIntent = new Intent(Intent.ACTION_VIEW);
        originalDeeplinkIntent.setData(Uri.parse(originalDeeplink));
        if (canExternalAppHandleIntent.apply(originalDeeplinkIntent)) {
            return IntentToAutofillAllowingAppResult.DEFER_TO_APP_LATER;
        }

        return IntentToAutofillAllowingAppResult.NONE;
    }
}
