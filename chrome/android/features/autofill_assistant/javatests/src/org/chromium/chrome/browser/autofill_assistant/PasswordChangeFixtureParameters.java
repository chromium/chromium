// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.url.GURL;

/**
 * Class that stores parameters for password change integration tests.
 */
public class PasswordChangeFixtureParameters {
    /**
     * Autofill assistant backend URL. Provided via --autofill-assistant-url.
     */
    private String mAutofillAssistantUrl;
    /**
     * Domain URL to launch the test. Provided via --domain-url.
     */
    private GURL mDomainUrl;
    /**
     * Username that specifies what credential to update during script runs. Used in tests
     * that run the script for a single credential.
     *
     * Provided via --run-for-username.
     */
    private String mUsername;
    /**
     * Username list for initial set of credentials. The usernames should be separated by space. The
     * number of usernames should correspond to the number of passwords provided via
     * --seed-passwords. Ex: "username1@example.com username2@example2.com username3@example3.com".
     *
     * Provided via --seed-usernames.
     */
    private String mSeedUsernames;
    /**
     * Password list for initial set of credentials. The passwords should be separated by space. The
     * number of passwords should correspond to the number of usernames provided via
     * --seed-usernames. Ex: "aeGcvLPXQwaT2Rf 5GPUXKBUmMxNZTK 7J4rNxgmj6c6BMA".
     *
     * Provided via --seed-passwords.
     */
    private String mSeedPasswords;
    /**
     * List of initial credentials to seed the password store. The list is created by one username
     * and one password from --seed-usernames and --seed-passwords respectively from left to right.
     */
    private PasswordStoreCredential[] mSeedCredentials;
    /**
     * Script debug bundle id. Provided via --debug-bundle-id.
     */
    private String mDebugBundleId;
    /**
     * Script debug socket id. Provided via --debug-socket-id.
     */
    private String mDebugSocketId;
    /**
     * Number of consecutive script runs. Provided via --num-runs.
     */
    private int mNumRuns;

    /**
     * Loads test parameters from command line.
     */
    public static PasswordChangeFixtureParameters loadFromCommandLine() {
        assert CommandLine.isInitialized() : "CommandLine is expected to be initialized.";

        PasswordChangeFixtureParameters params = new PasswordChangeFixtureParameters();
        params.mAutofillAssistantUrl =
                CommandLine.getInstance().getSwitchValue("autofill-assistant-url");
        params.mDomainUrl = new GURL(CommandLine.getInstance().getSwitchValue("domain-url"));
        params.mUsername = CommandLine.getInstance().getSwitchValue("run-for-username");
        params.mSeedUsernames = CommandLine.getInstance().getSwitchValue("seed-usernames", "");
        params.mSeedPasswords = CommandLine.getInstance().getSwitchValue("seed-passwords", "");
        params.mDebugBundleId = CommandLine.getInstance().getSwitchValue("debug-bundle-id");
        params.mDebugSocketId = CommandLine.getInstance().getSwitchValue("debug-socket-id");
        params.mNumRuns =
                Integer.parseInt(CommandLine.getInstance().getSwitchValue("num-runs", "1"));

        String[] seedUsernames = params.mSeedUsernames.trim().split("\\s+");
        String[] seedPasswords = params.mSeedPasswords.trim().split("\\s+");
        assert seedUsernames.length
                == seedPasswords.length
            : "Number of usernames and passwords provided must be equal.";

        params.mSeedCredentials = new PasswordStoreCredential[seedPasswords.length];
        for (int i = 0; i < seedUsernames.length; i++) {
            params.mSeedCredentials[i] = new PasswordStoreCredential(
                    params.mDomainUrl, seedUsernames[i], seedPasswords[i]);
        }

        assert params.mAutofillAssistantUrl != null : "--autofill-assistant-url must be provided.";
        assert !GURL.isEmptyOrInvalid(params.mDomainUrl) : "Valid --domain-url must be provided.";
        assert params.mUsername != null : "--username must be provided.";
        assert params.mDebugBundleId != null : "--debug-bundle-id must be provided.";
        assert params.mDebugSocketId != null : "--debug-socket-id must be provided.";
        assert params.mNumRuns > 0 : "--num-runs must be greater than 0.";

        return params;
    }

    public String getAutofillAssistantUrl() {
        return mAutofillAssistantUrl;
    }

    public GURL getDomainUrl() {
        return mDomainUrl;
    }

    public String getUsername() {
        return mUsername;
    }

    public PasswordStoreCredential[] getSeedCredentials() {
        return mSeedCredentials;
    }

    public String getDebugBundleId() {
        return mDebugBundleId;
    }

    public String getDebugSocketId() {
        return mDebugSocketId;
    }

    public int getNumRuns() {
        return mNumRuns;
    }
}
