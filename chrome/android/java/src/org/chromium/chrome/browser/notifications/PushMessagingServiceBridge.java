// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Provides the ability for the PushMessagingServiceImpl to revoke Notifications permission.
 *
 * <p>This class should only be used on the UI thread.
 */
@NullMarked
public class PushMessagingServiceBridge {
    private static @Nullable PushMessagingServiceBridge sInstance;

    /**
     * Returns the current instance of the PushMessagingServiceBridge.
     *
     * @return The instance of the PushMessagingServiceBridge.
     */
    static PushMessagingServiceBridge getInstance() {
        if (sInstance == null) {
            sInstance = new PushMessagingServiceBridge();
        }

        return sInstance;
    }

    /**
     * Verifies if Notifications permission should be revoked for an origin.
     *
     * @param origin Full text of the origin, including the protocol, owning this notification.
     * @param profileId Id of the profile that showed the notification.
     * @param appLevelNotificationsEnabled Whether Chrome has app-level Notifications permission.
     */
    public void verify(
            String origin, @Nullable String profileId, boolean appLevelNotificationsEnabled) {
        PushMessagingServiceBridgeJni.get()
                .verifyAndRevokeNotificationsPermission(
                        origin, profileId, appLevelNotificationsEnabled);
    }

    @NativeMethods
    interface Natives {
        void verifyAndRevokeNotificationsPermission(
                @JniType("std::string") String origin,
                @JniType("std::string") @Nullable String profileId,
                boolean appLevelNotificationsEnabled);
    }
}
