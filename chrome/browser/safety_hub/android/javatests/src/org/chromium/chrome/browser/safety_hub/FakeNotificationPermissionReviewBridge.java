// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;

/** Java implementation of NotificationPermissionReviewBridge for testing. */
class FakeNotificationPermissionReviewBridge implements NotificationPermissionReviewBridge.Natives {
    private HashSet<String> mActiveOriginSet = new HashSet<>();
    private HashMap<String, NotificationPermissions> mNotificationPermissionMap = new HashMap<>();

    public void setNotificationPermissionsForReview(
            NotificationPermissions[] notificationPermissionsList) {
        for (NotificationPermissions notificationPermissions : notificationPermissionsList) {
            String origin = notificationPermissions.getPrimaryPattern();
            mNotificationPermissionMap.put(origin, notificationPermissions);
            mActiveOriginSet.add(origin);
        }
    }

    @Override
    public NotificationPermissions[] getNotificationPermissions(Profile profile) {
        List<NotificationPermissions> notificationPermissionsList = new ArrayList<>();
        for (String origin : mActiveOriginSet) {
            notificationPermissionsList.add(mNotificationPermissionMap.get(origin));
        }
        return notificationPermissionsList.toArray(new NotificationPermissions[0]);
    }

    @Override
    public void ignoreOriginForNotificationPermissionReview(Profile profile, String origin) {
        mActiveOriginSet.remove(origin);
    }

    @Override
    public void undoIgnoreOriginForNotificationPermissionReview(Profile profile, String origin) {
        mActiveOriginSet.add(origin);
    }

    @Override
    public void allowNotificationPermissionForOrigin(Profile profile, String origin) {
        mActiveOriginSet.add(origin);
    }

    @Override
    public void resetNotificationPermissionForOrigin(Profile profile, String origin) {
        mActiveOriginSet.remove(origin);
    }
}
