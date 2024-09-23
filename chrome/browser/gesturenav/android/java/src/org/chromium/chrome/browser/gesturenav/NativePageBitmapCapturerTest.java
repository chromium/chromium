// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.concurrent.TimeoutException;

/** Test whether {@link NativePageBitmapCapturer} can correctly capture views. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.BACK_FORWARD_TRANSITIONS})
public class NativePageBitmapCapturerTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Test
    @SmallTest
    public void testWithNativePage() throws TimeoutException {
        mTabbedActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            NativePageBitmapCapturer.maybeCaptureNativeView(
                                    mTabbedActivityTestRule.getActivity().getActivityTab(),
                                    (bitmap) -> {
                                        callbackHelper.notifyCalled();
                                    }));
                });

        callbackHelper.waitForOnly();
    }

    @Test
    @SmallTest
    public void testWithNonNativePage() {
        mTabbedActivityTestRule.startMainActivityOnBlankPage();

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            NativePageBitmapCapturer.maybeCaptureNativeView(
                                    mTabbedActivityTestRule.getActivity().getActivityTab(),
                                    (bitmap) -> {
                                        callbackHelper.notifyCalled();
                                    }));
                });

        // Capture will be finished before the following task.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Assert.assertEquals(0, callbackHelper.getCallCount());
                });
    }
}
