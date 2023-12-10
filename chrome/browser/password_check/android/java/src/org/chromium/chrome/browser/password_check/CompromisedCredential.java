// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.os.Parcel;
import android.os.Parcelable;

import org.jni_zero.CalledByNative;

import org.chromium.url.GURL;

import java.util.Objects;

/**
 * This class holds the data used to represent a compromised credential in the Password Check
 * settings screen.
 */
public class CompromisedCredential implements Parcelable {
    /** This static member is required to automagically deserialize credential parcels . */
    public static final Parcelable.Creator<CompromisedCredential> CREATOR =
            new Parcelable.Creator<CompromisedCredential>() {
                @Override
                public CompromisedCredential createFromParcel(Parcel in) {
                    final String signonRealm = in.readString();
                    final GURL associatedUrl = GURL.deserialize(in.readString());
                    final String username = in.readString();
                    final String displayOrigin = in.readString();
                    final String displayUsername = in.readString();
                    final String password = in.readString();
                    final String passwordChangeUrl = in.readString();
                    final String associatedApp = in.readString();
                    final long creationTime = in.readLong();
                    final long lastUsedTime = in.readLong();
                    boolean[] boolArguments = new boolean[4];
                    in.readBooleanArray(boolArguments);
                    final boolean leaked = boolArguments[0];
                    final boolean phished = boolArguments[1];

                    return new CompromisedCredential(
                            signonRealm,
                            associatedUrl,
                            username,
                            displayOrigin,
                            displayUsername,
                            password,
                            passwordChangeUrl,
                            associatedApp,
                            creationTime,
                            lastUsedTime,
                            leaked,
                            phished);
                }

                @Override
                public CompromisedCredential[] newArray(int size) {
                    return new CompromisedCredential[size];
                }
            };

    private final String mSignonRealm;
    private final GURL mAssociatedUrl;
    private final String mUsername;
    private final String mDisplayOrigin;
    private final String mDisplayUsername;
    private final String mPassword;
    private final String mPasswordChangeUrl;
    private final String mAssociatedApp;
    private final long mCreationTime;
    private final long mLastUsedTime;
    private final boolean mOnlyLeaked;
    private final boolean mOnlyPhished;

    /**
     * @param signonRealm The URL leading to the sign-on page.
     * @param associatedUrl The associated URL to this credential, for android
     *         credentials without affiliation information it will be empty.
     * @param username The name used to identify this credential (may be empty).
     * @param displayOrigin The origin displayed to the user. Not necessarily a valid URL (e.g.
     *         missing scheme).
     * @param displayUsername The username displayed to the user (substituted if empty).
     * @param password The compromised password.
     * @param passwordChangeUrl A URL that links to the password change form of the affected site.
     * @param associatedApp The associated app if the password originates from it.
     * @param creationTime The time at which the compromised credential was created, which is the
     *        time at which the compromised credential was first found to be compromised during
     *        a check.
     * @param lastUsedTime The time when compromised credential was last time used.
     * @param isOnlyLeaked True if the credential was (only) discovered to be in a leak.
     * @param isOnlyPhished True if the credential was (only) entered on an unsafe site.
     */
    public CompromisedCredential(
            String signonRealm,
            GURL associatedUrl,
            String username,
            String displayOrigin,
            String displayUsername,
            String password,
            String passwordChangeUrl,
            String associatedApp,
            long creationTime,
            long lastUsedTime,
            boolean isOnlyLeaked,
            boolean isOnlyPhished) {
        assert associatedUrl != null
                : "Credential associated URL is null! Pass an empty one instead.";
        assert signonRealm != null;
        assert passwordChangeUrl != null : "Change URL may be empty but not null!";
        assert associatedApp != null : "App package name may be empty but not null!";
        assert !passwordChangeUrl.isEmpty() || !associatedApp.isEmpty()
                : "Change URL and app name may not be empty at the same time!";
        mSignonRealm = signonRealm;
        mAssociatedUrl = associatedUrl;
        mUsername = username;
        mDisplayOrigin = displayOrigin;
        mDisplayUsername = displayUsername;
        mPassword = password;
        mPasswordChangeUrl = passwordChangeUrl;
        mAssociatedApp = associatedApp;
        mCreationTime = creationTime;
        mLastUsedTime = lastUsedTime;
        mOnlyLeaked = isOnlyLeaked;
        mOnlyPhished = isOnlyPhished;
    }

