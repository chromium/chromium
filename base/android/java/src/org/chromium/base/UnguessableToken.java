// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;

/**
 * This class mirrors unguessable_token.h .  Since tokens are passed by value,
 * we don't bother to maintain a native token.  This implements Parcelable so
 * that it may be sent via binder.
 *
 * To get one of these from native, one must start with a
 * base::UnguessableToken, then create a Java object from it.  See
 * jni_unguessable_token.h for information.
 */
public class UnguessableToken implements Parcelable {
    private final long mHigh;
    private final long mLow;

    private UnguessableToken(long high, long low) {
        mHigh = high;
        mLow = low;
    }

    @CalledByNative
    private static UnguessableToken create(long high, long low) {
        return new UnguessableToken(high, low);
    }

    @CalledByNative
    public long getHighForSerialization() {
        return mHigh;
    }

    @CalledByNative
    public long getLowForSerialization() {
        return mLow;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeLong(mHigh);
        dest.writeLong(mLow);
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj == null || getClass() != obj.getClass()) return false;

        return ((UnguessableToken) obj).mHigh == mHigh && ((UnguessableToken) obj).mLow == mLow;
    }

    @Override
    public int hashCode() {
        int mLowHash = (int) (mLow ^ (mLow >>> 32));
        int mHighHash = (int) (mHigh ^ (mHigh >>> 32));
        return 31 * mLowHash + mHighHash;
    }

    public static final Parcelable.Creator<UnguessableToken> CREATOR =
            new Parcelable.Creator<UnguessableToken>() {
                @Override
                public UnguessableToken createFromParcel(Parcel source) {
                    long high = source.readLong();
                    long low = source.readLong();
                    if (high == 0 || low == 0) {
                        // Refuse to create an empty UnguessableToken.
                        return null;
                    }
                    return new UnguessableToken(high, low);
                }

                @Override
                public UnguessableToken[] newArray(int size) {
                    return new UnguessableToken[size];
                }
            };

    // To avoid unwieldy calls in JNI for tests, parcel and unparcel.
    // TODO(liberato): It would be nice if we could include this only with a
    // java driver that's linked only with unit tests, but i don't see a way
    // to do that.
    @CalledByNative
    private UnguessableToken parcelAndUnparcelForTesting() {
        Parcel parcel = Parcel.obtain();
        writeToParcel(parcel, 0);

        // Rewind the parcel and un-parcel.
        parcel.setDataPosition(0);
        UnguessableToken token = CREATOR.createFromParcel(parcel);
        parcel.recycle();

        return token;
    }
};
