// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.reader_mode;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.reader_mode.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests for making sure the distillability service is communicating correctly. */
// TODO(crbug.com/579933598): Manage EmbeddedTestServer lifecycle and investigate batching.
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "crbug.com/579933598: Starts an unmanaged EmbeddedTestServer")
public class DistillabilityServiceTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEST_PAGE = "/chrome/test/data/reader_mode/simple_article.html";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();
    }

    /** Make sure that distillability is determined after navigating from a native page. */
    @Test
    @Feature({"Distillability-Service"})
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testServiceAliveAfterNativePage() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        final CallbackHelper distillabilityCallbackHelper = new CallbackHelper();

        TestWebContentsObserver observer =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new TestWebContentsObserver(mActivityTestRule.getWebContents()));
        OnPageFinishedHelper finishHelper = observer.getOnPageFinishedHelper();
        TabDistillabilityProvider distillabilityProvider =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> TabDistillabilityProvider.get(mActivityTestRule.getActivityTab()));
        Assert.assertNotNull(distillabilityProvider);
        DistillabilityObserver distillabilityObserver =
                (Tab tab, boolean isDistillable, boolean isLast, boolean isMobileOptimized) -> {
                    if (isDistillable && isLast) {
                        distillabilityCallbackHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> distillabilityProvider.addObserver(distillabilityObserver));

        try {
            // Navigate to a native page.
            int curCallCount = finishHelper.getCallCount();
            mActivityTestRule.loadUrl("chrome://history/");
            finishHelper.waitForCallback(curCallCount, /* numberOfCallsToWaitFor= */ 1);
            Assert.assertEquals(0, distillabilityCallbackHelper.getCallCount());

            // Navigate to a normal page.
            curCallCount = distillabilityCallbackHelper.getCallCount();
            mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));
            distillabilityCallbackHelper.waitForCallback(
                    curCallCount, /* numberOfCallsToWaitFor= */ 1);
            Assert.assertEquals(1, distillabilityCallbackHelper.getCallCount());
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        distillabilityProvider.removeObserver(distillabilityObserver);
                        observer.observe(null);
                    });
        }
    }
}
