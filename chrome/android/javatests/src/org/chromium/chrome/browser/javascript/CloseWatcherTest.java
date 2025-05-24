// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.javascript;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** Unit tests for CloseWatcher's ability to receive signals from the system back button. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-experimental-web-platform-features",
    "enable-features=CloseWatcher"
})
@Batch(Batch.PER_CLASS)
public class CloseWatcherTest {

    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    // Define constants for URLs and expected title
    private static final String CLOSE_WATCHER_TEST_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<body><script>let watcher = new CloseWatcher(); watcher.onclose = () =>"
                            + " window.document.title = 'SUCCESS';</script></body>");

    private static final String DIALOG_ELEMENT_TEST_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<dialog id=mydialog>hello</dialog>"
                            + "<script>mydialog.showModal();mydialog.onclose = () =>"
                            + " window.document.title = 'SUCCESS';</script>");

    private static final String EXPECTED_TITLE_SUCCESS = "SUCCESS";

    @Test
    @MediumTest
    public void testBackButtonTriggersCloseWatcher() {
        WebPageStation initialStation = mTestRule.start();
        WebPageStation loadedPage =
                initialStation.loadWebPageProgrammatically(CLOSE_WATCHER_TEST_URL);
        WebPageStation finalStationExpected =
                WebPageStation.newBuilder()
                        .initFrom(loadedPage)
                        .withExpectedTitle(EXPECTED_TITLE_SUCCESS)
                        .build();
        loadedPage.pressBack(finalStationExpected);

        // No explicit assertion needed; pressBack ensures the transition completed successfully.
    }

    @Test
    @MediumTest
    public void testBackButtonClosesDialogElement() {
        WebPageStation initialStation = mTestRule.start();
        WebPageStation loadedPage =
                initialStation.loadWebPageProgrammatically(DIALOG_ELEMENT_TEST_URL);
        WebPageStation finalStationExpected =
                WebPageStation.newBuilder()
                        .withExpectedTitle(EXPECTED_TITLE_SUCCESS)
                        .initFrom(loadedPage)
                        .build();
        loadedPage.pressBack(finalStationExpected);

        // No explicit assertion needed.
    }
}
