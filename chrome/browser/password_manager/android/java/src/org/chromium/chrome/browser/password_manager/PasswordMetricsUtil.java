// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.build.annotations.NullMarked;

/** This class should contain helpers for recording Password Manager metrics. */
@NullMarked
public class PasswordMetricsUtil {
    public static final String ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Latency";
    public static final String ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Success";
    public static final String ACCOUNT_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Error";
    public static final String ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.APIError";
    public static final String ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.Launch.Success";

    public static final String LOCAL_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Latency";
    public static final String LOCAL_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Success";
    public static final String LOCAL_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Error";
    public static final String LOCAL_GET_INTENT_API_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.APIError";
    public static final String LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.Launch.Success";

    public static final String PASSWORD_CHECKUP_HISTOGRAM_BASE = "PasswordManager.PasswordCheckup";

    public static final String PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.Launch.Success2";
}
