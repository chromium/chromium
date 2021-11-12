// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.xsurface.LoggingParameters;
import org.chromium.components.feed.proto.FeedUiProto;

/**
 * Implements LoggingParameters. Contains parameters needed for logging.
 */
class FeedLoggingParameters implements LoggingParameters {
    private final String mClientInstanceId;
    private final String mSignedOutSessionId;
    private final String mAccountName;

    /**
     * Creates logging parameters. Creation of this implies that logging is enabled.
     */
    public FeedLoggingParameters(
            String clientInstanceId, String accountName, String signedOutSessionId) {
        mClientInstanceId = clientInstanceId;
        mSignedOutSessionId = signedOutSessionId;
        mAccountName = accountName;
    }

    public FeedLoggingParameters(FeedUiProto.LoggingParameters proto) {
        this(proto.getClientInstanceId(), proto.getEmail(), proto.getSessionId());
    }

    @Override
    public String accountName() {
        return mAccountName;
    }
    @Override
    public String clientInstanceId() {
        return mClientInstanceId;
    }
    @Override
    public String signedOutSessionId() {
        return mSignedOutSessionId;
    }
    @Override
    public boolean loggingParametersEquals(LoggingParameters otherObject) {
        if (otherObject == null) {
            return false;
        }
        FeedLoggingParameters rhs = (FeedLoggingParameters) otherObject;
        return nullableStringEqual(mAccountName, rhs.mAccountName)
                && nullableStringEqual(mClientInstanceId, rhs.mClientInstanceId)
                && nullableStringEqual(mSignedOutSessionId, rhs.mSignedOutSessionId);
    }
    static boolean nullableStringEqual(@Nullable String a, @Nullable String b) {
        return (a == null ? "" : a).equals(b == null ? "" : b);
    }
}
