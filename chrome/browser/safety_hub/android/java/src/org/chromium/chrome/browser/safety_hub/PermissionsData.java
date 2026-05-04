// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.content_settings.ContentSettingsType;

/** Container class needed to pass information from Native to Java and vice versa. */
@NullMarked
public class PermissionsData {
    private final String mOrigin;
    private final @ContentSettingsType.EnumType int[] mPermissions;
    // Microseconds since windows epoch.
    private final long mExpiration;
    // TimeDelta in microseconds.
    private final long mLifetime;
    // The reason why permissions were revoked.
    private final @PermissionsRevocationType int mRevocationType;

    // Serialized string encoding revoked permission data needed by the C++ code for regrant/undo
    // operations.
    private final String mSerializedRevokedPermissionsMap;

    private PermissionsData(
            String origin,
            @ContentSettingsType.EnumType int[] permissions,
            long expiration,
            long lifetime,
            @PermissionsRevocationType int revocationType,
            String serializedRevokedPermissionsMap) {
        mOrigin = origin;
        mPermissions = permissions;
        mExpiration = expiration;
        mLifetime = lifetime;
        mRevocationType = revocationType;
        mSerializedRevokedPermissionsMap = serializedRevokedPermissionsMap;
    }

    @CalledByNative
    static PermissionsData create(
            @JniType("std::string") String origin,
            @JniType("std::vector<int32_t>") @ContentSettingsType.EnumType int[] permissions,
            @JniType("std::int64_t") long expiration,
            @JniType("std::int64_t") long lifetime,
            @JniType("std::int32_t") @PermissionsRevocationType int revocationType,
            @JniType("std::string") String serializedRevokedPermissionsMap) {
        return new PermissionsData(
                origin,
                permissions,
                expiration,
                lifetime,
                revocationType,
                serializedRevokedPermissionsMap);
    }

    @VisibleForTesting
    static PermissionsData create(
            String origin,
            @ContentSettingsType.EnumType int[] permissions,
            long expiration,
            long lifetime,
            @PermissionsRevocationType int revocationType) {
        return new PermissionsData(origin, permissions, expiration, lifetime, revocationType, "{}");
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

    @CalledByNative
    public @JniType("std::int32_t") @PermissionsRevocationType int getRevocationType() {
        return mRevocationType;
    }

    @CalledByNative
    public @JniType("std::string") String getSerializedRevokedPermissionsMap() {
        return mSerializedRevokedPermissionsMap;
    }
}
