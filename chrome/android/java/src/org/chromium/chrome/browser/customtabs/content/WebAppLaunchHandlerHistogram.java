// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records a histogram that tracks usage of Launch Handler API. All actions must be kept in sync
 * with the definition in tools/metrics/histograms/metadata/custom_tabs/enums.xml.
 */
public class WebAppLaunchHandlerHistogram {
    private WebAppLaunchHandlerHistogram() {}

    private static final String CLIENT_MODE_HISTOGRAM =
            "TrustedWebActivity.LaunchHandler.ClientMode";

    @IntDef({
        ClientModeAction.INITIAL_INTENT,
        ClientModeAction.MODE_NAVIGATE_EXISTING,
        ClientModeAction.MODE_NAVIGATE_NEW,
        ClientModeAction.MODE_FOCUS_EXISTING,
        ClientModeAction.MODE_AUTO,
        ClientModeAction.COUNT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClientModeAction {
        int INITIAL_INTENT = 0;
        int MODE_NAVIGATE_EXISTING = 1;
        int MODE_NAVIGATE_NEW = 2;
        int MODE_FOCUS_EXISTING = 3;
        int MODE_AUTO = 4;

        /** Total count of entries. */
        int COUNT = 5;
    }

    public static void logClientMode(@ClientModeAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                CLIENT_MODE_HISTOGRAM, action, ClientModeAction.COUNT);
    }

    private static final String FILE_HANDLING_HISTOGRAM =
            "TrustedWebActivity.LaunchHandler.FileHandling";

    @IntDef({
        FileHandlingAction.NO_FILES,
        FileHandlingAction.SINGLE_FILE,
        FileHandlingAction.MULTIPLE_FILES,
        FileHandlingAction.COUNT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FileHandlingAction {
        int NO_FILES = 0;
        int SINGLE_FILE = 1;
        int MULTIPLE_FILES = 2;

        /** Total count of entries. */
        int COUNT = 3;
    }

    public static void logFileHandling(@FileHandlingAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                FILE_HANDLING_HISTOGRAM, action, FileHandlingAction.COUNT);
    }

    private static final String FAILURE_REASON_HISTOGRAM =
            "TrustedWebActivity.LaunchHandler.FailureReason";

    @IntDef({
        FailureReasonAction.TARGET_URL_VERIFICATION_FAILED,
        FailureReasonAction.CURRENT_PAGE_VERIFICATION_FAILED,
        FailureReasonAction.COUNT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FailureReasonAction {
        int TARGET_URL_VERIFICATION_FAILED = 0;
        int CURRENT_PAGE_VERIFICATION_FAILED = 1;

        /** Total count of entries. */
        int COUNT = 2;
    }

    public static void logFailureReason(@FailureReasonAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                FAILURE_REASON_HISTOGRAM, action, FailureReasonAction.COUNT);
    }
}
