// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import org.chromium.base.ContextUtils;

/**
 * Preferences used to provide prefetch related notifications.
 *  - Having new pages: boolean indicating whether new pages have been saved or not
 *  - Notification timestamp: the last time a notification is shown
 *  - Offline counter: how many times the task ran and seen that we are offline
 *  - Ignored notification counter: how many times in a row we showed a notification without user
 *    reacting to it
 */
public class PrefetchPrefs {
    static final String PREF_PREFETCH_NOTIFICATION_ENABLED = "prefetch_notification_enabled";
    static final String PREF_PREFETCH_HAS_NEW_PAGES = "prefetch_notification_has_new_pages";
    static final String PREF_PREFETCH_NOTIFICATION_TIME = "prefetch_notification_shown_time";
    static final String PREF_PREFETCH_OFFLINE_COUNTER = "prefetch_notification_offline_counter";
    static final String PREF_PREFETCH_IGNORED_NOTIFICATION_COUNTER =
            "prefetch_notification_ignored_counter";

    /**
     * Sets the flag to tell whether prefetch notifications are enabled in user settings.
     */
    public static void setNotificationEnabled(boolean enabled) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(PREF_PREFETCH_NOTIFICATION_ENABLED, enabled)
                .apply();
    }

    /**
     * Returns the flag to tell whether prefetch notifications are enabled in user settings.
     */
    public static boolean getNotificationEnabled() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                PREF_PREFETCH_NOTIFICATION_ENABLED, true);
    }

    /**
     * Sets the flag to tell whether new pages have been saved.
     */
    public static void setHasNewPages(boolean hasNewPages) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(PREF_PREFETCH_HAS_NEW_PAGES, hasNewPages)
                .apply();
    }

    /**
     * Returns the flag to tell whether new pages have been saved.
     */
    public static boolean getHasNewPages() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                PREF_PREFETCH_HAS_NEW_PAGES, false);
    }

    /**
     * Sets the last time a notification is shown, in milliseconds since the epoch.
     */
    public static void setNotificationLastShownTime(long timeInMillis) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(PREF_PREFETCH_NOTIFICATION_TIME, timeInMillis)
                .apply();
    }

    /**
     * Returns the last time a notification is shown, in milliseconds since the epoch.
     */
    public static long getNotificationLastShownTime() {
        return ContextUtils.getAppSharedPreferences().getLong(PREF_PREFETCH_NOTIFICATION_TIME, 0);
    }

    /**
     * Sets the offline counter.
     */
    public static void setOfflineCounter(int counter) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(PREF_PREFETCH_OFFLINE_COUNTER, counter)
                .apply();
    }

    /**
     * Returns the offline counter.
     */
    public static int getOfflineCounter() {
        return ContextUtils.getAppSharedPreferences().getInt(PREF_PREFETCH_OFFLINE_COUNTER, 0);
    }

    /**
     * Sets the ignored notification counter.
     */
    public static void setIgnoredNotificationCounter(int counter) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(PREF_PREFETCH_IGNORED_NOTIFICATION_COUNTER, counter)
                .apply();
    }

    /**
     * Returns the ignored notification counter.
     */
    public static int getIgnoredNotificationCounter() {
        return ContextUtils.getAppSharedPreferences().getInt(
                PREF_PREFETCH_IGNORED_NOTIFICATION_COUNTER, 0);
    }
}
