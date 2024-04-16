// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.util.Objects;

/** Java counterpart to the native base::Token. A {@link Token} is a random 128-bit integer. */
@JNINamespace("base::android")
@DoNotMock("Create a real instance instead.")
public final class Token {
    private final long mHigh;
    private final long mLow;

    /** Returns a new random token using the native implementation. */
    public static Token createRandom() {
        return TokenJni.get().createRandom();
    }

    /**
     * Create a token from a high and low values. This is intended to be used when passing a token
     * up from native or when creating a token from a serialized pair of longs not for new tokens.
     *
     * @param high The high portion of the token.
     * @param low The low portion of the token.
     */
    @CalledByNative
    public Token(long high, long low) {
        mHigh = high;
        mLow = low;
    }

    /** Returns whether the Token is 0. */
    public boolean isZero() {
        return mHigh == 0 && mLow == 0;
    }

    /** Returns the high portion of the token. */
    @CalledByNative
    public long getHigh() {
        return mHigh;
    }

    /** Returns the low portion of the token. */
    @CalledByNative
    public long getLow() {
        return mLow;
    }

    @Override
    public String toString() {
        return String.format("%016X%016X", mHigh, mLow);
    }

    @Override
    public final int hashCode() {
        return Objects.hash(mHigh, mLow);
    }

    @Override
    public final boolean equals(@Nullable Object obj) {
        if (obj == null || getClass() != obj.getClass()) return false;

        Token other = (Token) obj;
        return other.mHigh == mHigh && other.mLow == mLow;
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        Token createRandom();
    }
}
