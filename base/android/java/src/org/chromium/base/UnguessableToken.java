// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Parcel;
import android.os.Parcelable;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * This class mirrors unguessable_token.h. Since tokens are passed by value, we don't bother to
 * maintain a native token. This implements Parcelable so that it may be sent via binder.
 *
 * <p>To get one of these from native, one must start with a base::UnguessableToken, then create a
 * Java object from it. See unguessable_token_android.h for information.
 */
@DoNotMock("This is a simple value object.")
@JNINamespace("base::android")
public final class UnguessableToken extends TokenBase implements Parcelable {
    private static int sCounterForTesting;

    public static UnguessableToken createForTesting() {
        return new UnguessableToken(sCounterForTesting++, sCounterForTesting++);
    }

    @CalledByNative
    private UnguessableToken(long high, long low) {
        super(high, low);
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

    public long getHighForTesting() {
        return mHigh;
    }

    public long getLowForTesting() {
        return mLow;
    }

    // To avoid unwieldy calls in JNI for tests, parcel and unparcel.
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

    // Silences ObjectToString Error Prone warnings when printing instances.
    @Override
    @SuppressWarnings("RedundantOverride")
    public String toString() {
        return super.toString();
    }
}
