// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager.one_time_passwords;

import com.google.android.gms.common.api.ApiException;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

/** Interface for sending one time passwords requests to a downstream implementation. */
@NullMarked
public interface AndroidSmsOtpFetcher {
    /**
     * Triggers a SMS code retrieval request.
     *
     * @param otpValueCallback Callback that is called on success with the fetched OTP value string.
     * @param failureCallback A callback that is called on failure for any reason.
     */
    void retrieveSmsOtp(Callback<String> otpValueCallback, Callback<ApiException> failureCallback);
}
