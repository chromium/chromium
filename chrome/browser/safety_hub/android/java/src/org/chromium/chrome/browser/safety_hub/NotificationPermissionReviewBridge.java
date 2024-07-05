// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

import java.util.Arrays;
import java.util.List;

public class NotificationPermissionReviewBridge {
    interface Observer {
        void notificationPermissionsChanged();
    }

    private static ProfileKeyedMap<NotificationPermissionReviewBridge> sProfileMap;

    private final Profile mProfile;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    static NotificationPermissionReviewBridge getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileMap.getForProfile(profile, NotificationPermissionReviewBridge::new);
    }

    NotificationPermissionReviewBridge(Profile profile) {
        mProfile = profile;
    }

    void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** Returns a list of NotificationPermissions to review sorted by priority. */
    List<NotificationPermissions> getNotificationPermissions() {
        return Arrays.asList(
                NotificationPermissionReviewBridgeJni.get().getNotificationPermissions(mProfile));
    }

    /** Ignores the given origin for notification permission review. */
    void ignoreOriginForNotificationPermissionReview(String origin) {
        NotificationPermissionReviewBridgeJni.get()
                .ignoreOriginForNotificationPermissionReview(mProfile, origin);
        notifyNotificationPermissionsChanged();
    }

    /** Reverts the action of ignoring the given origin for notification permission review. */
    void undoIgnoreOriginForNotificationPermissionReview(String origin) {
        NotificationPermissionReviewBridgeJni.get()
                .undoIgnoreOriginForNotificationPermissionReview(mProfile, origin);
        notifyNotificationPermissionsChanged();
    }

    /**
     * Resets the notification permission for several origins in bulk and notifies the observers at
     * the end.
     */
    void bulkResetNotificationPermissions() {
        for (NotificationPermissions notificationPermissions : getNotificationPermissions()) {
            NotificationPermissionReviewBridgeJni.get()
                    .resetNotificationPermissionForOrigin(
                            mProfile, notificationPermissions.getPrimaryPattern());
        }
        notifyNotificationPermissionsChanged();
    }

    /** Allows the notification permission for the given origin. */
    void allowNotificationPermissionForOrigin(String origin) {
        NotificationPermissionReviewBridgeJni.get()
                .allowNotificationPermissionForOrigin(mProfile, origin);
        notifyNotificationPermissionsChanged();
    }

    /**
     * Allow the notification permission for several origins in bulk and notifies the observers at
     * the end.
     */
    void bulkAllowNotificationPermissions(
            List<NotificationPermissions> notificationPermissionsList) {
        for (NotificationPermissions notificationPermissions : notificationPermissionsList) {
            NotificationPermissionReviewBridgeJni.get()
                    .allowNotificationPermissionForOrigin(
                            mProfile, notificationPermissions.getPrimaryPattern());
        }
        notifyNotificationPermissionsChanged();
    }

    /** Resets the notification permission for the given origin. */
    void resetNotificationPermissionForOrigin(String origin) {
        NotificationPermissionReviewBridgeJni.get()
                .resetNotificationPermissionForOrigin(mProfile, origin);
        notifyNotificationPermissionsChanged();
    }

    private void notifyNotificationPermissionsChanged() {
        for (Observer observer : mObservers) {
            observer.notificationPermissionsChanged();
        }
    }

    @NativeMethods
    interface Natives {
        @JniType("std::vector<NotificationPermissions>")
        NotificationPermissions[] getNotificationPermissions(@JniType("Profile*") Profile profile);

        void ignoreOriginForNotificationPermissionReview(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void undoIgnoreOriginForNotificationPermissionReview(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void allowNotificationPermissionForOrigin(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void resetNotificationPermissionForOrigin(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);
    }
}
