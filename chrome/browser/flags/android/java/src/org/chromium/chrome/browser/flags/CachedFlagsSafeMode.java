// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

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

    private @Behavior int mBehavior = Behavior.UNKNOWN;

    CachedFlagsSafeMode() {}

    /**
     * Call right before any flag is checked. The first time this is called, check if safe mode
     * should be engaged, and engages it if necessary.
     */
    void onFlagChecked() {
        if (mBehavior == Behavior.UNKNOWN) {
            if (shouldEnterSafeMode()) {
                String cachedVersion = SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.FLAGS_CACHED_SAFE_VALUES_VERSION, "");
                if (cachedVersion.isEmpty()) {
                    mBehavior = Behavior.ENGAGED_WITHOUT_SAFE_VALUES;
                } else if (!cachedVersion.equals(ChromeVersionInfo.getProductVersion())) {
                    mBehavior = Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES;
                } else {
                    mBehavior = Behavior.ENGAGED_WITH_SAFE_VALUES;
                }
                RecordHistogram.recordEnumeratedHistogram(
                        "Variations.SafeModeCachedFlags.Engaged", mBehavior, Behavior.NUM_ENTRIES);
                engageSafeModeInNative();
                restoreSafeValues();
            } else {
                mBehavior = Behavior.NOT_ENGAGED_BELOW_THRESHOLD;
                RecordHistogram.recordEnumeratedHistogram(
                        "Variations.SafeModeCachedFlags.Engaged", mBehavior, Behavior.NUM_ENTRIES);
            }
        }
    }

    /**
     * Call at an early point in the path that leads to caching flags. If onFinishedCachingFlags()
     * does not get called before the next run, this run will be considered a crash for purposes of
     * counting the crash streak and entering Safe Mode.
     */
    public void onStartCheckpoint() {
        SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.FLAGS_CRASH_STREAK_BEFORE_CACHE);
    }

    /**
     * Call when all flags have been cached. Signals that the current configuration is safe. It will
     * be saved to be used in Safe Mode.
     */
    void onEndCheckpoint(ValuesReturned safeValuesReturned) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.FLAGS_CRASH_STREAK_BEFORE_CACHE, 0);
        writeSafeValues(safeValuesReturned);
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
                "Variations.SafeModeCached.Streak.Crashes", crashStreak, 50);

        if (crashStreak >= CRASH_STREAK_TO_ENTER_SAFE_MODE) {
            Log.e(TAG, "Enter Safe Mode for CachedFlags, crash streak is %d.", crashStreak);
            return true;
        } else {
            return false;
        }
    }

    private void writeSafeValues(ValuesReturned safeValuesReturned) {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();

        Map<String, Boolean> boolValuesToWrite = prependPrefixToKeys(
                ChromePreferenceKeys.FLAGS_CACHED_SAFE_VALUES_BOOL, safeValuesReturned.boolMap());
        prefs.writeBooleans(boolValuesToWrite);

        Map<String, Integer> intValuesToWrite = prependPrefixToKeys(
                ChromePreferenceKeys.FLAGS_CACHED_SAFE_VALUES_INT, safeValuesReturned.intMap());
        prefs.writeInts(intValuesToWrite);

        Map<String, Double> doubleValuesToWrite =
                prependPrefixToKeys(ChromePreferenceKeys.FLAGS_CACHED_SAFE_VALUES_DOUBLE,
                        safeValuesReturned.doubleMap());
        prefs.writeDoubles(doubleValuesToWrite);

        Map<String, String> stringValuesToWrite =
                prependPrefixToKeys(ChromePreferenceKeys.FLAGS_CACHED_SAFE_VALUES_STRING,
                        safeValuesReturned.stringMap());
        stringValuesToWrite.put(ChromePreferenceKeys.FLAGS_CACHED_SAFE_VALUES_VERSION,
                ChromeVersionInfo.getProductVersion());
        prefs.writeStrings(stringValuesToWrite);

        RecordHistogram.recordEnumeratedHistogram(
                "Variations.SafeModeCachedFlags.Cached", mBehavior, Behavior.NUM_ENTRIES);
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
        return mBehavior;
    }

    void clearForTesting() {
        mBehavior = Behavior.UNKNOWN;
    }
}
