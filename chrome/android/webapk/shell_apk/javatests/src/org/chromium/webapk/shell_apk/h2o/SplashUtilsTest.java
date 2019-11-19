// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

/** Tests for {@link SplashUtils} */
@RunWith(BaseJUnit4ClassRunner.class)
public class SplashUtilsTest {
    /**
     * Tests that {@link SplashUtils#createAndImmediatelyScreenshotSplashView{}} returns a non-blank
     * bitmap.
     */
    @Test
    @SmallTest
    public void testCreateAndImmediatelyScreenshotSplashView() {
        // Request large splash screen so that icon does not take up all of the space.
        final int requestedSplashWidth = 1000;
        final int requestedSplashHeight = 1000;
        Bitmap screenshot = SplashUtils.createAndImmediatelyScreenshotSplashView(
                InstrumentationRegistry.getTargetContext(), requestedSplashWidth,
                requestedSplashHeight, 1024 * 1024 * 4 /* maxSizeBytes */);
        Assert.assertNotNull(screenshot);
        Assert.assertEquals(requestedSplashWidth, screenshot.getWidth());
        Assert.assertEquals(requestedSplashHeight, screenshot.getHeight());

        // Check that the screenshot is non-blank.
        Assert.assertTrue(!allPixelsHaveSameColor(screenshot));
    }

    private boolean allPixelsHaveSameColor(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        if (width == 0 || height == 0) return true;

        int[] pixels = new int[width * height];
        try {
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        } catch (Exception e) {
            Assert.fail();
        }
        int firstColor = pixels[0];
        for (int i = 1; i < pixels.length; ++i) {
            if (pixels[i] != firstColor) return false;
        }
        return true;
    }
}
