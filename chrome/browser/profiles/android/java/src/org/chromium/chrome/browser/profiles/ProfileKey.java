// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;

/** Wrapper that allows passing a ProfileKey reference around in the Java layer. */
public class ProfileKey implements SimpleFactoryKeyHandle {
    /** Whether this wrapper corresponds to an off the record ProfileKey. */
    private final boolean mIsOffTheRecord;

    /** Pointer to the Native-side ProfileKey. */
    private long mNativeProfileKeyAndroid;

    private ProfileKey(long nativeProfileKeyAndroid) {
        mNativeProfileKeyAndroid = nativeProfileKeyAndroid;
        mIsOffTheRecord = ProfileKeyJni.get().isOffTheRecord(mNativeProfileKeyAndroid);
    }

    /**
     * Handles type conversion of Java side {@link SimpleFactoryKeyHandle} to {@link ProfileKey}.
     * @param simpleFactoryKeyHandle Java reference to native SimpleFactoryKey.
     * @return A strongly typed reference the {@link ProfileKey}.
     */
    public static ProfileKey fromSimpleFactoryKeyHandle(
            SimpleFactoryKeyHandle simpleFactoryKeyHandle) {
        return (ProfileKey) simpleFactoryKeyHandle;
    }

    /**
     * @return The original (not off the record) profile key.
     */
    public ProfileKey getOriginalKey() {
        return ProfileKeyJni.get().getOriginalKey(mNativeProfileKeyAndroid);
    }

    /**
     * @return Whether this profile is off the record and should avoid writing to durable records.
     */
    public boolean isOffTheRecord() {
        return mIsOffTheRecord;
    }

    @Override
    public long getNativeSimpleFactoryKeyPointer() {
        return ProfileKeyJni.get().getSimpleFactoryKeyPointer(mNativeProfileKeyAndroid);
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
        ProfileKey getOriginalKey(long nativeProfileKeyAndroid);

        boolean isOffTheRecord(long nativeProfileKeyAndroid);

        long getSimpleFactoryKeyPointer(long nativeProfileKeyAndroid);
    }
}
