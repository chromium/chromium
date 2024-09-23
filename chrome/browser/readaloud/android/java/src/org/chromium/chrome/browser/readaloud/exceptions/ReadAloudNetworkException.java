// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.exceptions;

import org.chromium.chrome.browser.readaloud.exceptions.ReadAloudException.ReadAloudErrorCode;

/** Represents a ReadAloud error caused by network issues */
public class ReadAloudNetworkException extends ReadAloudException {

    private int mChromeErrorCode;

    public ReadAloudNetworkException(int chromeErrorCode, @ReadAloudErrorCode int canonicalCode) {
        super("Chrome network error code: " + chromeErrorCode, null, canonicalCode);
        mChromeErrorCode = chromeErrorCode;
    }

    /**
     * An error code with negative value defined in:
     * https://source.chromium.org/chromium/chromium/src/+/main:net/base/net_error_list.h
     */
    public int getChromeErrorCode() {
        return mChromeErrorCode;
    }
}
