// Copyright 2015 The Chromium Authors
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
    private final long mLastUpdate;
    private final boolean mSecure;
    private final boolean mHttpOnly;
    private final int mSameSite;
    private final int mPriority;
    private final String mPartitionKey;
    private final int mSourceScheme;
    private final int mSourcePort;
    private final int mSourceType;

    /** Constructs a CanonicalCookie */
    CanonicalCookie(
            String name,
            String value,
            String domain,
            String path,
            long creation,
            long expiration,
            long lastAccess,
            long lastUpdate,
            boolean secure,
            boolean httpOnly,
            int sameSite,
            int priority,
            String partitionKey,
            int sourceScheme,
            int sourcePort,
            int sourceType) {
        mName = name;
        mValue = value;
        mDomain = domain;
        mPath = path;
        mCreation = creation;
        mExpiration = expiration;
        mLastAccess = lastAccess;
        mLastUpdate = lastUpdate;
        mSecure = secure;
        mHttpOnly = httpOnly;
        mSameSite = sameSite;
        mPriority = priority;
        mPartitionKey = partitionKey;
        mSourceScheme = sourceScheme;
        mSourcePort = sourcePort;
        mSourceType = sourceType;
    }

    /**
     * @return Priority of the cookie.
     */
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

    /** @return Last updated time. */
    long getLastUpdateDate() {
        return mLastUpdate;
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

    /** @return Cookie partition key. */
    String getPartitionKey() {
        return mPartitionKey;
    }

    /**
     * @return Source scheme of the cookie.
     */
    int sourceScheme() {
        return mSourceScheme;
    }

    /** @return Source port of the cookie. */
    int sourcePort() {
        return mSourcePort;
    }

    /**
     * @return Source of the cookie (http, script, etc.).
     */
    int sourceType() {
        return mSourceType;
    }

    // Note incognito state cannot persist across app installs since the encryption key is stored
    // in the activity state bundle. So the version here is more of a guard than a real version
    // used for format migrations.
    private static final int SERIALIZATION_VERSION = 20210712;

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
        out.writeUTF(mName);
        out.writeUTF(mValue);
        out.writeUTF(mDomain);
        out.writeUTF(mPath);
        out.writeLong(mCreation);
        out.writeLong(mExpiration);
        out.writeLong(mLastAccess);
        out.writeLong(mLastUpdate);
        out.writeBoolean(mSecure);
        out.writeBoolean(mHttpOnly);
        out.writeInt(mSameSite);
        out.writeInt(mPriority);
        out.writeUTF(mPartitionKey);
        out.writeInt(mSourceScheme);
        out.writeInt(mSourcePort);
        out.writeInt(mSourceType);
    }

    private static CanonicalCookie createFromStream(DataInputStream in) throws IOException {
        return new CanonicalCookie(
                in.readUTF(), // name
                in.readUTF(), // value
                in.readUTF(), // domain
                in.readUTF(), // path
                in.readLong(), // creation
                in.readLong(), // expiration
                in.readLong(), // last access
                in.readLong(), // last update
                in.readBoolean(), // secure
                in.readBoolean(), // httponly
                in.readInt(), // samesite
                in.readInt(), // priority
                in.readUTF(), // partition key
                in.readInt(), // source scheme
                in.readInt(), // source port
                in.readInt()); // source type
    }
}
