// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.exceptions;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

/** Base class for ReadAloud exceptions reported from the service. */
public class ReadAloudException extends Exception {

    private final @ReadAloudErrorCode int mStatusCode;

    public ReadAloudException(String message, @Nullable Throwable cause, int statusCode) {
        super(message, cause);
        mStatusCode = statusCode;
    }

    /** Returns the status code of the error. */
    public @ReadAloudErrorCode int getStatusCode() {
        return mStatusCode;
    }

    @IntDef({
        ReadAloudErrorCode.OK,
        ReadAloudErrorCode.CANCELLED,
        ReadAloudErrorCode.UNKNOWN,
        ReadAloudErrorCode.INVALID_ARGUMENT,
        ReadAloudErrorCode.DEADLINE_EXCEEDED,
        ReadAloudErrorCode.NOT_FOUND,
        ReadAloudErrorCode.ALREADY_EXISTS,
        ReadAloudErrorCode.PERMISSION_DENIED,
        ReadAloudErrorCode.UNAUTHENTICATED,
        ReadAloudErrorCode.RESOURCE_EXHAUSTED,
        ReadAloudErrorCode.FAILED_PRECONDITION,
        ReadAloudErrorCode.ABORTED,
        ReadAloudErrorCode.OUT_OF_RANGE,
        ReadAloudErrorCode.UNIMPLEMENTED,
        ReadAloudErrorCode.INTERNAL,
        ReadAloudErrorCode.UNAVAILABLE,
        ReadAloudErrorCode.DATA_LOSS,
        ReadAloudErrorCode.COUNT
    })
    // This must be kept in sync with readaloud/enums.xml values
    public @interface ReadAloudErrorCode {
        int OK = 0;
        int CANCELLED = 1;
        int UNKNOWN = 2;
        int INVALID_ARGUMENT = 3;
        int DEADLINE_EXCEEDED = 4;
        int NOT_FOUND = 5;
        int ALREADY_EXISTS = 6;
        int PERMISSION_DENIED = 7;
        int UNAUTHENTICATED = 16;
        int RESOURCE_EXHAUSTED = 8;
        int FAILED_PRECONDITION = 9;
        int ABORTED = 10;
        int OUT_OF_RANGE = 11;
        int UNIMPLEMENTED = 12;
        int INTERNAL = 13;
        int UNAVAILABLE = 14;
        int DATA_LOSS = 15;
        int COUNT = 17;
    }
}
