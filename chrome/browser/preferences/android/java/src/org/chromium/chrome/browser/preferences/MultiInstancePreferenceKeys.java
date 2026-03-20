// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.NullMarked;

import java.util.Arrays;
import java.util.List;

/**
 * Contains String and {@link KeyPrefix} constants with the SharedPreferences keys used for
 * multi-instance support.
 */
@NullMarked
public final class MultiInstancePreferenceKeys {
    // {Instance:Task} ID mapping for multi-instance support.
    public static final KeyPrefix MULTI_INSTANCE_TASK_MAP =
            new KeyPrefix("Chrome.MultiInstance.TaskMap.*");
    public static final String MULTI_WINDOW_START_TIME = "Chrome.MultiWindow.StartTime";
    public static final String MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM =
            "Chrome.MultiWindow.CloseWindowSkipConfirm";

    public static final String MULTI_INSTANCE_MAX_INSTANCE_LIMIT =
            "Chrome.MultiWindow.MaxInstanceLimit";
    public static final String MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED =
            "Chrome.MultiWindow.InstanceLimitDowngradeTriggered";
    public static final KeyPrefix MULTI_INSTANCE_PROFILE_TYPE =
            new KeyPrefix("Chrome.MultiInstance.ProfileType.*");
    public static final KeyPrefix MULTI_INSTANCE_LATEST_PERSISTENT_STATE_ID =
            new KeyPrefix("Chrome.MultiInstance.LatestPersistentStateId.*");

    public static final String MULTI_INSTANCE_START_TIME = "Chrome.MultiInstance.StartTime";

    // Start timestamp of 1-day period for measuring the max count of instances used simultaneously.
    public static final String MULTI_INSTANCE_MAX_COUNT_TIME = "Chrome.MultiInstance.MaxCountTime";
    // Max count of active Chrome instances used in a day.
    public static final String MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT =
            "Chrome.MultiInstance.MaxActiveInstanceCount";
    // Max count of Chrome instances used in a day.
    public static final String MULTI_INSTANCE_MAX_INSTANCE_COUNT =
            "Chrome.MultiInstance.MaxInstanceCount";
    // Max count of Chrome Incognito instances used in a day.
    public static final String MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO =
            "Chrome.MultiInstance.MaxInstanceCountIncognito";
    // Information on each instance.
    public static final KeyPrefix MULTI_INSTANCE_INCOGNITO_TAB_COUNT =
            new KeyPrefix("Chrome.MultiInstance.IncognitoTabCount.*");
    public static final KeyPrefix MULTI_INSTANCE_IS_INCOGNITO_SELECTED =
            new KeyPrefix("Chrome.MultiInstance.IsIncognitoSelected.*");
    public static final KeyPrefix MULTI_INSTANCE_TAB_COUNT =
            new KeyPrefix("Chrome.MultiInstance.TabCount.*"); // Normal tab count

    // The total tab count at the time Chrome is shut down for use during relaunch. This value may
    // not be accurate if Chrome remains active in the foreground or background without being
    // terminated.
    public static final KeyPrefix MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH =
            new KeyPrefix("Chrome.MultiInstance.TabCountForRelaunch.*");
    // The default window title, equivalent to the active tab title.
    public static final KeyPrefix MULTI_INSTANCE_TITLE =
            new KeyPrefix("Chrome.MultiInstance.Title.*");
    // A custom window title set by the user.
    public static final KeyPrefix MULTI_INSTANCE_CUSTOM_TITLE =
            new KeyPrefix("Chrome.MultiInstance.CustomTitle.*");
    public static final KeyPrefix MULTI_INSTANCE_LAST_ACCESSED_TIME =
            new KeyPrefix("Chrome.MultiInstance.LastAccessedTime.*");
    public static final KeyPrefix MULTI_INSTANCE_CLOSURE_TIME =
            new KeyPrefix("Chrome.MultiInstance.ClosureTime.*");
    public static final KeyPrefix MULTI_INSTANCE_URL = new KeyPrefix("Chrome.MultiInstance.Url.*");
    public static final KeyPrefix MULTI_INSTANCE_MARKED_FOR_DELETION =
            new KeyPrefix("Chrome.MultiInstance.MarkedForDeletion.*");

    // Start timestamp of 1-day period for measuring the duration of disjoint time spent in various
    // windowing modes.
    public static final String MULTI_WINDOW_MODE_CYCLE_START_TIME =
            "Chrome.MultiWindowMode.CycleStartTime3";
    // Start timestamp of the current windowing mode.
    public static final KeyPrefix MULTI_WINDOW_MODE_START_TIME =
            new KeyPrefix("Chrome.MultiWindowMode.StartTime3.*");
    // Tracks window IDs of activities in a given windowing mode.
    public static final KeyPrefix MULTI_WINDOW_MODE_ACTIVITIES =
            new KeyPrefix("Chrome.MultiWindowMode.Activities3.*");
    // Aggregated duration of time spent in a given windowing mode.
    public static final KeyPrefix MULTI_WINDOW_MODE_DURATION_MS =
            new KeyPrefix("Chrome.MultiWindowMode.DurationMs3.*");

    /**
     * Returns The list of [keys in use] conforming to the format.
     */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> getKeysInUse() {
        return Arrays.asList(
                MULTI_INSTANCE_TASK_MAP.pattern(),
                MULTI_WINDOW_START_TIME,
                MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM,
                MULTI_INSTANCE_MAX_INSTANCE_LIMIT,
                MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                MULTI_INSTANCE_PROFILE_TYPE.pattern(),
                MULTI_INSTANCE_LATEST_PERSISTENT_STATE_ID.pattern(),
                MULTI_INSTANCE_START_TIME,
                MULTI_INSTANCE_MAX_COUNT_TIME,
                MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT,
                MULTI_INSTANCE_MAX_INSTANCE_COUNT,
                MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO,
                MULTI_INSTANCE_INCOGNITO_TAB_COUNT.pattern(),
                MULTI_INSTANCE_IS_INCOGNITO_SELECTED.pattern(),
                MULTI_INSTANCE_TAB_COUNT.pattern(),
                MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH.pattern(),
                MULTI_INSTANCE_TITLE.pattern(),
                MULTI_INSTANCE_CUSTOM_TITLE.pattern(),
                MULTI_INSTANCE_LAST_ACCESSED_TIME.pattern(),
                MULTI_INSTANCE_CLOSURE_TIME.pattern(),
                MULTI_INSTANCE_URL.pattern(),
                MULTI_INSTANCE_MARKED_FOR_DELETION.pattern(),
                MULTI_WINDOW_MODE_CYCLE_START_TIME,
                MULTI_WINDOW_MODE_START_TIME.pattern(),
                MULTI_WINDOW_MODE_ACTIVITIES.pattern(),
                MULTI_WINDOW_MODE_DURATION_MS.pattern());
    }

    private MultiInstancePreferenceKeys() {}
}
