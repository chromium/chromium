// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.Map;

/**
 * Manages persisted instance state. This includes information pertinent to an instance that may be
 * active (ie. a Chrome window associated with a live activity / task), inactive, or recently closed
 * by the user.
 */
@NullMarked
public class MultiInstancePersistentStore {
    private static @MonotonicNonNull SharedPreferencesManager sPrefsManager;

    private MultiInstancePersistentStore() {}

    private static SharedPreferencesManager getManager() {
        if (sPrefsManager == null) {
            sPrefsManager = ChromeSharedPreferences.getInstance();
        }
        return sPrefsManager;
    }

    static Map<String, Integer> readTaskMap() {
        return getManager().readIntsWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
    }

    static int readTaskId(int instanceId) {
        return getManager().readInt(taskIdKey(instanceId), MultiInstanceManager.INVALID_TASK_ID);
    }

    static void writeTaskId(int instanceId, int taskId) {
        getManager().writeInt(taskIdKey(instanceId), taskId);
    }

    static void removeTaskId(int instanceId) {
        getManager().removeKey(taskIdKey(instanceId));
    }

    private static String taskIdKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(String.valueOf(instanceId));
    }
}
