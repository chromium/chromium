// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class ReadAloudMetrics {
    public static String IS_READABLE = "ReadAloud.IsPageReadable";
    public static String READABILITY_SUCCESS = "ReadAloud.IsPageReadabilitySuccessful";
    public static String INELIGIBILITY_REASON = "ReadAloud.Eligibility.IneligiblityReason";
    public static String IS_USER_ELIGIBLE = "ReadAloud.Eligibility.IsUserEligible";
    public static String IS_TAB_PLAYBACK_CREATION_SUCCESSFUL =
            "ReadAloud.IsTabPlaybackCreationSuccessful";
    public static String HAS_TAP_TO_SEEK_FOUND_MATCH = "ReadAloud.HasTapToSeekFoundMatch";
    public static String TAB_PLAYBACK_CREATION_SUCCESS = "ReadAloud.TabPlaybackCreationSuccess";
    public static String TAB_PLAYBACK_CREATION_FAILURE = "ReadAloud.TabPlaybackCreationFailure";
    public static String TAB_PLAYBACK_WITHOUT_READABILITY_CHECK_ERROR =
            "ReadAloud.ReadAloudPlaybackWithoutReadabilityCheckError";
    public static String TIME_SPENT_LISTENING = "ReadAloud.DurationListened";
    public static String TIME_SPENT_LISTENING_LOCKED_SCREEN =
            "ReadAloud.DurationListened.LockedScreen";
    public static String HAS_DATE_MODIFIED = "ReadAloud.HasDateModified";
    public static String READABILITY_SERVER_SIDE = "ReadAloud.ServerReadabilityResult";
    public static String TAP_TO_SEEK_TIME = "ReadAloud.TapToSeekTime";
    public static String EMPTY_URL_PLAYBACK = "ReadAloud.EmptyURLPlayback";
    public static String REASON_FOR_STOPPING_PLAYBACK = "ReadAloud.TabPlaybackStoppedReason";
    public static String DURATION_SCRUBBING_FORWARDS_SEEKBAR =
            "ReadAloud.DurationScrubbingForwardsSeekbar";
    public static String DURATION_SCRUBBING_BACKWARDS_SEEKBAR =
            "ReadAloud.DurationScrubbingBackwardsSeekbar";

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

    /**
     * Reasons for stopping a playback defined in readaloud/enums.xml.
     *
     * <p>Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({
        ReasonForStoppingPlayback.UNKNOWN_REASON,
        ReasonForStoppingPlayback.MANUAL_CLOSE,
        ReasonForStoppingPlayback.NAVIGATION_WITHIN_PLAYING_TAB,
        ReasonForStoppingPlayback.APP_BACKGROUNDED,
        ReasonForStoppingPlayback.APP_DESTROYED,
        ReasonForStoppingPlayback.VOICE_PREVIEW,
        ReasonForStoppingPlayback.VOICE_CHANGE,
        ReasonForStoppingPlayback.NEW_PLAYBACK_REQUEST,
        ReasonForStoppingPlayback.TRANSLATION_STATE_CHANGE,
        ReasonForStoppingPlayback.ACTIVITY_ATTACHEMENT_CHANGED,
        ReasonForStoppingPlayback.EXTERNAL_PLAYBACK_REQUEST,
        ReasonForStoppingPlayback.TAB_CLOSED,
    })
    public @interface ReasonForStoppingPlayback {
        int UNKNOWN_REASON = 0;
        // User stopped playback by closing the player UI.
        int MANUAL_CLOSE = 1;
        // User stopped the playack by navigating to a different URL from within a playing tab or
        // refreshing current tab
        int NAVIGATION_WITHIN_PLAYING_TAB = 2;
        // Playback stopped due to chrome being backgrounded
        int APP_BACKGROUNDED = 3;
        // Playback stopped due to app being destroyed
        int APP_DESTROYED = 4;
        // Playback stopped due to the user pre-viewing other available voices
        int VOICE_PREVIEW = 5;
        // Playback stopped due to changing selected voice
        int VOICE_CHANGE = 6;
        // Playback stopped due to a new request (entrypoint click)
        int NEW_PLAYBACK_REQUEST = 7;
        // Page translated during anctive playback
        int TRANSLATION_STATE_CHANGE = 8;
        // Playing tab was moved to a different activity
        int ACTIVITY_ATTACHEMENT_CHANGED = 9;
        // Playing tab was closed.
        int TAB_CLOSED = 10;
        // Playback was requested from another activity (e.g. Chrome playback must stop because CCT
        // is about to play).
        int EXTERNAL_PLAYBACK_REQUEST = 11;
        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 12;
    }

    public static void recordDurationMsListened(long durationMs) {
        if (durationMs != 0) {
            RecordHistogram.recordLongTimesHistogram(TIME_SPENT_LISTENING, durationMs);
        }
    }

    public static void recordTapToSeekTime(long durationMs) {
        RecordHistogram.recordTimesHistogram(TAP_TO_SEEK_TIME, durationMs);
    }

    public static void recordDurationMsListenedLockedScreen(long durationMs) {
        if (durationMs != 0) {
            RecordHistogram.recordLongTimesHistogram(
                    TIME_SPENT_LISTENING_LOCKED_SCREEN, durationMs);
        }
    }

    public static void recordDurationScrubbingForwards(long durationMs) {
        RecordHistogram.recordTimesHistogram(DURATION_SCRUBBING_FORWARDS_SEEKBAR, durationMs);
    }

    public static void recordDurationScrubbingBackwards(long durationMs) {
        RecordHistogram.recordTimesHistogram(DURATION_SCRUBBING_BACKWARDS_SEEKBAR, durationMs);
    }

    public static void recordSeekForwardTapped() {
        RecordUserAction.record("ReadAloud.SeekForward");
    }

    public static void recordSeekBackwardTapped() {
        RecordUserAction.record("ReadAloud.SeekBackward");
    }

    public static void recordIsPageReadable(boolean successful) {
        RecordHistogram.recordBooleanHistogram(IS_READABLE, successful);
    }

    public static void recordServerReadabilityResult(boolean successful) {
        RecordHistogram.recordBooleanHistogram(READABILITY_SERVER_SIDE, successful);
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

    public static void recordHasTapToSeekFoundMatch(boolean matchFound) {
        RecordHistogram.recordBooleanHistogram(HAS_TAP_TO_SEEK_FOUND_MATCH, matchFound);
    }

    public static void recordTabCreationSuccess(int entrypoint, int maxVal) {
        RecordHistogram.recordEnumeratedHistogram(
                TAB_PLAYBACK_CREATION_SUCCESS, entrypoint, maxVal);
    }

    public static void recordTabCreationFailure(int entrypoint, int maxVal) {
        RecordHistogram.recordEnumeratedHistogram(
                TAB_PLAYBACK_CREATION_FAILURE, entrypoint, maxVal);
    }

    public static void recordPlaybackWithoutReadabilityCheck(int entrypoint, int maxVal) {
        RecordHistogram.recordEnumeratedHistogram(
                TAB_PLAYBACK_WITHOUT_READABILITY_CHECK_ERROR, entrypoint, maxVal);
    }

    public static void recordEmptyURLPlayback(int entrypoint, int maxVal) {
        RecordHistogram.recordEnumeratedHistogram(EMPTY_URL_PLAYBACK, entrypoint, maxVal);
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

    public static void recordHasDateModified(boolean hasDateModified) {
        RecordHistogram.recordBooleanHistogram(HAS_DATE_MODIFIED, hasDateModified);
    }

    public static void recordReasonForStoppingPlayback(@ReasonForStoppingPlayback int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                REASON_FOR_STOPPING_PLAYBACK, reason, ReasonForStoppingPlayback.NUM_ENTRIES);
    }
}
