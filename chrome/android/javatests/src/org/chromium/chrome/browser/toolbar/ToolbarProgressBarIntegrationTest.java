// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content_public.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Integration tests for the toolbar progress bar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE)
public class ToolbarProgressBarIntegrationTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/progressbar_test.html";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ToolbarProgressBar mProgressBar;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mProgressBar =
                mActivityTestRule.getActivity().getToolbarManager().getToolbar().getProgressBar();

        mProgressBar.resetStartCountForTesting();
        ThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.finish(false));
    }

    /** Test that the progress bar only traverses the page a single time per navigation. */
    @Test
    @Feature({"Android-Progress-Bar"})
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1269029")
    public void testProgressBarTraversesScreenOnce() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        final WebContents webContents = mActivityTestRule.getWebContents();

        TestWebContentsObserver observer =
                ThreadUtils.runOnUiThreadBlocking(() -> new TestWebContentsObserver(webContents));
        // Start and stop load events are carefully tracked; there should be two start-stop pairs
        // that do not overlap.
        OnPageStartedHelper startHelper = observer.getOnPageStartedHelper();
        OnPageFinishedHelper finishHelper = observer.getOnPageFinishedHelper();

        // Ensure no load events have occurred yet.
        assertEquals("Page load should not have started.", 0, startHelper.getCallCount());
        assertEquals("Page load should not have finished.", 0, finishHelper.getCallCount());

        mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));

        // Wait for the initial page to be loaded if it hasn't already.
        if (finishHelper.getCallCount() == 0) {
            finishHelper.waitForCallback(finishHelper.getCallCount(), 1);
        }

        // Exactly one start load and one finish load event should have occurred.
        assertEquals("Page load should have started.", 1, startHelper.getCallCount());
        assertEquals("Page load should have finished.", 1, finishHelper.getCallCount());

        // Load content in the iframe of the test page to trigger another load.
        JavaScriptUtils.executeJavaScript(webContents, "loadIframeInPage();");

        // A load start will be triggered.
        startHelper.waitForCallback(startHelper.getCallCount(), 1);
        assertEquals("Iframe should have triggered page load.", 2, startHelper.getCallCount());

        // Wait for the iframe to finish loading.
        finishHelper.waitForCallback(finishHelper.getCallCount(), 1);
        assertEquals("Iframe should have finished loading.", 2, finishHelper.getCallCount());

        // Though the page triggered two load events, the progress bar should have only appeared a
        // single time.
        assertEquals(
                "The progress bar should have only started once.",
                1,
                mProgressBar.getStartCountForTesting());
    }
}
