// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.base.SplitCompatApplication;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Integration test suite for the failure to raise appropriate exceptions and launch a dialog when
 * the native library cannot be loaded.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test interacts with startup.")
public class LaunchFailedActivityTest {

    private static class MockContext extends ContextWrapper {
        public MockContext(Context context) {
            super(context);
        }

        private Intent mLastIntent;

        @Override
        public void startActivity(Intent intent, Bundle options) {
            mLastIntent = intent;
        }
    }

    @Test
    @SmallTest
    public void testLaunchFailedWithoutCallbackRaisesProcessInitException() {
        LibraryLoader.setLoadFailedCallbackForTesting(null);
        LibraryLoader.setOverrideNativeLibraryCannotBeLoadedForTesting();

        ProcessInitException caught = null;
        try {
            LibraryLoader.getInstance().ensureInitialized();
        } catch (ProcessInitException e) {
            caught = e;
        }
        Assert.assertNotNull("Did not throw ProcessInitException", caught);
    }

    @Test
    @SmallTest
    public void testLaunchFailedWithCallbackRaisesExceptionAndStartsActivity() {
        MockContext mockContext = new MockContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mockContext);
        LibraryLoader.setOverrideNativeLibraryCannotBeLoadedForTesting();

        RuntimeException caught = null;
        try {
            LibraryLoader.getInstance().ensureInitialized();
        } catch (RuntimeException e) {
            caught = e;
        }
        Assert.assertNotNull("Did not throw RuntimeException", caught);
        Assert.assertTrue(caught.getMessage().contains("64-bit"));

        // Check for the intent instead of using an ActivityMonitor since LaunchFailedActivity is
        // started in a different process that is not instrumented.
        CriteriaHelper.pollUiThread(
                () -> {
                    return mockContext.mLastIntent != null
                            && mockContext
                                    .mLastIntent
                                    .getComponent()
                                    .getClassName()
                                    .equals(
                                            SplitCompatApplication
                                                    .LAUNCH_FAILED_ACTIVITY_CLASS_NAME);
                });
    }
}
