// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.KeyPrefix;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.version.ChromeVersionInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Controls Safe Mode for {@link CachedFeatureFlags}.
 *
 * Safe Mode is a mechanism that allows Chrome to prevent crashes gated behind flags used before
 * native from becoming a crash loop that cannot be recovered from by disabling the experiment.
 *
 * TODO(crbug.com/1217708): Safe mode at the moment does not engage. Validate the crash streak logic
 * in Canary before turning it on.
 */
class CachedFlagsSafeMode {
    private static final String TAG = "Flags";
    private static final int CRASH_STREAK_TO_ENTER_SAFE_MODE = 2;

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @VisibleForTesting
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({Behavior.UNKNOWN, Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
            Behavior.ENGAGED_WITH_SAFE_VALUES, Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES,
            Behavior.ENGAGED_WITHOUT_SAFE_VALUES})
    @interface Behavior {
        int UNKNOWN = 0;
        int NOT_ENGAGED_BELOW_THRESHOLD = 1;
        int ENGAGED_WITH_SAFE_VALUES = 2;
        int ENGAGED_IGNORING_OUTDATED_SAFE_VALUES = 3;
        int ENGAGED_WITHOUT_SAFE_VALUES = 4;

        int NUM_ENTRIES = 5;
    }

    private AtomicInteger mBehavior = new AtomicInteger(Behavior.UNKNOWN);

    CachedFlagsSafeMode() {}

    /**
     * Call right before any flag is checked. The first time this is called, check if safe mode
     * should be engaged, and engages it if necessary.
     */
    @AnyThread
    void onFlagChecked() {
        synchronized (mBehavior) {
            if (mBehavior.get() != Behavior.UNKNOWN) return;
            if (shouldEnterSafeMode()) {
                String cachedVersion = SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.FLAGS_CACHED_SAFE_VALUES_VERSION, "");
                int behavior;
                if (cachedVersion.isEmpty()) {
                    behavior = Behavior.ENGAGED_WITHOUT_SAFE_VALUES;
                } else if (!cachedVersion.equals(ChromeVersionInfo.getProductVersion())) {
                    behavior = Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES;
                } else {
                    behavior = Behavior.ENGAGED_WITH_SAFE_VALUES;
                }
                mBehavior.set(behavior);
                RecordHistogram.recordEnumeratedHistogram(
                        "Variations.SafeModeCachedFlags.Engaged", behavior, Behavior.NUM_ENTRIES);
                engageSafeModeInNative();
                restoreSafeValues();
            } else {
                mBehavior.set(Behavior.NOT_ENGAGED_BELOW_THRESHOLD);
                RecordHistogram.recordEnumeratedHistogram("Variations.SafeModeCachedFlags.Engaged",
                        Behavior.NOT_ENGAGED_BELOW_THRESHOLD, Behavior.NUM_ENTRIES);
            }
        }
    }

    /**
     * Call at an early point in the path that leads to caching flags. If onFinishedCachingFlags()
     * does not get called before the next run, this run will be considered a crash for purposes of
     * counting the crash streak and entering Safe Mode.
     */
    public void onStartOrResumeCheckpoint() {
        SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.FLAGS_CRASH_STREAK_BEFORE_CACHE);
        RecordHistogram.recordEnumeratedHistogram(
                "Variations.SafeModeCachedFlags.WillCache", mBehavior.get(), Behavior.NUM_ENTRIES);
    }

    /**
     * Call when aborting a path that leads to caching flags. Rolls back the crash streak
     * incremented in {@link #onStartOrResumeCheckpoint} but does not reset it.
     */
    public void onPauseCheckpoint() {
        int currentStreak = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.FLAGS_CRASH_STREAK_BEFORE_CACHE);
        assert currentStreak >= 0;
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.FLAGS_CRASH_STREAK_BEFORE_CACHE, currentStreak - 1);
        RecordHistogram.recordEnumeratedHistogram(
                "Variations.SafeModeCachedFlags.Pause", mBehavior.get(), Behavior.NUM_ENTRIES);
    }

    /**
     * Call when all flags have been cached. Signals that the current configuration is safe. It will
     * be saved to be used in Safe Mode.
     */
    void onEndCheckpoint(ValuesReturned safeValuesReturned) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.FLAGS_CRASH_STREAK_BEFORE_CACHE, 0);
        writeSafeValues(safeValuesReturned);
        RecordHistogram.recordEnumeratedHistogram(
                "Variations.SafeModeCachedFlags.Cached", mBehavior.get(), Behavior.NUM_ENTRIES);
    }

    private void engageSafeModeInNative() {
        // TODO(crbug.com/1217708): Notify native that a safe seed should be used.
    }

    private void restoreSafeValues() {
        // TODO(crbug.com/1217708): Overwrite cached values with safe values.
        // TODO(crbug.com/1217708): Ignore safe values from previous versions.
        // TODO(crbug.com/1217708): Fallback to default values.
    }

    private boolean shouldEnterSafeMode() {
        int crashStreak = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.FLAGS_CRASH_STREAK_BEFORE_CACHE, 0);
        RecordHistogram.recordExactLinearHistogram(
                "Variations.SafeModeCachedFlags.Streak.Crashes", crashStreak, 50);

        if (crashStreak >= CRASH_STREAK_TO_ENTER_SAFE_MODE) {
            Log.e(TAG, "Enter Safe Mode for CachedFlags, crash streak is %d.", crashStreak);
            return true;
        } else {
            return false;
        }
    }

    private void writeSafeValues(ValuesReturned safeValuesReturned) {
        // TODO(crbug.com/1217708): Write safe values.
    }

    private static <T> Map<String, T> prependPrefixToKeys(KeyPrefix prefix, Map<String, T> map) {
        Map<String, T> prefixed = new HashMap<>();
        for (Map.Entry<String, T> kv : map.entrySet()) {
            String safeKey = prefix.createKey(kv.getKey());
            prefixed.put(safeKey, kv.getValue());
        }
        return prefixed;
    }

    @Behavior
    int getBehaviorForTesting() {
        return mBehavior.get();
    }

    void clearForTesting() {
        mBehavior.set(Behavior.UNKNOWN);
    }
}
