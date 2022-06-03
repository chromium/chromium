// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.TargetApi;
import android.app.Activity;
import android.os.Build;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

/**
 * Test of ApiCompatibilityUtils
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ApiCompatibilityUtilsTest {
    private static final long WAIT_TIMEOUT_IN_MS = 5000;
    private static final long SLEEP_INTERVAL_IN_MS = 50;

    static class MockActivity extends Activity {
        int mFinishAndRemoveTaskCallbackCount;
        int mFinishCallbackCount;
        boolean mIsFinishing;

        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        @Override
        public void finishAndRemoveTask() {
            mFinishAndRemoveTaskCallbackCount++;
            if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) mIsFinishing = true;
        }

        @Override
        public void finish() {
            mFinishCallbackCount++;
            mIsFinishing = true;
        }

        @Override
        public boolean isFinishing() {
            return mIsFinishing;
        }
    }

    @Test
    @SmallTest
    public void testFinishAndRemoveTask() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                MockActivity activity = new MockActivity();
                ApiCompatibilityUtils.finishAndRemoveTask(activity);

                if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
                    Assert.assertEquals(1, activity.mFinishAndRemoveTaskCallbackCount);
                    Assert.assertEquals(0, activity.mFinishCallbackCount);
                } else if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP) {
                    long startTime = SystemClock.uptimeMillis();
                    while (activity.mFinishCallbackCount == 0
                            && SystemClock.uptimeMillis() - startTime < WAIT_TIMEOUT_IN_MS) {
                        try {
                            Thread.sleep(SLEEP_INTERVAL_IN_MS);
                        } catch (InterruptedException e) {
                            throw new RuntimeException("Interrupted thread sleep", e);
                        }
                    }

                    // MockActivity#finishAndRemoveTask() never sets isFinishing() to true for
                    // LOLLIPOP to simulate an exceptional case. In that case, MockActivity#finish()
                    // should be called after 3 tries.
                    Assert.assertEquals(3, activity.mFinishAndRemoveTaskCallbackCount);
                    Assert.assertEquals(1, activity.mFinishCallbackCount);
                } else {
                    Assert.assertEquals(0, activity.mFinishAndRemoveTaskCallbackCount);
                    Assert.assertEquals(1, activity.mFinishCallbackCount);
                }
                Assert.assertTrue(activity.mIsFinishing);
            }
        });
    }
}
