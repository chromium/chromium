// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.network;

/** Representation of an HTTP response. */
public final class HttpResponse {
    private final int mResponseCode;
    private final byte[] mResponseBody;
    private final boolean mIsSignedIn;

    public HttpResponse(int responseCode, byte[] responseBody, boolean isSignedIn) {
        this.mResponseCode = responseCode;
        this.mResponseBody = responseBody;
        this.mIsSignedIn = isSignedIn;
    }

    /**
     * Gets the response code for the response.
     *
     * <p>Note: this does not have to correspond to an HTTP response code, e.g. if there is a
     * network issue and no request was able to be sent.
     */
    public int getResponseCode() {
        return mResponseCode;
    }

    /** Gets the body for the response. */
    public byte[] getResponseBody() {
        return mResponseBody;
    }

    public boolean isSignedIn() {
        return mIsSignedIn;
    }
}
