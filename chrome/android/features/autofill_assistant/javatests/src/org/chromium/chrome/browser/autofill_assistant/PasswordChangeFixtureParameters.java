// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.CommandLine;

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
    private String mDomainUrl;
    /**
     * Username credential for login. Provided via --username.
     */
    private String mUsername;
    /**
     * Password credential for login. Provided via --password.
     */
    private String mPassword;
    /**
     * Script debug bundle id. Provided via --debug-bundle-id.
     */
    private String mDebugBundleId;
    /**
     * Script debug socket id. Provided via --debug-socket-id.
     */
    private String mDebugSocketId;

    /**
     * Loads test parameters from command line.
     */
    public static PasswordChangeFixtureParameters loadFromCommandLine() {
        assert CommandLine.isInitialized() : "CommandLine is expected to be initialized.";

        PasswordChangeFixtureParameters params = new PasswordChangeFixtureParameters();
        params.mAutofillAssistantUrl =
                CommandLine.getInstance().getSwitchValue("autofill-assistant-url");
        params.mDomainUrl = CommandLine.getInstance().getSwitchValue("domain-url");
        params.mUsername = CommandLine.getInstance().getSwitchValue("username");
        params.mPassword = CommandLine.getInstance().getSwitchValue("password", "");
        params.mDebugBundleId = CommandLine.getInstance().getSwitchValue("debug-bundle-id");
        params.mDebugSocketId = CommandLine.getInstance().getSwitchValue("debug-socket-id");

        assert params.mAutofillAssistantUrl != null : "--autofill-assistant-url must be provided.";
        assert params.mDomainUrl != null : "--domain-url must be provided.";
        assert params.mUsername != null : "--username must be provided.";
        assert params.mDebugBundleId != null : "--debug-bundle-id must be provided.";
        assert params.mDebugSocketId != null : "--debug-socket-id must be provided.";

        return params;
    }

    public String getAutofillAssistantUrl() {
        return mAutofillAssistantUrl;
    }

    public String getDomainUrl() {
        return mDomainUrl;
    }

    public String getUsername() {
        return mUsername;
    }

    public String getPassword() {
        return mPassword;
    }

    public String getDebugBundleId() {
        return mDebugBundleId;
    }

    public String getDebugSocketId() {
        return mDebugSocketId;
    }
}
