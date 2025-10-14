// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.chromium.base.ObserverList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;

/**
 * A class to manage rate limiting of the reader mode contextual page action. This works by:
 *
 * <ol>
 *   <li>Allowing a certain number of CPA show events (READER_MODE_ACTION_SHOW_COUNT) within a
 *       certain window of time (READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP).
 *   <li>If the READER_MODE_ACTION_SHOW_COUNT exceeds the limit in the given window then the CPA is
 *       temporarily suppressed for a certain period of time
 *       (READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP).
 *   <li>If the CPA is temporarily suppressed more then the allowable amount
 *       (READER_MODE_ACTION_SUPPRESSION_COUNT), then it's permanently suppressed.
 * </ol>
 *
 * <p>Each of these parameters are configurable within finch in the following manner:
 *
 * <ul>
 *   <li>READER_MODE_ACTION_SHOW_COUNT - cpa_show_limit.
 *   <li>READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP - tracking_window_ms.
 *   <li>READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP - suppression_window_ms
 *   <li>READER_MODE_ACTION_SUPPRESSION_COUNT - suppression_limit
 * </ul>
 */
@NullMarked
public class ReaderModeActionRateLimiter {
    private static final int INVALID_TIME = -1;
    @Nullable private static ReaderModeActionRateLimiter sInstance;

    public interface Observer {
        /** Called when the contextual page action was just shown. */
        default void onActionShown() {}
    }

    /** No-op implementation for when the feature is off. */
    private static class EmptyLimiter extends ReaderModeActionRateLimiter {
        @Override
        public boolean isActionSuppressed() {
            return false;
        }

        @Override
        public void onActionShown() {}

        @Override
        public void onActionClicked() {}
    }

    public static ReaderModeActionRateLimiter getInstance() {
        if (sInstance == null) {
            sInstance =
                    DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()
                            ? new ReaderModeActionRateLimiter()
                            : new EmptyLimiter();
        }

        return sInstance;
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private ReaderModeActionRateLimiter() {}

    /** Adds an observer */
    public void addObserver(Observer obs) {
        mObservers.addObserver(obs);
    }

    /** Removes an observer */
    public void removeObserver(Observer obs) {
        mObservers.removeObserver(obs);
    }

    /**
     * Checks if the reader mode action should be suppressed.
     *
     * @return True if the action should be suppressed, false otherwise.
     */
    public boolean isActionSuppressed() {
        // If the action has been suppressed past the allowable count, then permanently suppress it
        // until the user interacts with the feature.
        if (ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_COUNT, 0)
                >= DomDistillerFeatures.sReaderModeDistillInAppSuppressionLimit.getValue()) {
            return true;
        }

        long suppressionEnd =
                ChromeSharedPreferences.getInstance()
                        .readLong(
                                ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP,
                                INVALID_TIME);
        return System.currentTimeMillis() < suppressionEnd;
    }

    /** Called when the reader mode action is shown. */
    public void onActionShown() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        long firstShownTimestamp =
                prefs.readLong(
                        ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP,
                        INVALID_TIME);
        int showCount = prefs.readInt(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT, 0);

        // The tracking window has elapsed, reset the variables.
        if (System.currentTimeMillis() - firstShownTimestamp
                > DomDistillerFeatures.sReaderModeDistillInAppTrackingWindowMs.getValue()) {
            showCount = 0;
            firstShownTimestamp = INVALID_TIME;
        }

        // The tracking window is unset, use the current timestamp.
        if (firstShownTimestamp == INVALID_TIME) {
            firstShownTimestamp = System.currentTimeMillis();
            prefs.writeLong(
                    ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP,
                    firstShownTimestamp);
        }

        showCount++;
        prefs.writeInt(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT, showCount);
        if (showCount >= DomDistillerFeatures.sReaderModeDistillInAppCpaShowLimit.getValue()) {
            startTemporarySuppression(prefs);
        }

        for (Observer obs : mObservers) {
            obs.onActionShown();
        }
    }

    /** Called when the reader mode action is clicked. */
    public void onActionClicked() {
        resetTemporarySuppression(ChromeSharedPreferences.getInstance());
    }

    /**
     * Starts the temporary suppression window by:
     *
     * <ol>
     *   <li>Calculate the suppression ending timestamp, and store it.
     *   <li>Increment the number of times that the CPA has been suppressed and store it.
     * </ol>
     */
    private void startTemporarySuppression(SharedPreferencesManager prefs) {
        long suppressionEnd =
                System.currentTimeMillis()
                        + DomDistillerFeatures.sReaderModeDistillInAppSuppressionWindowMs
                                .getValue();
        prefs.writeLong(
                ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP, suppressionEnd);
        prefs.writeInt(
                ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_COUNT,
                prefs.readInt(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_COUNT, 0) + 1);
    }

    /** Reset all temporary suppression variables. */
    private void resetTemporarySuppression(SharedPreferencesManager prefs) {
        prefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT);
        prefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP);
        prefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP);
    }

    // Testing methods.

    public static void setInstanceForTesting(ReaderModeActionRateLimiter instance) {
        sInstance = instance;
    }
}
