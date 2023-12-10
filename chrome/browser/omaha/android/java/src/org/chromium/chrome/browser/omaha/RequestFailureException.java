// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

/** Serves as a general exception for failed POST requests to the Omaha Update Server. */
public class RequestFailureException extends Exception {
    public static final int ERROR_UNDEFINED = 0;
    public static final int ERROR_MALFORMED_XML = 1;
    public static final int ERROR_PARSE_ROOT = 2;
    public static final int ERROR_PARSE_RESPONSE = 3;
    public static final int ERROR_PARSE_DAYSTART = 4;
    public static final int ERROR_PARSE_APP = 5;
    public static final int ERROR_PARSE_PING = 6;
    public static final int ERROR_PARSE_EVENT = 7;
    public static final int ERROR_PARSE_URLS = 8;
    public static final int ERROR_PARSE_UPDATECHECK = 9;
    public static final int ERROR_PARSE_MANIFEST = 10;
    public static final int ERROR_CONNECTIVITY = 11;

    public int errorCode = ERROR_UNDEFINED;

    public RequestFailureException(String message) {
        this(message, ERROR_UNDEFINED);
    }

    public RequestFailureException(String message, int error) {
        super(message);
        errorCode = error;
    }

    public RequestFailureException(String message, Throwable cause) {
        this(message, cause, ERROR_UNDEFINED);
    }

    public RequestFailureException(String message, Throwable cause, int error) {
        super(message, cause);
        errorCode = error;
    }
}
