// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicReference;

/** Class containing utility functions for interacting with VrShellDelegate during tests. */
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
                () -> {
                    delegate.set(TestVrShellDelegate.getInstance());
                });
        return delegate.get();
    }
}
