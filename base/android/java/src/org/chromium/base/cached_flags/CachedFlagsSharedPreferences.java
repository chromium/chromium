// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.BuildConfig;

import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/** Shared preferences used by org.chromium.base.cached_flags */
public class CachedFlagsSharedPreferences {
    private static final String TAG = "CachedFlags";

    /** CachedFlags store flag values for the next run with this prefix. */
    public static final KeyPrefix FLAGS_CACHED = new KeyPrefix("Chrome.Flags.CachedFlag.*");

    /** CachedFieldTrialParameters store parameter values for the next run with this prefix. */
    public static final KeyPrefix FLAGS_FIELD_TRIAL_PARAM_CACHED =
            new KeyPrefix("Chrome.Flags.FieldTrialParamCached.*");

    /**
     * Streak of crashes before caching flags from native. This controls Safe Mode for Cached Flags.
     */
    public static final String FLAGS_CRASH_STREAK_BEFORE_CACHE =
            "Chrome.Flags.CrashStreakBeforeCache";

    /** How many runs of Safe Mode for Cached Flags are left before trying a normal run. */
    public static final String FLAGS_SAFE_MODE_RUNS_LEFT = "Chrome.Flags.SafeModeRunsLeft";

    public static final PreferenceKeyRegistry REGISTRY =
            (BuildConfig.ENABLE_ASSERTS
                    ? new PreferenceKeyRegistry(
                            /* module= */ "cached_flags",
                            /* keysInUse= */ List.of(
                                    FLAGS_CACHED.pattern(),
                                    FLAGS_CRASH_STREAK_BEFORE_CACHE,
                                    FLAGS_FIELD_TRIAL_PARAM_CACHED.pattern(),
                                    FLAGS_SAFE_MODE_RUNS_LEFT),
                            /* legacyKeys= */ Collections.EMPTY_LIST,
                            /* legacyPrefixes= */ Collections.EMPTY_LIST)
                    : null);

    public static SharedPreferencesManager getInstance() {
        return SharedPreferencesManager.getInstanceForRegistry(REGISTRY);
    }

    /**
     * @return A human-readable string uniquely identifying the field trial parameter.
     */
    private static String generateParamFullName(String featureName, String parameterName) {
        return featureName + ":" + parameterName;
    }

    public static String generateParamSharedPreferenceKey(
            String featureName, String parameterName) {
        return FLAGS_FIELD_TRIAL_PARAM_CACHED.createKey(
                generateParamFullName(featureName, parameterName));
    }

    public static String encodeParams(Map<String, String> params) {
        return new JSONObject(params).toString();
    }

    /** Decodes a previously encoded map. Returns empty map on parse error. */
    public static Map<String, String> decodeJsonEncodedMap(String value) {
        Map<String, String> resultingMap = new HashMap<>();
        if (value.isEmpty()) {
            return resultingMap;
        }
        try {
            final JSONObject json = new JSONObject(value);
            Iterator<String> keys = json.keys();
            while (keys.hasNext()) {
                final String key = keys.next();
                resultingMap.put(key, json.getString(key));
            }
            return resultingMap;
        } catch (JSONException e) {
            Log.e(TAG, "Error parsing JSON", e);
            return new HashMap<>();
        }
    }
}
