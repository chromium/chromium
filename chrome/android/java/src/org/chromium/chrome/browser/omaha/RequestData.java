// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Represents parameters for a single XML request to send to the server. Persisted requests (those
 * that must be resent in case of failure) should use the same ID from the first failed attempt.
 */
@NullMarked
public class RequestData {
    private final long mCreationTimestamp;
    private final boolean mSendInstallEvent;
    private final String mRequestID;
    private final @Nullable String mInstallSource;

    public RequestData(
            boolean sendInstallEvent,
            long timeStamp,
            String requestID,
            @Nullable String installSource) {
        assert requestID != null;
        mSendInstallEvent = sendInstallEvent;
        mCreationTimestamp = timeStamp;
        mRequestID = requestID;
        mInstallSource = installSource;
    }

    /**
     * Whether or not we are telling the server about a new install.
     * False indicates a ping/updatecheck.
     */
    public boolean isSendInstallEvent() {
        return mSendInstallEvent;
    }

    /** ID of the request we're sending to the server. */
    public String getRequestID() {
        return mRequestID;
    }

    /** Get the age in milliseconds. */
    public long getAgeInMilliseconds(long currentTimestamp) {
        return currentTimestamp - mCreationTimestamp;
    }

    /** Get the age in seconds. */
    public long getAgeInSeconds(long currentTimestamp) {
        return getAgeInMilliseconds(currentTimestamp) / 1000;
    }

    /** Get the exact timestamp when this was created. */
    public long getCreationTimestamp() {
        return mCreationTimestamp;
    }

    /**
     * Get the install source for the APK. Values can include {@link
     * OmahaClient#INSTALL_SOURCE_SYSTEM} or {@link OmahaClient#INSTALL_SOURCE_ORGANIC}.
     */
    public @Nullable String getInstallSource() {
        return mInstallSource;
    }
}
