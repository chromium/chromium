// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.model_execution;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Optional;

/**
 * Represents a result from a model execution. The result may be complete, partial or contain an
 * error code.
 */
public class ExecutionResult {

    /** Possible failure modes of model execution. */
    @IntDef({
        ExecutionError.UNKNOWN,
        ExecutionError.NOT_AVAILABLE,
        ExecutionError.BUSY,
        ExecutionError.FILTERED,
        ExecutionError.INPUT_FILTERED,
        ExecutionError.REQUEST_TOO_LARGE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ExecutionError {
        public static final int UNKNOWN = 0;
        public static final int NOT_AVAILABLE = 1;
        public static final int BUSY = 2;
        public static final int FILTERED = 3;
        public static final int INPUT_FILTERED = 4;
        public static final int REQUEST_TOO_LARGE = 5;
    }

    /**
     * Constructor for results with error codes.
     *
     * @param errorCode A value from {@code ExecutionError}
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public ExecutionResult(@ExecutionError int errorCode) {
        mIsCompleteResult = false;
        mErrorCode = Optional.of(errorCode);
        mResponse = null;
    }

    /**
     * Constructor for successful results.
     *
     * @param response A response string.
     * @param isCompleteResult Whether {@code response} is a complete result or part of a stream.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public ExecutionResult(String response, boolean isCompleteResult) {
        mIsCompleteResult = isCompleteResult;
        mResponse = response;
        mErrorCode = Optional.empty();
    }

    /**
     * Returns the response text related to this execution result. If {@code isCompleteResult()} is
     * true then it'll return the full string returned from the model. Otherwise it'll contain a
     * partial substring meant to be concatenated to other streaming results to form a full
     * response. If the model failed to execute then null is returned.
     *
     * @return A string representing a full or partial execution result.
     */
    public String getResponse() {
        return mResponse;
    }

    /**
     * Returns an error code if the model failed to execute, {@code Optional.empty()} otherwise.
     *
     * @return A value from {@link ExecutionError} if the model failed to execute, empty optional
     *     otherwise.
     */
    public @ExecutionError Optional<Integer> getErrorCode() {
        return mErrorCode;
    }

    /** Returns whether this is a complete response or part of a stream. */
    public boolean isCompleteResult() {
        return mIsCompleteResult;
    }

    private final String mResponse;

    @ExecutionError private final Optional<Integer> mErrorCode;
    private final boolean mIsCompleteResult;
}
