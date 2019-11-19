// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.metrics.FeatureModuleInstallation;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;

/**
 * Records user actions and histograms related to Autofill Assistant.
 *
 * All enums are auto generated from
 * components/autofill_assistant/browser/metrics.h.
 */
/* package */ class AutofillAssistantMetrics {
    /**
     * Note: Java-side constructors expect a (NUM_ENTRIES+1) value, but C++ works with implicitly
     * defined enum boundaries using 'kMaxValue'. See crbug.com/983518.
     */
    private static final EnumeratedHistogramSample ENUMERATED_DROP_OUT_REASON =
            new EnumeratedHistogramSample(
                    "Android.AutofillAssistant.DropOutReason", DropOutReason.MAX_VALUE + 1);

    private static final EnumeratedHistogramSample ENUMERATED_ON_BOARDING =
            new EnumeratedHistogramSample(
                    "Android.AutofillAssistant.OnBoarding", OnBoarding.MAX_VALUE + 1);

    private static final EnumeratedHistogramSample ENUMERATED_FEATURE_MODULE_INSTALLATION =
            new EnumeratedHistogramSample("Android.AutofillAssistant.FeatureModuleInstallation",
                    FeatureModuleInstallation.MAX_VALUE + 1);

    /**
     * Records the reason for a drop out.
     */
    /* package */ static void recordDropOut(@DropOutReason int reason) {
        ENUMERATED_DROP_OUT_REASON.record(reason);
    }

    /**
     * Records the onboarding related action.
     */
    /* package */ static void recordOnBoarding(@OnBoarding int metric) {
        ENUMERATED_ON_BOARDING.record(metric);
    }

    /**
     * Records the feature module installation action.
     */
    /* package */ static void recordFeatureModuleInstallation(
            @FeatureModuleInstallation int metric) {
        ENUMERATED_FEATURE_MODULE_INSTALLATION.record(metric);
    }
}
