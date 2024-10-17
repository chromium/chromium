// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;

/** Test utils for hub search. */
public class TabSwitcherSearchTestUtils {
    /** Sets the given server port, and returns the EmbeddedTestServer. */
    public static EmbeddedTestServer setServerPortAndGetTestServer(
            ChromeTabbedActivityTestRule activityTestRule, int serverPort) {
        activityTestRule.getEmbeddedTestServerRule().setServerPort(serverPort);
        return activityTestRule.getTestServer();
    }

    /** Opens the given urls in new tabs. Doesn't support "chrome:"" urls. */
    public static void openUrls(
            ChromeTabbedActivityTestRule activityTestRule,
            List<String> urlsToOpen,
            boolean incognito) {
        for (String url : urlsToOpen) {
            activityTestRule.loadUrlInNewTab(
                    activityTestRule.getTestServer().getURL(url), incognito);
        }
    }
}
