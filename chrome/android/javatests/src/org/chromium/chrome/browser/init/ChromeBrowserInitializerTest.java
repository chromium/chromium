// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Tests for ChromeBrowserInitializer. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ChromeBrowserInitializerTest {
    private ChromeBrowserInitializer mInstance;

    @Before
    public void setUp() {
        mInstance = ChromeBrowserInitializer.getInstance();
        Assert.assertFalse(mInstance.hasNativeInitializationCompleted());
    }

    @Test
    @SmallTest
    public void testSynchronousInitialization() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mInstance.hasNativeInitializationCompleted());
            mInstance.handleSynchronousStartup();
            Assert.assertTrue(mInstance.hasNativeInitializationCompleted());
            return true;
        });
    }

    @Test
    @SmallTest
    public void testAsynchronousStartup() throws Exception {
        final Semaphore done = new Semaphore(0);
        BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                done.release();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mInstance.hasNativeInitializationCompleted());
            mInstance.handlePreNativeStartup(parts);
            mInstance.handlePostNativeStartup(true, parts);

            Assert.assertFalse(
                    "Should not be synchronous", mInstance.hasNativeInitializationCompleted());
            return true;
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Inititialization tasks should yield to new UI thread tasks",
                    mInstance.hasNativeInitializationCompleted());
        });
        Assert.assertTrue(done.tryAcquire(10, TimeUnit.SECONDS));
    }

    @Test
    @SmallTest
    public void testDelayedTasks() throws Exception {
        final Semaphore done = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mInstance.runNowOrAfterNativeInitialization(done::release);
            Assert.assertFalse("Should not run synchronously", done.tryAcquire());
            mInstance.handleSynchronousStartup();
            Assert.assertTrue(done.tryAcquire());
            return true;
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mInstance.runNowOrAfterNativeInitialization(done::release);
            // Runs right away in the same task is initialization is done.
            Assert.assertTrue(done.tryAcquire());
        });
    }
}
