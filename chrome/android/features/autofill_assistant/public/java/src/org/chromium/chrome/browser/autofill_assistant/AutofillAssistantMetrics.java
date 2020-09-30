// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.metrics.FeatureModuleInstallation;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptOnboarding;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptShownToUser;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptStarted;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.metrics.UkmRecorder;
import org.chromium.content_public.browser.WebContents;

/**
 * Records user actions and histograms related to Autofill Assistant.
 *
 * All enums are auto generated from
 * components/autofill_assistant/browser/metrics.h.
 */
/* package */ class AutofillAssistantMetrics {
    /**
     * Records the reason for a drop out.
     */
    /* package */ static void recordDropOut(@DropOutReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.DropOutReason", reason, DropOutReason.MAX_VALUE + 1);
    }

    /**
     * Records the onboarding related action.
     */
    /* package */ static void recordOnBoarding(@OnBoarding int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.OnBoarding", metric, OnBoarding.MAX_VALUE + 1);
    }

    /**
     * Records the feature module installation action.
     */
    /* package */ static void recordFeatureModuleInstallation(
            @FeatureModuleInstallation int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.FeatureModuleInstallation", metric,
                FeatureModuleInstallation.MAX_VALUE + 1);
    }

    /**
     * UKM metric. Records the start of a lite script.
     */
    /* package */ static void recordLiteScriptStarted(
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
     */
    /* package */ static void recordLiteScriptFinished(
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
     * UKM metric. Records the onboarding after a successful lite script.
     */
    /* package */ static void recordLiteScriptOnboarding(
            WebContents webContents, @LiteScriptOnboarding int onboarding) {
        if (!areWebContentsValid(webContents)) {
            return;
        }
        new UkmRecorder.Bridge().recordEventWithIntegerMetric(webContents,
                /* eventName = */ "AutofillAssistant.LiteScriptOnboarding",
                /* metricName = */ "LiteScriptOnboarding",
                /* metricValue = */ onboarding);
    }

    /**
     * UKM metric. Records whether the lite script prompt was shown to the user or not.
     */
    /* package */ static void recordLiteScriptShownToUser(
            WebContents webContents, @LiteScriptShownToUser int shownToUser) {
        if (!areWebContentsValid(webContents)) {
            return;
        }
        new UkmRecorder.Bridge().recordEventWithIntegerMetric(webContents,
                /* eventName = */ "AutofillAssistant.LiteScriptShownToUser",
                /* metricName = */ "LiteScriptShownToUser",
                /* metricValue = */ shownToUser);
    }

    /**
     * Returns whether {@code webContents} are non-null and valid. Invalid webContents will cause a
     * failed DCHECK when attempting to report UKM metrics.
     */
    private static boolean areWebContentsValid(@Nullable WebContents webContents) {
        return webContents != null && !webContents.isDestroyed();
    }
}