    @CalledByNative
    public String getSignonRealm() {
        return mSignonRealm;
    }

    @CalledByNative
    public String getUsername() {
        return mUsername;
    }

    @CalledByNative
    public GURL getAssociatedUrl() {
        return mAssociatedUrl;
    }

    @CalledByNative
    public String getPassword() {
        return mPassword;
    }

    public String getDisplayUsername() {
        return mDisplayUsername;
    }

    public String getDisplayOrigin() {
        return mDisplayOrigin;
    }

    public String getAssociatedApp() {
        return mAssociatedApp;
    }

    public long getCreationTime() {
        return mCreationTime;
    }

    @CalledByNative
    public long getLastUsedTime() {
        return mLastUsedTime;
    }

    public String getPasswordChangeUrl() {
        return mPasswordChangeUrl;
    }

    public boolean isOnlyLeaked() {
        return mOnlyLeaked;
    }

    public boolean isOnlyPhished() {
        return mOnlyPhished;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        CompromisedCredential that = (CompromisedCredential) o;
        return mSignonRealm.equals(that.mSignonRealm)
                && mAssociatedUrl.equals(that.mAssociatedUrl)
                && mUsername.equals(that.mUsername)
                && mDisplayOrigin.equals(that.mDisplayOrigin)
                && mDisplayUsername.equals(that.mDisplayUsername)
                && mPassword.equals(that.mPassword)
                && mPasswordChangeUrl.equals(that.mPasswordChangeUrl)
                && mAssociatedApp.equals(that.mAssociatedApp)
                && mCreationTime == that.mCreationTime
                && mLastUsedTime == that.mLastUsedTime
                && mOnlyLeaked == that.mOnlyLeaked
                && mOnlyPhished == that.mOnlyPhished;
    }

    @Override
    public String toString() {
        return "CompromisedCredential{"
                + "signonRealm='"
                + mSignonRealm
                + ", associatedUrl='"
                + mAssociatedUrl
                + '\''
                + '\''
                + ", username='"
                + mUsername
                + '\''
                + ", displayOrigin='"
                + mDisplayOrigin
                + '\''
                + ", displayUsername='"
                + mDisplayUsername
                + '\''
                + ", password='"
                + mPassword
                + '\''
                + ", passwordChangeUrl='"
                + mPasswordChangeUrl
                + '\''
                + ", associatedApp='"
                + mAssociatedApp
                + '\''
                + ", creationTime="
                + mCreationTime
                + ". lastUsedTime="
                + mLastUsedTime
                + ", leaked="
                + mOnlyLeaked
                + ", phished="
                + mOnlyPhished
                + '}';
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mSignonRealm,
                mAssociatedUrl.getPossiblyInvalidSpec(),
                mUsername,
                mDisplayOrigin,
                mDisplayUsername,
                mPassword,
                mPasswordChangeUrl,
                mAssociatedApp,
                mCreationTime,
                mLastUsedTime,
                mOnlyLeaked,
                mOnlyPhished);
    }

    @Override
    public void writeToParcel(Parcel parcel, int flags) {
        parcel.writeString(mSignonRealm);
        parcel.writeString(mAssociatedUrl.serialize());
        parcel.writeString(mUsername);
        parcel.writeString(mDisplayOrigin);
        parcel.writeString(mDisplayUsername);
        parcel.writeString(mPassword);
        parcel.writeString(mPasswordChangeUrl);
        parcel.writeString(mAssociatedApp);
        parcel.writeLong(mCreationTime);
        parcel.writeLong(mLastUsedTime);
        parcel.writeBooleanArray(new boolean[] {mOnlyLeaked, mOnlyPhished});
    }

    @Override
    public int describeContents() {
        return 0; // No file descriptor necessary.
    }
}
