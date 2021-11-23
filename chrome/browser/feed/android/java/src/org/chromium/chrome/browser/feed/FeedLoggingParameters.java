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
    private final String mAccountName;
    private final boolean mLoggingEnabled;
    private final boolean mViewActionsEnabled;

    /**
     * Creates logging parameters. Creation of this implies that logging is enabled.
     */
    public FeedLoggingParameters(String clientInstanceId, String accountName,
            boolean loggingEnabled, boolean viewActionsEnabled) {
        mClientInstanceId = clientInstanceId;
        mAccountName = accountName;
        mLoggingEnabled = loggingEnabled;
        mViewActionsEnabled = viewActionsEnabled;
    }

    public FeedLoggingParameters(FeedUiProto.LoggingParameters proto) {
        this(proto.getClientInstanceId(), proto.getEmail(), proto.getLoggingEnabled(),
                proto.getViewActionsEnabled());
    }

    public static FeedUiProto.LoggingParameters convertToProto(
            LoggingParameters loggingParameters) {
        return FeedUiProto.LoggingParameters.newBuilder()
                .setEmail(loggingParameters.accountName())
                .setClientInstanceId(loggingParameters.clientInstanceId())
                .setLoggingEnabled(loggingParameters.loggingEnabled())
                .setViewActionsEnabled(loggingParameters.viewActionsEnabled())
                .build();
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
    public boolean loggingParametersEquals(LoggingParameters otherObject) {
        if (otherObject == null) {
            return false;
        }
        FeedLoggingParameters rhs = (FeedLoggingParameters) otherObject;
        return mLoggingEnabled == rhs.mLoggingEnabled
                && mViewActionsEnabled == rhs.mViewActionsEnabled
                && nullableStringEqual(mAccountName, rhs.mAccountName)
                && nullableStringEqual(mClientInstanceId, rhs.mClientInstanceId);
    }
    @Override
    public boolean loggingEnabled() {
        return mLoggingEnabled;
    }
    @Override
    public boolean viewActionsEnabled() {
        return mViewActionsEnabled;
    }
    static boolean nullableStringEqual(@Nullable String a, @Nullable String b) {
        return (a == null ? "" : a).equals(b == null ? "" : b);
    }
}
