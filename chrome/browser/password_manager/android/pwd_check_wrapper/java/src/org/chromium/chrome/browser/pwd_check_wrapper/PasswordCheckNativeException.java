// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;

/**
 * The exception returned by {@link ChromeNativePasswordCheckController} notifying there was an
 * error during password check.
 */
public class PasswordCheckNativeException extends Exception {
    public @PasswordCheckUIStatus int errorCode;

    public PasswordCheckNativeException(String message, @PasswordCheckUIStatus int status) {
        super(message);
        errorCode = status;
    }
}
