// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

public class NotificationPermissionReviewBridge {
    @NativeMethods
    interface Natives {
        @JniType("std::vector<NotificationPermissions>")
        NotificationPermissions[] getNotificationPermissions(@JniType("Profile*") Profile profile);

        void ignoreOriginForNotificationPermissionReview(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void undoIgnoreOriginForNotificationPermissionReview(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void blockNotificationPermissionForOrigin(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void allowNotificationPermissionForOrigin(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void resetNotificationPermissionForOrigin(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);
    }
}
