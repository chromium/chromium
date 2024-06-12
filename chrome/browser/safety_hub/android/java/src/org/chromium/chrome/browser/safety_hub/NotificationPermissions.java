// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

/** Container class needed to pass information from Native to Java and vice versa. */
public class NotificationPermissions {
    private final String mPrimaryPattern;
    private final String mSecondaryPattern;
    private final int mNotificationCount;

    private NotificationPermissions(
            String primaryPattern, String secondaryPattern, int notificationCount) {
        mPrimaryPattern = primaryPattern;
        mSecondaryPattern = secondaryPattern;
        mNotificationCount = notificationCount;
    }

    @VisibleForTesting
    @CalledByNative
    static NotificationPermissions create(
            @JniType("std::string") String primaryPattern,
            @JniType("std::string") String secondaryPattern,
            @JniType("int32_t") int notificationCount) {
        return new NotificationPermissions(primaryPattern, secondaryPattern, notificationCount);
    }

    @CalledByNative
    public @JniType("std::string") String getPrimaryPattern() {
        return mPrimaryPattern;
    }

    @CalledByNative
    public @JniType("std::string") String getSecondaryPattern() {
        return mSecondaryPattern;
    }

    @CalledByNative
    public @JniType("int32_t") int getNotificationCount() {
        return mNotificationCount;
    }
}
