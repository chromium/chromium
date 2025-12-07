// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.delegate.ScreenshotResult;

import java.util.concurrent.TimeoutException;

/** Test whether {@link NativePageBitmapCapturer} can correctly capture views. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NativePageBitmapCapturerTest {
    @Rule
    public FreshCtaTransitTestRule mTabbedActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        NativePageBitmapCapturer.setIgnoreCurrentUrlCheckForTesting();
    }

    @Test
    @SmallTest
    public void testWithNativePage() throws TimeoutException {
        RegularNewTabPageStation ntp = mTabbedActivityTestRule.startOnNtp();

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            NativePageBitmapCapturer.maybeCaptureNativeView(
                                    ntp.getTab(),
                                    (result) -> {
                                        Assert.assertNotNull(result);
                                        Assert.assertNotNull(result.getBitmap());
                                        callbackHelper.notifyCalled();
                                    },
                                    ScreenshotResult.Destination.BITMAP));
                });

        callbackHelper.waitForOnly();
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_less_than = 31)
    public void testWithNativePageHardwareBuffer() throws TimeoutException {
        RegularNewTabPageStation ntp = mTabbedActivityTestRule.startOnNtp();

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            NativePageBitmapCapturer.maybeCaptureNativeView(
                                    ntp.getTab(),
                                    (ScreenshotResult result) -> {
                                        Assert.assertNotNull(result);
                                        Assert.assertNotNull(result.getHardwareBuffer());
                                        Runnable releaseCallback = result.getReleaseCallback();
                                        callbackHelper.notifyCalled();
                                        releaseCallback.run();
                                    },
                                    ScreenshotResult.Destination.HARDWARE_BUFFER));
                });

        callbackHelper.waitForOnly();
    }

    @Test
    @SmallTest
    public void testWithNonNativePage() {
        WebPageStation blankPage = mTabbedActivityTestRule.startOnBlankPage();

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            NativePageBitmapCapturer.maybeCaptureNativeView(
                                    blankPage.getTab(),
                                    (result) -> {
                                        callbackHelper.notifyCalled();
                                    },
                                    ScreenshotResult.Destination.BITMAP));
                });

        // Capture will be finished before the following task.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Assert.assertEquals(0, callbackHelper.getCallCount());
                });
    }
}
