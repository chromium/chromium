// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import org.junit.Assert;

import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.VrCoreInfo;
import org.chromium.chrome.browser.vr.mock.MockVrCoreVersionChecker;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Class containing utility functions for interacting with VrShellDelegate
 * during tests.
 */
public class VrShellDelegateUtils {
    /**
     * Retrieves the current VrShellDelegate instance from the UI thread. This is necessary in case
     * acquiring the instance causes the delegate to be constructed, which must happen on the UI
     * thread.
     *
     * @return The TestVrShellDelegate instance currently in use.
     */
    public static TestVrShellDelegate getDelegateInstance() {
        final AtomicReference<TestVrShellDelegate> delegate =
                new AtomicReference<TestVrShellDelegate>();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { delegate.set(TestVrShellDelegate.getInstance()); });
        return delegate.get();
    }

    /**
     * Creates and sets a MockVrCoreVersionCheckerImpl as the VrShellDelegate's VrCoreVersionChecker
     * instance.
     *
     * @param compatibility An int corresponding to a VrCoreCompatibility value that the mock
     *        version checker will return.
     * @return The MockVrCoreVersionCheckerImpl that was set as VrShellDelegate's
     *        VrCoreVersionChecker instance.
     */
    public static MockVrCoreVersionChecker setVrCoreCompatibility(int compatibility) {
        final MockVrCoreVersionChecker mockChecker = new MockVrCoreVersionChecker();
        mockChecker.setMockReturnValue(new VrCoreInfo(null, compatibility));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            VrShellDelegateUtils.getDelegateInstance().overrideVrCoreVersionCheckerForTesting(
                    mockChecker);
        });
        Assert.assertEquals("Overriding VrCoreVersionChecker failed", compatibility,
                mockChecker.getLastReturnValue().compatibility);
        return mockChecker;
    }
}
