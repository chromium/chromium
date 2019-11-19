// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * An interface for recording user interactions.  One implementation does logging to Ranker.
 */
public interface ContextualSearchInteractionRecorder {
    // NOTE: this list needs to be kept in sync with the white list in
    // predictor_config_definitions.cc, the names list in ContextualSearchRankerLoggerImpl.java
    // and with ukm.xml!
    @IntDef({Feature.UNKNOWN, Feature.OUTCOME_WAS_PANEL_OPENED,
            Feature.OUTCOME_WAS_QUICK_ACTION_CLICKED, Feature.OUTCOME_WAS_QUICK_ANSWER_SEEN,
            Feature.OUTCOME_WAS_CARDS_DATA_SHOWN, Feature.DURATION_AFTER_SCROLL_MS,
            Feature.SCREEN_TOP_DPS, Feature.WAS_SCREEN_BOTTOM,
            Feature.PREVIOUS_WEEK_IMPRESSIONS_COUNT, Feature.PREVIOUS_WEEK_CTR_PERCENT,
            Feature.PREVIOUS_28DAY_IMPRESSIONS_COUNT, Feature.PREVIOUS_28DAY_CTR_PERCENT,
            Feature.DID_OPT_IN, Feature.IS_SHORT_WORD, Feature.IS_LONG_WORD, Feature.IS_WORD_EDGE,
            Feature.IS_ENTITY, Feature.TAP_DURATION_MS, Feature.FONT_SIZE,
            Feature.IS_SECOND_TAP_OVERRIDE, Feature.IS_HTTP, Feature.IS_ENTITY_ELIGIBLE,
            Feature.IS_LANGUAGE_MISMATCH, Feature.PORTION_OF_ELEMENT, Feature.TAP_COUNT,
            Feature.OPEN_COUNT, Feature.QUICK_ANSWER_COUNT, Feature.ENTITY_IMPRESSIONS_COUNT,
            Feature.ENTITY_OPENS_COUNT, Feature.QUICK_ACTION_IMPRESSIONS_COUNT,
            Feature.QUICK_ACTIONS_TAKEN_COUNT, Feature.QUICK_ACTIONS_IGNORED_COUNT})
    @Retention(RetentionPolicy.SOURCE)
    @interface Feature {
        // Values are used for indexing - should start from 0 and can't have gaps.
        int UNKNOWN = 0;
        // Outcome labels:
        int OUTCOME_WAS_PANEL_OPENED = 1;
        int OUTCOME_WAS_QUICK_ACTION_CLICKED = 2;
        int OUTCOME_WAS_QUICK_ANSWER_SEEN = 3;
        int OUTCOME_WAS_CARDS_DATA_SHOWN = 4; // a UKM CS v2 label.
                                              // Features:
        int DURATION_AFTER_SCROLL_MS = 5;
        int SCREEN_TOP_DPS = 6;
        int WAS_SCREEN_BOTTOM = 7;
        // User usage features:
        int PREVIOUS_WEEK_IMPRESSIONS_COUNT = 8;
        int PREVIOUS_WEEK_CTR_PERCENT = 9;
        int PREVIOUS_28DAY_IMPRESSIONS_COUNT = 10;
        int PREVIOUS_28DAY_CTR_PERCENT = 11;
        // UKM CS v2 features (see go/ukm-cs-2).
        int DID_OPT_IN = 12;
        int IS_SHORT_WORD = 13;
        int IS_LONG_WORD = 14;
        int IS_WORD_EDGE = 15;
        int IS_ENTITY = 16;
        int TAP_DURATION_MS = 17;
        // UKM CS v3 features (see go/ukm-cs-3).
        int FONT_SIZE = 18;
        int IS_SECOND_TAP_OVERRIDE = 19;
        int IS_HTTP = 20;
        int IS_ENTITY_ELIGIBLE = 21;
        int IS_LANGUAGE_MISMATCH = 22;
        int PORTION_OF_ELEMENT = 23;
        // UKM CS v4 features (see go/ukm-cs-4).
        int TAP_COUNT = 24;
        int OPEN_COUNT = 25;
        int QUICK_ANSWER_COUNT = 26;
        int ENTITY_IMPRESSIONS_COUNT = 27;
        int ENTITY_OPENS_COUNT = 28;
        int QUICK_ACTION_IMPRESSIONS_COUNT = 29;
        int QUICK_ACTIONS_TAKEN_COUNT = 30;
        int QUICK_ACTIONS_IGNORED_COUNT = 31;

        int NUM_ENTRIES = 32;
    }

    /**
     * Sets up logging for the base page which is identified by the given {@link WebContents}.
     * This method must be called before calling {@link #logFeature} or {@link #logOutcome}.
     * @param basePageWebContents The {@link WebContents} of the base page to log with Ranker.
     */
    void setupLoggingForPage(@Nullable WebContents basePageWebContents);

    /**
     * Logs a particular feature at inference time as a key/value pair.
     * @param feature The feature to log.
     * @param value The value to log, which is associated with the given key.
     */
    void logFeature(@Feature int feature, Object value);

    /**
     * Returns whether or not AssistRanker query is enabled.
     */
    boolean isQueryEnabled();

    /**
     * Logs an outcome value at training time that indicates an ML label as a key/value pair.
     * @param feature The feature to log.
     * @param value The outcome label value.
     */
    void logOutcome(@Feature int feature, Object value);

    /**
     * Tries to run the machine intelligence model for tap suppression and returns an int that
     * describes whether the prediction was obtainable and what it was.
     * See chrome/browser/android/contextualsearch/contextual_search_ranker_logger_impl.h for
     * details on the {@link AssistRankerPrediction} possibilities.
     * @return An integer that encodes the prediction result.
     */
    @AssistRankerPrediction
    int runPredictionForTapSuppression();

    /**
     * Gets the previous result from trying to run the machine intelligence model for tap
     * suppression. A previous call to {@link #runPredictionForTapSuppression} is required.
     * See chrome/browser/android/contextualsearch/contextual_search_ranker_logger_impl.h for
     * details on the {@link AssistRankerPrediction} possibilities.
     * @return An integer that encodes the prediction.
     */
    @AssistRankerPrediction
    int getPredictionForTapSuppression();

    /** Stores an Event ID from the server that we should persist along with user interactions. */
    void persistInteraction(long eventId);

    /* Gets the {@link ContextualSearchInteractionPersister} managed by this interface. */
    ContextualSearchInteractionPersister getInteractionPersister();

    /**
     * Resets the logger so that future log calls accumulate into a new record.
     * Any accumulated logging for the current record is discarded.
     */
    void reset();

    /**
     * Writes all the accumulated log entries and resets the logger so that future log calls
     * accumulate into a new record. This can be called multiple times without side-effects when
     * nothing new has been written to the log.
     * After calling this method another call to {@link #setupLoggingForPage} is required before
     * additional {@link #logFeature} or {@link #logOutcome} calls.
     */
    void writeLogAndReset();
}
