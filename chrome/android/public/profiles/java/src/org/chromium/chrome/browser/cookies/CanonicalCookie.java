// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cookies;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Java representation of net/cookies/canonical_cookie.h.
 *
 * Also has static methods serialize Cookies.
 */
class CanonicalCookie {
    private final String mName;
    private final String mValue;
    private final String mDomain;
    private final String mPath;
    private final long mCreation;
    private final long mExpiration;
    private final long mLastAccess;
    private final boolean mSecure;
    private final boolean mHttpOnly;
    private final int mSameSite;
    private final int mPriority;

    /** Constructs a CanonicalCookie */
    CanonicalCookie(String name, String value, String domain, String path, long creation,
            long expiration, long lastAccess, boolean secure, boolean httpOnly, int sameSite,
            int priority) {
        mName = name;
        mValue = value;
        mDomain = domain;
        mPath = path;
        mCreation = creation;
        mExpiration = expiration;
        mLastAccess = lastAccess;
        mSecure = secure;
        mHttpOnly = httpOnly;
        mSameSite = sameSite;
        mPriority = priority;
    }

    /** @return Priority of the cookie. */
    int getPriority() {
        return mPriority;
    }

    /** @return True if the cookie is HTTP only. */
    boolean isHttpOnly() {
        return mHttpOnly;
    }

    /** @return SameSite enum */
    int getSameSite() {
        return mSameSite;
    }

    /** @return True if the cookie is secure. */
    boolean isSecure() {
        return mSecure;
    }

    /** @return Last accessed time. */
    long getLastAccessDate() {
        return mLastAccess;
    }

    /** @return Expiration time. */
    long getExpirationDate() {
        return mExpiration;
    }

    /** @return Creation time. */
    long getCreationDate() {
        return mCreation;
    }

    /** @return Cookie name. */
    String getName() {
        return mName;
    }

    /** @return Cookie path. */
    String getPath() {
        return mPath;
    }

    /** @return Cookie domain. */
    String getDomain() {
        return mDomain;
    }

    /** @return Cookie value. */
    String getValue() {
        return mValue;
    }

    // Note incognito state cannot persist across app installs since the encryption key is stored
    // in the activity state bundle. So the version here is more of a guard than a real version
    // used for format migrations.
    private static final int SERIALIZATION_VERSION = 20160426;

    static void saveListToStream(DataOutputStream out, CanonicalCookie[] cookies)
            throws IOException {
        if (out == null) {
            throw new IllegalArgumentException("out arg is null");
        }
        if (cookies == null) {
            throw new IllegalArgumentException("cookies arg is null");
        }
        for (CanonicalCookie cookie : cookies) {
            if (cookie == null) {
                throw new IllegalArgumentException("cookies arg contains null value");
            }
        }

        int length = cookies.length;
        out.writeInt(SERIALIZATION_VERSION);
        out.writeInt(length);
        for (int i = 0; i < length; ++i) {
            cookies[i].saveToStream(out);
        }
    }

    // Not private for tests.
    static class UnexpectedFormatException extends Exception {
        public UnexpectedFormatException(String message) {
            super(message);
        }
    }

    static List<CanonicalCookie> readListFromStream(DataInputStream in)
            throws IOException, UnexpectedFormatException {
        if (in == null) {
            throw new IllegalArgumentException("in arg is null");
        }

        final int version = in.readInt();
        if (version != SERIALIZATION_VERSION) {
            throw new UnexpectedFormatException("Unexpected version");
        }
        final int length = in.readInt();
        if (length < 0) {
            throw new UnexpectedFormatException("Negative length: " + length);
        }

        ArrayList<CanonicalCookie> cookies = new ArrayList<>(length);
        for (int i = 0; i < length; ++i) {
            cookies.add(createFromStream(in));
        }
        return cookies;
    }

    private void saveToStream(DataOutputStream out) throws IOException {
        // URL is no longer included. Keep for backward compatability.
        out.writeUTF("");
        out.writeUTF(mName);
        out.writeUTF(mValue);
        out.writeUTF(mDomain);
        out.writeUTF(mPath);
        out.writeLong(mCreation);
        out.writeLong(mExpiration);
        out.writeLong(mLastAccess);
        out.writeBoolean(mSecure);
        out.writeBoolean(mHttpOnly);
        out.writeInt(mSameSite);
        out.writeInt(mPriority);
    }

    private static CanonicalCookie createFromStream(DataInputStream in) throws IOException {
        // URL is no longer included. Keep for backward compatability.
        in.readUTF();
        return new CanonicalCookie(in.readUTF(), in.readUTF(), in.readUTF(), in.readUTF(),
                in.readLong(), in.readLong(), in.readLong(), in.readBoolean(), in.readBoolean(),
                in.readInt(), in.readInt());
    }
}
