// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed;

import androidx.annotation.Nullable;

import com.google.protobuf.ByteString;

import org.chromium.chrome.browser.xsurface.LoggingParameters;
import org.chromium.components.feed.proto.FeedUiProto;

/** Implements LoggingParameters. Contains parameters needed for logging. */
class FeedLoggingParameters implements LoggingParameters {
    private final String mClientInstanceId;
    private final String mAccountName;
    private final boolean mLoggingEnabled;
    private final boolean mViewActionsEnabled;
    private final byte[] mRootEventId;

    /** Creates logging parameters. Creation of this implies that logging is enabled. */
    public FeedLoggingParameters(
            String clientInstanceId,
            String accountName,
            boolean loggingEnabled,
            boolean viewActionsEnabled,
            byte[] rootEventId) {
        mClientInstanceId = clientInstanceId;
        mAccountName = accountName;
        mLoggingEnabled = loggingEnabled;
        mViewActionsEnabled = viewActionsEnabled;
        mRootEventId = rootEventId;
    }

    public FeedLoggingParameters(FeedUiProto.LoggingParameters proto) {
        this(
                proto.getClientInstanceId(),
                proto.getEmail(),
                proto.getLoggingEnabled(),
                proto.getViewActionsEnabled(),
                proto.getRootEventId().toByteArray());
    }

    public static FeedUiProto.LoggingParameters convertToProto(
            LoggingParameters loggingParameters) {
        FeedUiProto.LoggingParameters.Builder builder =
                FeedUiProto.LoggingParameters.newBuilder()
                        .setEmail(loggingParameters.accountName())
                        .setClientInstanceId(loggingParameters.clientInstanceId())
                        .setLoggingEnabled(loggingParameters.loggingEnabled())
                        .setViewActionsEnabled(loggingParameters.viewActionsEnabled());
        byte[] rootEventId = loggingParameters.rootEventId();
        if (rootEventId != null) {
            builder.setRootEventId(ByteString.copyFrom(rootEventId));
        }
        return builder.build();
    }

    @Override
    public String accountName() {
        return mAccountName;
    }

    @Override
    public String clientInstanceId() {
        return mClientInstanceId;
    }

    @Deprecated
    @Override
    public boolean loggingParametersEquals(LoggingParameters otherObject) {
        return false;
    }

    @Override
    public boolean loggingEnabled() {
        return mLoggingEnabled;
    }

    @Override
    public boolean viewActionsEnabled() {
        return mViewActionsEnabled;
    }

    @Override
    @Nullable
    public byte[] rootEventId() {
        return mRootEventId;
    }
}
