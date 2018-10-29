// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.support.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;

import java.util.concurrent.atomic.AtomicReferenceArray;

/**
 * This class allows setting crash keys from the Java side. The set of crash keys is defined at
 * build time. To add a new crash key, add a new entry to:
 * <ol>
 *     <li>The CrashKeyIndex enum in {@code crash_keys_android.h}</li>
 *     <li>The CrashKeyString array in {@code crash_keys_android.cc}</li>
 *     <li>The {@link #KEYS} array in this class.</li>
 * </ol>
 */
public class CrashKeys {
    private static final String[] KEYS =
            new String[] {"loaded_dynamic_module", "active_dynamic_module", "application_status"};

    private final AtomicReferenceArray<String> mValues = new AtomicReferenceArray<>(KEYS.length);

    // Outside of assertions only accessed on the UI thread.
    private boolean mFlushed;

    private static class Holder { static final CrashKeys INSTANCE = new CrashKeys(); }

    private CrashKeys() {
        assert CrashKeyIndex.NUM_KEYS == KEYS.length;
    }

    /**
     * @return The shared instance of this class.
     */
    @CalledByNative
    public static CrashKeys getInstance() {
        return Holder.INSTANCE;
    }

    /**
     * @param keyIndex The index of a crash key.
     * @return The key for the given index.
     */
    static String getKey(@CrashKeyIndex int keyIndex) {
        return KEYS[keyIndex];
    }

    /**
     * @return An atomic array of all the crash key values. This method should only be called before
     *         the values have been flushed to the native side.
     * @see #flushToNative
     */
    AtomicReferenceArray<String> getValues() {
        assert !mFlushed;
        return mValues;
    }

    /**
     * Sets a given crash key to the given value, or clears it. The value will either be stored in
     * Java (for use by pure-Java exception reporting), or forwarded to the native CrashKeys.
     * This method should only be called on the UI thread.
     * @param keyIndex The {@link CrashKeyIndex} of a crash key.
     * @param value The value for the given key, or null to clear it.
     */
    @CalledByNative
    public void set(@CrashKeyIndex int keyIndex, @Nullable String value) {
        ThreadUtils.assertOnUiThread();
        if (mFlushed) {
            nativeSet(keyIndex, value);
            return;
        }

        mValues.set(keyIndex, value);
    }

    /**
     * Flushes all crash keys to the native side. This method should be called on the UI thread when
     * pure-Java exception handling is disabled in favor of native crash reporting.
     */
    @CalledByNative
    void flushToNative() {
        ThreadUtils.assertOnUiThread();

        assert !mFlushed;
        for (@CrashKeyIndex int i = 0; i < mValues.length(); i++) {
            nativeSet(i, mValues.getAndSet(i, null));
        }
        mFlushed = true;
    }

    private native void nativeSet(int key, String value);
}
