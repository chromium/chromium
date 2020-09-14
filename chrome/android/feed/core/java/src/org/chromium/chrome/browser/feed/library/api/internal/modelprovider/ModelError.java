// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import com.google.protobuf.ByteString;

/**
 * Class which contains information needed by the {@link
 * org.chromium.chrome.browser.feed.library.api.client.stream.Stream} about errors which may occur
 * in the infrastructure.
 */
public final class ModelError {
    /** Defines errors are exposed through the ModelProvider to the Stream. */
    @IntDef({ErrorType.UNKNOWN, ErrorType.NO_CARDS_ERROR, ErrorType.PAGINATION_ERROR,
            ErrorType.SYNTHETIC_TOKEN_ERROR})
    public @interface ErrorType {
        // An unknown error, this is not expected to ever be used.
        int UNKNOWN = 0;
        // No cards are available due to an error such as, no network available or a request failed,
        // etc.
        int NO_CARDS_ERROR = 1;
        // Pagination failed due to some type of error such as no network available or a request
        // failed, etc.
        int PAGINATION_ERROR = 2;
        // Pagination failed due to a synthetic token error.
        int SYNTHETIC_TOKEN_ERROR = 3;
    }

    private final @ErrorType int mErrorType;
    @Nullable
    private final ByteString mContinuationToken;

    public ModelError(@ErrorType int errorType, @Nullable ByteString continuationToken) {
        this.mErrorType = errorType;
        this.mContinuationToken = continuationToken;
    }

    /** Returns the ErrorType assocated with the error. */
    public @ErrorType int getErrorType() {
        return mErrorType;
    }

    /** This should be non-null if the ErrorType is PAGINATION_ERROR. */
    @Nullable
    public ByteString getContinuationToken() {
        return mContinuationToken;
    }
}
