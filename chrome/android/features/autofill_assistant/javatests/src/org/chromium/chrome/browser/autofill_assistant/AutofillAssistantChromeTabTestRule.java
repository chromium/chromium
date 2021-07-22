// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.support.test.InstrumentationRegistry;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.net.test.EmbeddedTestServer;

class AutofillAssistantChromeTabTestRule
        extends AutofillAssistantTestRule<ChromeTabbedActivityTestRule> {
    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";

    private final String mTestPage;
    private final EmbeddedTestServer mTestServer;

    AutofillAssistantChromeTabTestRule(ChromeTabbedActivityTestRule testRule, String testPage) {
        super(testRule);
        mTestPage = testPage;
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    String getURL(String page) {
        return mTestServer.getURL(HTML_DIRECTORY + page);
    }

    @Override
    public void startActivity() {
        getTestRule().startMainActivityWithURL(getURL(mTestPage));
    }

    @Override
    public void cleanupAfterTest() {
        mTestServer.stopAndDestroyServer();
    }
}
