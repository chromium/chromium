// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Manages persisted instance state. This includes information pertinent to an instance that may be
 * active (ie. a Chrome window associated with a live activity / task), inactive, or recently closed
 * by the user.
 */
@NullMarked
class MultiInstancePersistentStore {
    private static @MonotonicNonNull SharedPreferencesManager sPrefsManager;

    private MultiInstancePersistentStore() {}

    private static SharedPreferencesManager getManager() {
        if (sPrefsManager == null) {
            sPrefsManager = ChromeSharedPreferences.getInstance();
        }
        return sPrefsManager;
    }

    static Set<Integer> readAllInstanceIds() {
        // We arbitrarily choose to use the lastAccessedTime map to extract persisted instance ids
        // from the SharedPreferences key suffix, from among the SharedPreferences that definitely
        // continue to persist across activity kills / restarts. The taskMap can be cleared when an
        // activity is destroyed and during invalid instance data cleanup which is why we will not
        // use the same to extract ids.
        Map<String, Long> lastAccessedTimeMap =
                ChromeSharedPreferences.getInstance()
                        .readLongsWithPrefix(
                                ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME);
        Pattern pattern = Pattern.compile("(\\d+)$");

        Set<Integer> ids = new HashSet<>();

        for (String prefKey : lastAccessedTimeMap.keySet()) {
            Matcher matcher = pattern.matcher(prefKey);
            boolean matchFound = matcher.find();
            assert matchFound : "Key should be suffixed with the instance id.";
            int id = Integer.parseInt(matcher.group(1));
            ids.add(id);
        }

        return ids;
    }

    static boolean hasInstance(int instanceId) {
        return readLastAccessedTime(instanceId) != 0;
    }

    static long readLastAccessedTime(int instanceId) {
        return getManager().readLong(lastAccessedTimeKey(instanceId));
    }

    static void writeLastAccessedTime(int instanceId) {
        getManager().writeLong(lastAccessedTimeKey(instanceId), TimeUtils.currentTimeMillis());
    }

    static void removeLastAccessedTime(int instanceId) {
        getManager().removeKey(lastAccessedTimeKey(instanceId));
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

    static void writeTabCount(int instanceId, int normalTabCount, int incognitoTabCount) {
        getManager().writeInt(normalTabCountKey(instanceId), normalTabCount);
        getManager().writeInt(incognitoTabCountKey(instanceId), incognitoTabCount);
    }

    static void removeTabCount(int instanceId) {
        getManager().removeKey(normalTabCountKey(instanceId));
        getManager().removeKey(incognitoTabCountKey(instanceId));
    }

    static int readNormalTabCount(int instanceId) {
        return getManager().readInt(normalTabCountKey(instanceId));
    }

    static int readIncognitoTabCount(int instanceId) {
        return getManager().readInt(incognitoTabCountKey(instanceId));
    }

    static int readTabCountForRelaunch(int instanceId) {
        return getManager().readInt(tabCountForRelaunchKey(instanceId));
    }

    static void writeTabCountForRelaunchSync(int instanceId, int tabCount) {
        getManager().writeIntSync(tabCountForRelaunchKey(instanceId), tabCount);
    }

    static void removeTabCountForRelaunch(int instanceId) {
        getManager().removeKey(tabCountForRelaunchKey(instanceId));
    }

    static @Nullable String readActiveTabUrl(int instanceId) {
        return getManager().readString(urlKey(instanceId), null);
    }

    static void writeActiveTabUrl(int instanceId, String url) {
        getManager().writeString(urlKey(instanceId), url);
    }

    static void removeActiveTabUrl(int instanceId) {
        getManager().removeKey(urlKey(instanceId));
    }

    static @Nullable String readActiveTabTitle(int instanceId) {
        return getManager().readString(activeTabTitleKey(instanceId), null);
    }

    static void writeActiveTabTitle(int instanceId, String title) {
        getManager().writeString(activeTabTitleKey(instanceId), title);
    }

    static void removeActiveTabTitle(int instanceId) {
        getManager().removeKey(activeTabTitleKey(instanceId));
    }

    static @Nullable String readCustomTitle(int instanceId) {
        return getManager().readString(customTitleKey(instanceId), null);
    }

    static void writeCustomTitle(int instanceId, String title) {
        getManager().writeString(customTitleKey(instanceId), title);
    }

    static void removeCustomTitle(int instanceId) {
        getManager().removeKey(customTitleKey(instanceId));
    }

    private static String lastAccessedTimeKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME.createKey(
                String.valueOf(instanceId));
    }

    private static String taskIdKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(String.valueOf(instanceId));
    }

    private static String normalTabCountKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(String.valueOf(instanceId));
    }

    private static String incognitoTabCountKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT.createKey(
                String.valueOf(instanceId));
    }

    private static String tabCountForRelaunchKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH.createKey(
                String.valueOf(instanceId));
    }

    private static String urlKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_URL.createKey(String.valueOf(instanceId));
    }

    private static String activeTabTitleKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_TITLE.createKey(String.valueOf(instanceId));
    }

    private static String customTitleKey(int instanceId) {
        return ChromePreferenceKeys.MULTI_INSTANCE_CUSTOM_TITLE.createKey(
                String.valueOf(instanceId));
    }
}
