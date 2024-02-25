// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.BuildConfig;

import java.util.Collections;
import java.util.List;

/** Shared preferences used by org.chromium.base.cached_flags */
public class CachedFlagsSharedPreferences {
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
}
