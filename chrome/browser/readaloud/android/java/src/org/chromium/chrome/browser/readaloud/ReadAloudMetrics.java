// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class ReadAloudMetrics {
    @VisibleForTesting public static String READABILITY_SUCCESS = "ReadAloud.IsPageReadable";

    @VisibleForTesting
    public static String INELIGIBILITY_REASON = "ReadAloud.Eligibility.IneligiblityReason";

    @VisibleForTesting
    public static String IS_USER_ELIGIBLE = "ReadAloud.Eligibility.IsUserEligible";

    @VisibleForTesting
    public static String IS_TAB_PLAYBACK_CREATION_SUCCESSFUL =
            "ReadAloud.IsTabPlaybackCreationSuccessful";

    /**
     * The reason why we clear the prepared message.
     *
     * <p>Needs to stay in sync with ReadAloudIneligibilityReason in enums.xml. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        IneligibilityReason.UNKNOWN,
        IneligibilityReason.FEATURE_FLAG_DISABLED,
        IneligibilityReason.INCOGNITO_MODE,
        IneligibilityReason.MSBB_DISABLED,
        IneligibilityReason.POLICY_DISABLED,
        IneligibilityReason.DEFAULT_SEARCH_ENGINE_GOOGLE_FALSE,
        IneligibilityReason.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface IneligibilityReason {
        int UNKNOWN = 0;
        int FEATURE_FLAG_DISABLED = 1;
        int INCOGNITO_MODE = 2;
        int MSBB_DISABLED = 3;
        int POLICY_DISABLED = 4;
        int DEFAULT_SEARCH_ENGINE_GOOGLE_FALSE = 5;
        // Always update COUNT to match the last reason in the list.
        int COUNT = 6;
    }

    public static void recordIsPageReadable(boolean successful) {
        RecordHistogram.recordBooleanHistogram(READABILITY_SUCCESS, successful);
    }

    public static void recordIsUserEligible(boolean eligible) {
        RecordHistogram.recordBooleanHistogram(IS_USER_ELIGIBLE, eligible);
    }

    public static void recordIneligibilityReason(@IneligibilityReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                INELIGIBILITY_REASON, reason, IneligibilityReason.COUNT);
    }

    public static void recordIsTabPlaybackCreationSuccessful(boolean successful) {
        RecordHistogram.recordBooleanHistogram(IS_TAB_PLAYBACK_CREATION_SUCCESSFUL, successful);
    }
}
