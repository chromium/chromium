// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwHttpAuthHandler;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** JUnit tests for AwHttpAuthHandler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwHttpAuthHandlerTest {
    private AwHttpAuthHandler mAuthHandler;

    @Before
    public void setUp() {
        mAuthHandler = AwHttpAuthHandler.create(0, true);
    }

    @Test
    public void testProceed_executesOnUiThread() {
        mAuthHandler.proceed("", "");
    }

    @Test
    public void testCancel_executesOnUiThread() {
        mAuthHandler.cancel();
    }

    @Test
    public void testIsFirstAttempt_executesOnUiThread() {
        mAuthHandler.isFirstAttempt();
    }

    @Test
    public void testProceed_dontExecuteBackgroundThread() {
        throwsInBackground(() -> mAuthHandler.proceed("", ""));
    }

    @Test
    public void testCancel_dontExecuteBackgroundThread() {
        throwsInBackground(() -> mAuthHandler.cancel());
    }

    @Test
    public void testIsFirstAttempt_dontExecuteBackgroundThread() {
        throwsInBackground(() -> mAuthHandler.isFirstAttempt());
    }

    private void throwsInBackground(Runnable r) {
        Thread thread =
                new Thread(
                        () -> {
                            try {
                                r.run();
                                Assert.fail("Should have thrown on background thread");
                            } catch (IllegalStateException e) {
                                // Smooth sailing, we expected this
                            }
                        });
        thread.start();

        try {
            thread.join();
        } catch (InterruptedException e) {
            throw new AssertionError("Background thread interrupted", e);
        }
    }
}
