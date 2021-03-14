// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.metrics.FeatureModuleInstallation;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptStarted;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.autofill_assistant.strings.IntentStrings;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.WebContents;

/**
 * Records user actions and histograms related to Autofill Assistant.
 *
 * All enums are auto generated from
 * components/autofill_assistant/browser/metrics.h.
 */
public class AutofillAssistantMetrics {
    /**
     * Records the reason for a drop out.
     */
    public static void recordDropOut(@DropOutReason int reason, String intent) {
        String histogramSuffix = getHistogramSuffixForIntent(intent);

        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.DropOutReason." + histogramSuffix, reason,
                DropOutReason.MAX_VALUE + 1);

        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.DropOutReason", reason, DropOutReason.MAX_VALUE + 1);
    }

    /**
     * Records the onboarding related action.
     */
    public static void recordOnBoarding(@OnBoarding int metric, String intent) {
        String histogramSuffix = getHistogramSuffixForIntent(intent);

        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.OnBoarding." + histogramSuffix, metric,
                OnBoarding.MAX_VALUE + 1);

        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.OnBoarding", metric, OnBoarding.MAX_VALUE + 1);
    }

    /**
     * Records the feature module installation action.
     */
    public static void recordFeatureModuleInstallation(@FeatureModuleInstallation int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.FeatureModuleInstallation", metric,
                FeatureModuleInstallation.MAX_VALUE + 1);
    }

    /**
     * UKM metric. Records the start of a lite script.
     *
     * The events recorded by this call lacks a trigger type. This is appropriate
     * when the trigger type is not yet known, because the Trigger protos sent by
     * the server have not been processed yet. If trigger protos are available,
     * record the metric from C++.
     */
    public static void recordLiteScriptStarted(
            WebContents webContents, @LiteScriptStarted int started) {
        if (!areWebContentsValid(webContents)) {
            return;
        }
        new UkmRecorder.Bridge().recordEventWithIntegerMetric(webContents,
                /* eventName = */ "AutofillAssistant.LiteScriptStarted",
                /* metricName = */ "LiteScriptStarted",
                /* metricValue = */ started);
    }

    /**
     * UKM metric. Records the finish of a lite script.
     *
     * The events recorded by this call lacks a trigger type. This is appropriate
     * when the trigger type is not yet known, because the Trigger protos sent by
     * the server have not been processed yet. If trigger protos are available,
     * record the metric from C++.
     */
    public static void recordLiteScriptFinished(
            WebContents webContents, @LiteScriptFinishedState int finishedState) {
        if (!areWebContentsValid(webContents)) {
            return;
        }
        new UkmRecorder.Bridge().recordEventWithIntegerMetric(webContents,
                /* eventName = */ "AutofillAssistant.LiteScriptFinished",
                /* metricName = */ "LiteScriptFinished",
                /* metricValue = */ finishedState);
    }

    /**
     * Returns whether {@code webContents} are non-null and valid. Invalid
     * webContents will cause a failed DCHECK when attempting to report UKM metrics.
     */
    private static boolean areWebContentsValid(@Nullable WebContents webContents) {
        return webContents != null && !webContents.isDestroyed();
    }

    /**
     * Returns histogram suffix for given intent.
     */
    private static String getHistogramSuffixForIntent(String intent) {
        if (intent == null) {
            // Intent is not set.
            return "NotSet";
        }
        switch (intent) {
            case IntentStrings.BUY_MOVIE_TICKET:
                return "BuyMovieTicket";
            case IntentStrings.FLIGHTS_CHECKIN:
                return "FlightsCheckin";
            case IntentStrings.FOOD_ORDERING:
                return "FoodOrdering";
            case IntentStrings.FOOD_ORDERING_DELIVERY:
                return "FoodOrderingDelivery";
            case IntentStrings.FOOD_ORDERING_PICKUP:
                return "FoodOrderingPickup";
            case IntentStrings.PASSWORD_CHANGE:
                return "PasswordChange";
            case IntentStrings.RENT_CAR:
                return "RentCar";
            case IntentStrings.SHOPPING:
                return "Shopping";
            case IntentStrings.SHOPPING_ASSISTED_CHECKOUT:
                return "ShoppingAssistedCheckout";
            case IntentStrings.TELEPORT:
                return "Teleport";
        }
        return "UnknownIntent";
    }
}
