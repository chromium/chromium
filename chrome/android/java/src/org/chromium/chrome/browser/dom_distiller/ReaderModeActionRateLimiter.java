// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;

import java.util.concurrent.TimeUnit;

/** A class to manage rate limiting of the reader mode contextual page action. */
@NullMarked
public class ReaderModeActionRateLimiter {
    private static final int INVALID_TIME = -1;

    /** The number of times the CPA can be shown without interaction before being suppressed. */
    private static final int SHOW_LIMIT = 3;

    /** The window of time to track the CPA being shown. */
    private static final long TRACKING_WINDOW_MS = TimeUnit.DAYS.toMillis(1);

    /** The window of time to suppress the CPA after it's been shown without interaction. */
    private static final long SUPPRESSION_DURATION_MS = TimeUnit.DAYS.toMillis(3);

    @Nullable private static ReaderModeActionRateLimiter sInstance;

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

    private ReaderModeActionRateLimiter() {}

    /**
     * Checks if the reader mode action should be suppressed.
     *
     * @return True if the action should be suppressed, false otherwise.
     */
    public boolean isActionSuppressed() {
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
        if (System.currentTimeMillis() - firstShownTimestamp > TRACKING_WINDOW_MS) {
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

        // If the CPA has been shown too many times, then suppress it for a window of time.
        if (showCount >= SHOW_LIMIT) {
            long suppressionEnd = System.currentTimeMillis() + SUPPRESSION_DURATION_MS;
            prefs.writeLong(
                    ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP,
                    suppressionEnd);
        }
    }

    /** Called when the reader mode action is clicked. */
    public void onActionClicked() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT);
        prefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP);
        prefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP);
    }

    // Testing methods.

    public static void setInstanceForTesting(ReaderModeActionRateLimiter instance) {
        sInstance = instance;
    }
}
