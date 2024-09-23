// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.components.content_settings.ContentSettingsType;

/** Container class needed to pass information from Native to Java and vice versa. */
public class PermissionsData {
    private final String mOrigin;
    private final @ContentSettingsType.EnumType int[] mPermissions;
    // Microseconds since windows epoch.
    private final long mExpiration;
    // TimeDelta in microseconds.
    private final long mLifetime;

    private PermissionsData(
            String origin,
            @ContentSettingsType.EnumType int[] permissions,
            long expiration,
            long lifetime) {
        mOrigin = origin;
        mPermissions = permissions;
        mExpiration = expiration;
        mLifetime = lifetime;
    }

    @VisibleForTesting
    @CalledByNative
    static PermissionsData create(
            @JniType("std::string") String origin,
            @JniType("std::vector<int32_t>") int[] permissions,
            @JniType("std::int64_t") long expiration,
            @JniType("std::int64_t") long lifetime) {
        return new PermissionsData(origin, permissions, expiration, lifetime);
    }

    @CalledByNative
    public @JniType("std::string") String getOrigin() {
        return mOrigin;
    }

    @CalledByNative
    public @JniType("std::vector<int32_t>") int[] getPermissions() {
        return mPermissions;
    }

    @CalledByNative
    public @JniType("std::int64_t") long getExpiration() {
        return mExpiration;
    }

    @CalledByNative
    public @JniType("std::int64_t") long getLifetime() {
        return mLifetime;
    }
}
