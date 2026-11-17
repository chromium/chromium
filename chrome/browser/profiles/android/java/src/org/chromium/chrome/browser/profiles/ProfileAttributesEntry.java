// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.google_apis.gaia.GaiaId;

/**
 * Java counterpart of a C++ {@code ProfileAttributesEntry}, identified by its profile path. The
 * attributes are read from native on each call.
 */
@NullMarked
class ProfileAttributesEntry {
    private final String mProfilePath;

    @CalledByNative
    ProfileAttributesEntry(@JniType("std::string") String profilePath) {
        mProfilePath = profilePath;
    }

    String getPath() {
        return mProfilePath;
    }

    /** Returns the Gaia ID of the primary account, or null if signed out or removed. */
    @Nullable GaiaId getGaiaId() {
        return ProfileAttributesEntryJni.get().getGaiaId(mProfilePath);
    }

    /** Returns whether the profile is omitted, false if removed. */
    boolean isOmitted() {
        return ProfileAttributesEntryJni.get().isOmitted(mProfilePath);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::optional<GaiaId>")
        @Nullable GaiaId getGaiaId(@JniType("std::string") String profilePath);
        boolean isOmitted(@JniType("std::string") String profilePath);
    }
}
