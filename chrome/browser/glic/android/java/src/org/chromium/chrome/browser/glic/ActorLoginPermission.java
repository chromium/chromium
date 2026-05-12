// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/** Java counterpart to password_manager::ActorLoginPermission. */
@NullMarked
public class ActorLoginPermission {
    private final String mSiteOrAppName;
    private final GURL mUrl;
    private final String mSignonRealm;
    private final String mUsername;
    private final GURL mFaviconUrl;

    @CalledByNative
    @VisibleForTesting
    ActorLoginPermission(
            String siteOrAppName, GURL url, String signonRealm, String username, GURL faviconUrl) {
        mSiteOrAppName = siteOrAppName;
        mUrl = url;
        mSignonRealm = signonRealm;
        mUsername = username;
        mFaviconUrl = faviconUrl;
    }

    /**
     * Returns the readable name of the site or app.
     *
     * @return The site or app name.
     */
    public String getSiteOrAppName() {
        return mSiteOrAppName;
    }

    /**
     * Returns the URL of the credential's origin.
     *
     * @return The origin URL.
     */
    public GURL getUrl() {
        return mUrl;
    }

    /**
     * Returns the signon realm of the credential.
     *
     * @return The signon realm.
     */
    public String getSignonRealm() {
        return mSignonRealm;
    }

    /**
     * Returns the username associated with the permission.
     *
     * @return The username.
     */
    public String getUsername() {
        return mUsername;
    }

    /**
     * Returns the URL to fetch the favicon.
     *
     * @return The favicon URL.
     */
    public GURL getFaviconUrl() {
        return mFaviconUrl;
    }
}
