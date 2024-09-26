// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.util.concurrent.TimeoutException;

/** Test suite for verifying {@link OmahaServiceStartDelayer} is initialized. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OmahaServiceStartDelayerIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Test
    @SmallTest
    @Feature({"Omaha"})
    public void testEnsureOmahaServiceStartDelayerIsInitializedWhenLaunched() throws Exception {
        final CallbackHelper callback = new CallbackHelper();
        OmahaServiceStartDelayer receiver =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ChromeActivitySessionTracker.getInstance()
                                        .getOmahaServiceStartDelayerForTesting());
        receiver.setOmahaRunnableForTesting(() -> callback.notifyCalled());
        mActivityTestRule.startMainActivityOnBlankPage();
        try {
            callback.waitForOnly();
        } catch (TimeoutException e) {
            throw new AssertionError("OmahaServiceStartDelayer never initialized", e);
        }
    }
}
