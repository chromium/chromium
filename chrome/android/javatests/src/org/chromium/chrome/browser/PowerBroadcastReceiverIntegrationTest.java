// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.PowerBroadcastReceiver.ServiceRunnable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/** Test suite for verifying PowerBroadcastReceiver is initialized. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PowerBroadcastReceiverIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Test
    @SmallTest
    @Feature({"Omaha", "Sync"})
    public void testEnsurePowerBroadcastReceiverRegisteredWhenLaunched() throws Exception {
        final CallbackHelper callback = new CallbackHelper();
        PowerBroadcastReceiver receiver = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> ChromeActivitySessionTracker.getInstance()
                                   .getPowerBroadcastReceiverForTesting());
        receiver.setServiceRunnableForTests(new ServiceRunnable() {
            @Override
            public void post() {
                callback.notifyCalled();
            }
        });
        mActivityTestRule.startMainActivityOnBlankPage();
        try {
            callback.waitForFirst();
        } catch (TimeoutException e) {
            Assert.fail("PowerBroadReceiver never initialized");
        }
    }
}
