// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class ReadAloudMetrics {
    @VisibleForTesting public static String IS_READABLE = "ReadAloud.IsPageReadable";

    @VisibleForTesting
    public static String READABILITY_SUCCESS = "ReadAloud.IsPageReadabilitySuccessful";

    @VisibleForTesting
    public static String INELIGIBILITY_REASON = "ReadAloud.Eligibility.IneligiblityReason";

    @VisibleForTesting
    public static String IS_USER_ELIGIBLE = "ReadAloud.Eligibility.IsUserEligible";

    @VisibleForTesting
    public static String IS_TAB_PLAYBACK_CREATION_SUCCESSFUL =
            "ReadAloud.IsTabPlaybackCreationSuccessful";
    public static String VOICE_CHANGED = "ReadAloud.VoiceChanged.";
    public static String VOICE_PREVIEWED = "ReadAloud.VoicePreviewed.";
    public static String TIME_SPENT_LISTENING = "ReadAloud.DurationListened";
    public static String TIME_SPENT_LISTENING_LOCKED_SCREEN =
            "ReadAloud.DurationListened.LockedScreen";

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

    /**
     * Speed settings.
     *
     * <p>Needs to stay in sync with ReadAloudIneligibilityReason in enums.xml. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        PlaybackSpeed.SPEED_0_5,
        PlaybackSpeed.SPEED_0_8,
        PlaybackSpeed.SPEED_1_0,
        PlaybackSpeed.SPEED_1_2,
        PlaybackSpeed.SPEED_1_5,
        PlaybackSpeed.SPEED_2_0,
        PlaybackSpeed.SPEED_3_0,
        PlaybackSpeed.SPEED_4_0,
        PlaybackSpeed.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PlaybackSpeed {
        int SPEED_0_5 = 0;
        int SPEED_0_8 = 1;
        int SPEED_1_0 = 2;
        int SPEED_1_2 = 3;
        int SPEED_1_5 = 4;
        int SPEED_2_0 = 5;
        int SPEED_3_0 = 6;
        int SPEED_4_0 = 7;
        // Always update COUNT to match the last value in the list.
        int COUNT = 7;
    }

    private static float[] sPlaybackSpeeds = {0.5f, 0.8f, 1.0f, 1.2f, 1.5f, 2.0f, 3.0f, 4.0f};

    public static void recordDurationMsListened(long durationMs) {
        if (durationMs != 0) {
            RecordHistogram.recordLongTimesHistogram(TIME_SPENT_LISTENING, durationMs);
        }
    }

    public static void recordDurationMsListenedLockedScreen(long durationMs) {
        if (durationMs != 0) {
            RecordHistogram.recordLongTimesHistogram(
                    TIME_SPENT_LISTENING_LOCKED_SCREEN, durationMs);
        }
    }

    public static void recordIsPageReadable(boolean successful) {
        RecordHistogram.recordBooleanHistogram(IS_READABLE, successful);
    }

    public static void recordIsPageReadabilitySuccessful(boolean successful) {
        RecordHistogram.recordBooleanHistogram(READABILITY_SUCCESS, successful);
    }

    public static void recordIsUserEligible(boolean eligible) {
        RecordHistogram.recordBooleanHistogram(IS_USER_ELIGIBLE, eligible);
    }

    public static void recordHighlightingEnabledChanged(boolean enabled) {
        RecordHistogram.recordBooleanHistogram("ReadAloud.HighlightingEnabled", enabled);
    }

    public static void recordHighlightingEnabledOnStartup(boolean enabled) {
        RecordHistogram.recordBooleanHistogram("ReadAloud.HighlightingEnabled.OnStartup", enabled);
    }

    public static void recordIneligibilityReason(@IneligibilityReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                INELIGIBILITY_REASON, reason, IneligibilityReason.COUNT);
    }

    public static void recordHighlightingSupported(boolean supported) {
        RecordHistogram.recordBooleanHistogram("ReadAloud.HighlightingSupported", supported);
    }

    public static void recordIsTabPlaybackCreationSuccessful(boolean successful) {
        RecordHistogram.recordBooleanHistogram(IS_TAB_PLAYBACK_CREATION_SUCCESSFUL, successful);
    }

    public static void recordSpeedChange(float speed) {
        for (int i = 0; i < sPlaybackSpeeds.length; i++) {
            if (speed == sPlaybackSpeeds[i]) {
                RecordHistogram.recordEnumeratedHistogram(
                        "ReadAloud.SpeedChange", i, PlaybackSpeed.COUNT);
            }
        }
    }

    public static void recordPlaybackStarted() {
        RecordUserAction.record("ReadAloud.PlaybackStarted");
    }

    public static void recordVoiceChanged(String voiceID) {
        RecordHistogram.recordBooleanHistogram(VOICE_CHANGED + voiceID, true);
    }

    public static void recordVoicePreviewed(String voiceID) {
        RecordHistogram.recordBooleanHistogram(VOICE_PREVIEWED + voiceID, true);
    }
}
