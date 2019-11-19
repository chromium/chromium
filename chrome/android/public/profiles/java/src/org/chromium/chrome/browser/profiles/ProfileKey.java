// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Wrapper that allows passing a ProfileKey reference around in the Java layer.
 */
public class ProfileKey {
    /** Whether this wrapper corresponds to an off the record ProfileKey. */
    private final boolean mIsOffTheRecord;

    /** Pointer to the Native-side ProfileKey. */
    private long mNativeProfileKeyAndroid;

    private ProfileKey(long nativeProfileKeyAndroid) {
        mNativeProfileKeyAndroid = nativeProfileKeyAndroid;
        mIsOffTheRecord =
                ProfileKeyJni.get().isOffTheRecord(mNativeProfileKeyAndroid, ProfileKey.this);
    }

    public static ProfileKey getLastUsedProfileKey() {
        // TODO(mheikal): Assert at least reduced mode is started when https://crbug.com/973241 is
        // fixed.
        return (ProfileKey) ProfileKeyJni.get().getLastUsedProfileKey();
    }

    public ProfileKey getOriginalKey() {
        return (ProfileKey) ProfileKeyJni.get().getOriginalKey(
                mNativeProfileKeyAndroid, ProfileKey.this);
    }

    public boolean isOffTheRecord() {
        return mIsOffTheRecord;
    }

    @CalledByNative
    private static ProfileKey create(long nativeProfileKeyAndroid) {
        return new ProfileKey(nativeProfileKeyAndroid);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeProfileKeyAndroid = 0;
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeProfileKeyAndroid;
    }

    @NativeMethods
    interface Natives {
        Object getLastUsedProfileKey();
        Object getOriginalKey(long nativeProfileKeyAndroid, ProfileKey caller);
        boolean isOffTheRecord(long nativeProfileKeyAndroid, ProfileKey caller);
    }
}
