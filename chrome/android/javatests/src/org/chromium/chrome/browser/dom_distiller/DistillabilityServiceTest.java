// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarContainerObserver;
import org.chromium.chrome.browser.infobar.ReaderModeInfoBar;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests for making sure the distillability service is communicating correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        "enable-dom-distiller", "reader-mode-heuristics=alwaystrue",
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
public class DistillabilityServiceTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Make sure that Reader Mode appears after navigating from a native page.
     */
    @Test
    @Feature({"Distillability-Service"})
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testServiceAliveAfterNativePage() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        final CallbackHelper readerShownCallbackHelper = new CallbackHelper();

        mActivityTestRule.getInfoBarContainer().addObserver(new InfoBarContainerObserver() {
            @Override
            public void onAddInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isFirst) {
                if (infoBar instanceof ReaderModeInfoBar) readerShownCallbackHelper.notifyCalled();
            }

            @Override
            public void onRemoveInfoBar(
                    InfoBarContainer container, InfoBar infoBar, boolean isLast) {}

            @Override
            public void onInfoBarContainerAttachedToWindow(boolean hasInfobars) {}

            @Override
            public void onInfoBarContainerShownRatioChanged(
                    InfoBarContainer container, float shownRatio) {}
        });

        TestWebContentsObserver observer =
                new TestWebContentsObserver(mActivityTestRule.getWebContents());
        OnPageFinishedHelper finishHelper = observer.getOnPageFinishedHelper();

        // Navigate to a native page.
        int curCallCount = finishHelper.getCallCount();
        mActivityTestRule.loadUrl("chrome://history/");
        finishHelper.waitForCallback(curCallCount, 1);
        Assert.assertEquals(0, readerShownCallbackHelper.getCallCount());

        // Navigate to a normal page.
        curCallCount = readerShownCallbackHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));
        readerShownCallbackHelper.waitForCallback(curCallCount, 1);
        Assert.assertEquals(1, readerShownCallbackHelper.getCallCount());
    }
}
