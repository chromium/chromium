// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwDawnLoader;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

/** Tests for {@link AwDawnLoader}. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS)
@Batch(Batch.PER_CLASS)
public class AwDawnLoaderTest {

    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetWGPUProcGetProcAddressHandshake() {
        long handleAtZeroMinor = AwDawnLoader.getWGPUProcGetProcAddress(1, 0L);
        long handle = AwDawnLoader.getWGPUProcGetProcAddress(1, 2026100200L);
        Assert.assertTrue(
                "getWGPUProcGetProcAddress should return a positive proc address", handle > 0L);
        Assert.assertEquals(handleAtZeroMinor, handle);

        long webviewMinorTooOld = AwDawnLoader.getWGPUProcGetProcAddress(1, Long.MAX_VALUE);
        Assert.assertEquals(AwDawnLoader.WEBGPU_ERROR_WEBVIEW_TOO_OLD, webviewMinorTooOld);

        long webviewMajorTooOld = AwDawnLoader.getWGPUProcGetProcAddress(999, 2026100200L);
        Assert.assertEquals(AwDawnLoader.WEBGPU_ERROR_WEBVIEW_TOO_OLD, webviewMajorTooOld);

        long max24BitMajor = AwDawnLoader.getWGPUProcGetProcAddress(0xFFFFFF, 0L);
        Assert.assertEquals(AwDawnLoader.WEBGPU_ERROR_WEBVIEW_TOO_OLD, max24BitMajor);

        long max40BitMinor = AwDawnLoader.getWGPUProcGetProcAddress(1, 0xFFFFFFFFFFL);
        Assert.assertEquals(AwDawnLoader.WEBGPU_ERROR_WEBVIEW_TOO_OLD, max40BitMinor);

        long appMajorTooOld = AwDawnLoader.getWGPUProcGetProcAddress(0, 2026100200L);
        Assert.assertEquals(AwDawnLoader.WEBGPU_ERROR_APP_TOO_OLD, appMajorTooOld);

        long negativeMajor = AwDawnLoader.getWGPUProcGetProcAddress(-1, 2026100200L);
        Assert.assertEquals(AwDawnLoader.WEBGPU_ERROR_APP_TOO_OLD, negativeMajor);

        long malformed = AwDawnLoader.getWGPUProcGetProcAddress(1, -1L);
        Assert.assertEquals(AwDawnLoader.WEBGPU_ERROR_APP_TOO_OLD, malformed);

        long minLongMinor = AwDawnLoader.getWGPUProcGetProcAddress(1, Long.MIN_VALUE);
        Assert.assertEquals(AwDawnLoader.WEBGPU_ERROR_APP_TOO_OLD, minLongMinor);
    }
}
