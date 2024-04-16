// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Base class for Token and UnguessableToken. */
@JNINamespace("base::android")
public abstract class TokenBase {
    protected final long mHigh;
    protected final long mLow;

    protected TokenBase(long high, long low) {
        mHigh = high;
        mLow = low;
    }

    @CalledByNative
    private long getHighForSerialization() {
        return mHigh;
    }

    @CalledByNative
    private long getLowForSerialization() {
        return mLow;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj == null || obj.getClass() != getClass()) {
            return false;
        }
        TokenBase other = (TokenBase) obj;
        return other.mHigh == mHigh && other.mLow == mLow;
    }

    @Override
    public int hashCode() {
        int mLowHash = (int) (mLow ^ (mLow >>> 32));
        int mHighHash = (int) (mHigh ^ (mHigh >>> 32));
        return 31 * mLowHash + mHighHash;
    }
}
