// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.HashSet;
import java.util.Set;
import java.util.regex.PatternSyntaxException;

/**
 * Class responsible for tracking the SendTabToSelf related notifications currently being displayed
 * to users through SharedPreferences. Currently, a notification consists of an version, id, and
 * guid all separated by an underscore. If the information to be serialized changes in the future,
 * the version should be incremented and the new serialization format should be incorporated along
 * with all previous versions.
 */
class NotificationSharedPrefManager {
    // Any time the serialization of the ActiveNotification needs to change, increment this version.
    private static final int VERSION = 1;

    /**
     * @return A non-negative integer greater than any active notification's notification ID. Once
     *         this id hits close to the INT_MAX_VALUE (unlikely), gets reset to 0.
     */
    static int getNextNotificationId() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        int nextId = prefs.readInt(ChromePreferenceKeys.SEND_TAB_TO_SELF_NEXT_NOTIFICATION_ID, -1);
        // Reset the counter when it gets close to max value
        if (nextId >= Integer.MAX_VALUE - 1) {
            nextId = -1;
        }
        nextId++;
        prefs.writeInt(ChromePreferenceKeys.SEND_TAB_TO_SELF_NEXT_NOTIFICATION_ID, nextId);
        return nextId;
    }

    /**
     * State of a SendTabToSelf notification currently being displayed to the user. It consists of
     * the notification id and GUID of the Share Entry.
     */
    static class ActiveNotification {
        public final int notificationId;
        @NonNull public final String guid;
        public final int version;

        ActiveNotification(int version, int notificationId, @NonNull String guid) {
            this.notificationId = notificationId;
            this.guid = guid;
            this.version = version;
        }

        ActiveNotification(int notificationId, @NonNull String guid) {
            this(VERSION, notificationId, guid);
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof ActiveNotification)) {
                return false;
            }
            ActiveNotification other = (ActiveNotification) obj;
            return this.notificationId == other.notificationId
                    && this.guid.equals(other.guid)
                    && this.version == other.version;
        }
    }

    /**
     * Takes the serialized version of the Notification and returns the ActiveNotification version
     * of it.
     *
     * @param notificationString The serialized version of the notification.
     * @return the deserialized version or null on failure.
     */
    @VisibleForTesting
    static ActiveNotification deserializeNotification(String notificationString) {
        try {
            String[] tokens = notificationString.split("_");
            if (tokens.length != 3) {
                return null;
            }
            // We can safely access all three tokens because trailing empty strings are discarded
            // when tokenizing thereby guaranteeing that the guid is available and non-null.
            return new ActiveNotification(
                    Integer.parseInt(tokens[0]), Integer.parseInt(tokens[1]), tokens[2]);
        } catch (NumberFormatException | PatternSyntaxException e) {
            return null;
        }
    }

    /** @return Serialized version of the fields in version_notificationId_guid string format */
    @VisibleForTesting
    static String serializeNotification(ActiveNotification notification) {
        return notification.version + "_" + notification.notificationId + "_" + notification.guid;
    }

    /**
     * Returns a mutable copy of the named pref. Never returns null.
     *
     * @param prefs The SharedPreferences to retrieve the set of strings from.
     * @param prefName The name of the preference to retrieve.
     * @return Existing set of strings associated with the prefName. If none exists, creates a new
     *         set.
     */
    private static @NonNull Set<String> getMutableStringSetPreference(
            SharedPreferencesManager prefs, String prefName) {
        Set<String> prefValue = prefs.readStringSet(prefName, null);
        if (prefValue == null) {
            return new HashSet<String>();
        }
        return new HashSet<String>(prefValue);
    }

    /**
     * Adds notification to the "active" set.
     *
     * @param notification Notification to be inserted into the active set.
     */
    static void addActiveNotification(ActiveNotification notification) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        Set<String> activeNotifications =
                getMutableStringSetPreference(
                        prefs, ChromePreferenceKeys.SEND_TAB_TO_SELF_ACTIVE_NOTIFICATIONS);
        boolean added = activeNotifications.add(serializeNotification(notification));
        if (added) {
            prefs.writeStringSet(
                    ChromePreferenceKeys.SEND_TAB_TO_SELF_ACTIVE_NOTIFICATIONS,
                    activeNotifications);
        }
    }

    /**
     * Removes notification from the "active" set.
     *
     * @param guid The GUID of the notification to remove.
     * @return whether the notification could be found in the active set and successfully removed.
     */
    static boolean removeActiveNotification(@Nullable String guid) {
        if (guid == null) {
            return false;
        }
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        ActiveNotification notification = findActiveNotification(guid);
        if (notification == null) {
            return false;
        }

        Set<String> activeNotifications =
                getMutableStringSetPreference(
                        prefs, ChromePreferenceKeys.SEND_TAB_TO_SELF_ACTIVE_NOTIFICATIONS);
        boolean removed = activeNotifications.remove(serializeNotification(notification));

        if (removed) {
            prefs.writeStringSet(
                    ChromePreferenceKeys.SEND_TAB_TO_SELF_ACTIVE_NOTIFICATIONS,
                    activeNotifications);
        }
        return removed;
    }

    /**
     * Returns an ActiveNotification corresponding to the GUID.
     *
     * @param guid The GUID of the notification to retrieve.
     * @return The Active Notification associated with the passed in GUID. May be null if none
     *         found.
     */
    static @Nullable ActiveNotification findActiveNotification(@Nullable String guid) {
        if (guid == null) {
            return null;
        }
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        Set<String> activeNotifications =
                prefs.readStringSet(
                        ChromePreferenceKeys.SEND_TAB_TO_SELF_ACTIVE_NOTIFICATIONS, null);
        if (activeNotifications == null) {
            return null;
        }

        for (String serialized : activeNotifications) {
            ActiveNotification activeNotification = deserializeNotification(serialized);
            if ((activeNotification != null) && guid.equals(activeNotification.guid)) {
                return activeNotification;
            }
        }
        return null;
    }
}
