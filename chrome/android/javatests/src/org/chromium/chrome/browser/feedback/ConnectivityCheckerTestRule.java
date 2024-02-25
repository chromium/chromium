// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Base class for tests related to checking connectivity.
 *
 * <p>It includes a {@link ConnectivityTestServer} which is set up and torn down automatically for
 * tests.
 */
public class ConnectivityCheckerTestRule extends ChromeBrowserTestRule {
    public static final int TIMEOUT_MS = 5000;

    private EmbeddedTestServer mTestServer;
    private String mGenerated200Url;
    private String mGenerated204Url;
    private String mGenerated302Url;
    private String mGenerated404Url;
    private String mGeneratedSlowUrl;

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        setUp();
                        base.evaluate();
                    }
                },
                description);
    }

    public String getGenerated200Url() {
        return mGenerated200Url;
    }

    public String getGenerated204Url() {
        return mGenerated204Url;
    }

    public String getGenerated302Url() {
        return mGenerated302Url;
    }

    public String getGenerated404Url() {
        return mGenerated404Url;
    }

    public String getGeneratedSlowUrl() {
        return mGeneratedSlowUrl;
    }

    private void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mGenerated200Url = mTestServer.getURL("/echo?status=200");
        mGenerated204Url = mTestServer.getURL("/echo?status=204");
        mGenerated302Url = mTestServer.getURL("/echo?status=302");
        mGenerated404Url = mTestServer.getURL("/echo?status=404");
        mGeneratedSlowUrl = mTestServer.getURL("/slow?5");
    }
}
