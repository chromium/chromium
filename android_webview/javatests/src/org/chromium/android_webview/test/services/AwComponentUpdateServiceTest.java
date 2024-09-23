// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.nonembedded.AwComponentUpdateService;
import org.chromium.android_webview.nonembedded.AwNonembeddedUmaRecorder;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

/** Tests for {@link org.chromium.android_webview.nonembedded.AwComponentUpdateService}. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@Batch(Batch.PER_CLASS)
public class AwComponentUpdateServiceTest {
    private CallbackHelper mCallbackHelper = new CallbackHelper();

    private class AwNonembeddedUmaRecorderForTest extends AwNonembeddedUmaRecorder {
        @Override
        public void recordExponentialHistogram(
                String name, int sample, int min, int max, int numBuckets) {
            mCallbackHelper.notifyCalled();
        }
    }

    private AwNonembeddedUmaRecorderForTest mUmaRecorder = new AwNonembeddedUmaRecorderForTest();

    @Before
    public void setup() {
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorder);
        final Context context = ContextUtils.getApplicationContext();
        final SharedPreferences sharedPreferences =
                context.getSharedPreferences(
                        AwComponentUpdateService.SHARED_PREFERENCES_NAME, Context.MODE_PRIVATE);
        sharedPreferences
                .edit()
                .putBoolean(AwComponentUpdateService.KEY_UNEXPECTED_EXIT, false)
                .apply();
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExpectedHistogramsAreCalled() throws Throwable {
        AwComponentUpdateService.setSharedPreferences(
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                AwComponentUpdateService.SHARED_PREFERENCES_NAME,
                                Context.MODE_PRIVATE));
        AwComponentUpdateService svc = new AwComponentUpdateService();
        int cbCount = mCallbackHelper.getCallCount();
        boolean success =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return svc.maybeStartUpdates(false);
                        });
        mCallbackHelper.waitForCallback(cbCount);
        Assert.assertTrue(success);
    }
}
