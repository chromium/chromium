// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.support.annotation.IntDef;

import org.chromium.chrome.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class mirrors the native GoogleServiceAuthError class State enum from:
 * google_apis/gaia/google_service_auth_error.h.
 */
public class GoogleServiceAuthError {
    @IntDef({State.NONE, State.INVALID_GAIA_CREDENTIALS, State.USER_NOT_SIGNED_UP,
            State.CONNECTION_FAILED, State.CAPTCHA_REQUIRED, State.ACCOUNT_DELETED,
            State.ACCOUNT_DISABLED, State.SERVICE_UNAVAILABLE, State.TWO_FACTOR,
            State.REQUEST_CANCELED, State.UNEXPECTED_SERVICE_RESPONSE, State.SERVICE_ERROR,
            State.WEB_LOGIN_REQUIRED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        // The user is authenticated.
        int NONE = 0;

        // The credentials supplied to GAIA were either invalid, or the locally
        // cached credentials have expired.
        int INVALID_GAIA_CREDENTIALS = 1;

        // The GAIA user is not authorized to use the service.
        int USER_NOT_SIGNED_UP = 2;

        // Could not connect to server to verify credentials. This could be in
        // response to either failure to connect to GAIA or failure to connect to
        // the service needing GAIA tokens during authentication.
        int CONNECTION_FAILED = 3;

        // The user needs to satisfy a CAPTCHA challenge to unlock their account.
        // If no other information is available, this can be resolved by visiting
        // https://www.google.com/accounts/DisplayUnlockCaptcha. Otherwise,
        // captcha() will provide details about the associated challenge.
        int CAPTCHA_REQUIRED = 4;

        // The user account has been deleted.
        int ACCOUNT_DELETED = 5;

        // The user account has been disabled.
        int ACCOUNT_DISABLED = 6;

        // The service is not available; try again later.
        int SERVICE_UNAVAILABLE = 7;

        // The password is valid but we need two factor to get a token.
        int TWO_FACTOR = 8;

        // The requestor of the authentication step cancelled the request
        // prior to completion.
        int REQUEST_CANCELED = 9;

        // HOSTED accounts are deprecated; left in enumeration to match
        // GoogleServiceAuthError enum in histograms.xml.
        // int HOSTED_NOT_ALLOWED_DEPRECATED = 10;

        // Indicates the service responded to a request, but we cannot
        // interpret the response.
        int UNEXPECTED_SERVICE_RESPONSE = 11;

        // Indicates the service responded and response carried details of the
        // application error.
        int SERVICE_ERROR = 12;

        // The password is valid but web login is required to get a token.
        int WEB_LOGIN_REQUIRED = 13;

        int NUM_ENTRIES = 14;
    }

    public static int getMessageID(@State int state) {
        switch (state) {
            case State.INVALID_GAIA_CREDENTIALS:
                return R.string.sync_error_ga;
            case State.CONNECTION_FAILED:
                return R.string.sync_error_connection;
            case State.SERVICE_UNAVAILABLE:
                return R.string.sync_error_service_unavailable;
            // case State.NONE:
            // case State.USER_NOT_SIGNED_UP:
            // case State.CAPTCHA_REQUIRED:
            // case State.ACCOUNT_DELETED:
            // case State.ACCOUNT_DISABLED:
            // case State.TWO_FACTOR:
            // case State.REQUEST_CANCELED:
            // case State.UNEXPECTED_SERVICE_RESPONSE:
            // case State.SERVICE_ERROR:
            // case State.WEB_LOGIN_REQUIRED:
            default:
                return R.string.sync_error_generic;
        }
    }
}
