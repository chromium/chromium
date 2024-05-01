// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.junit.Assert;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * An implementation of PlatformServiceBridge that can be used to wait for metrics logs in tests.
 */
public class MetricsTestPlatformServiceBridge extends PlatformServiceBridge {
    private final BlockingQueue<byte[]> mQueue;
    private int mStatus;

    public MetricsTestPlatformServiceBridge() {
        mQueue = new LinkedBlockingQueue<>();
        // Default the status code to success
        mStatus = 0;
    }

    @Override
    public boolean canUseGms() {
        return true;
    }

    @Override
    public void queryMetricsSetting(Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        callback.onResult(/* enabled= */ true);
    }

    @Override
    public void logMetrics(byte[] data) {
        mQueue.add(data);
    }

    @Override
    public int logMetricsBlocking(byte[] data) {
        logMetrics(data);
        return mStatus;
    }

    /**
     * This method can be used to set the status code to return from the {@link logMetricsBlocking}.
     */
    public void setLogMetricsBlockingStatus(int status) {
        mStatus = status;
    }

    /** Gets the latest metrics log we've received. */
    public ChromeUserMetricsExtension waitForNextMetricsLog() throws Exception {
        byte[] data = AwActivityTestRule.waitForNextQueueElement(mQueue);
        return ChromeUserMetricsExtension.parseFrom(data);
    }

    /** Asserts there are no more metrics logs queued up. */
    public void assertNoMetricsLogs() throws Exception {
        // Assert the size is zero (rather than the queue is empty), so if this fails we have
        // some hint as to how many logs were queued up.
        Assert.assertEquals("Expected no metrics logs to be in the queue", 0, mQueue.size());
    }
}
