// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.SystemClock;
import android.view.MotionEvent;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;

/** Tests for WebView nested scrolling APIs. */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class NestedScrollingApiTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetNestedScrollingEnabledTrueRecordsHistogram() throws Exception {
        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);

        ThreadUtils.runOnUiThreadBlocking(() -> testView.setNestedScrollingEnabled(true));

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.WebView.NestedScrollingEnabled", true)
                        .build()) {
            long eventTime = SystemClock.uptimeMillis();
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            testView.onTouchEvent(
                                    MotionEvent.obtain(
                                            eventTime,
                                            eventTime,
                                            MotionEvent.ACTION_DOWN,
                                            0,
                                            0,
                                            0)));
            watcher.assertExpected();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetNestedScrollingEnabledFalseRecordsHistogram() throws Exception {
        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);

        ThreadUtils.runOnUiThreadBlocking(() -> testView.setNestedScrollingEnabled(false));

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.WebView.NestedScrollingEnabled", false)
                        .build()) {
            long eventTime = SystemClock.uptimeMillis();
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            testView.onTouchEvent(
                                    MotionEvent.obtain(
                                            eventTime,
                                            eventTime,
                                            MotionEvent.ACTION_DOWN,
                                            0,
                                            0,
                                            0)));
            watcher.assertExpected();
        }
    }
}
