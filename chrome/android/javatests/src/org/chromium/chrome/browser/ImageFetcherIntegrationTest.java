// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.url.GURL;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/** Tests for {@link ImageFetcher}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ImageFetcherIntegrationTest {
    @ClassRule public static final ChromeBrowserTestRule sRule = new ChromeBrowserTestRule();

    @ClassRule
    public static final EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    private static class TestImageFetcherCallback extends CallbackHelper
            implements Callback<Bitmap> {
        public Bitmap mBitmap;

        @Override
        public void onResult(Bitmap bitmap) {
            mBitmap = bitmap;
            notifyCalled();
        }
    }

    /** Fetches image from ImageFetcher and waits for callback. */
    private static Bitmap fetchImageAndWait(
            String url, int desiredWidth, int desiredHeight, boolean shouldResize)
            throws Exception {
        TestImageFetcherCallback callbackWaiter = new TestImageFetcherCallback();
        ThreadUtils.runOnUiThreadBlocking(
                new Callable<Void>() {
                    @Override
                    public Void call() throws TimeoutException {
                        ImageFetcher.Params params =
                                shouldResize
                                        ? ImageFetcher.Params.create(
                                                url, "random", desiredWidth, desiredHeight)
                                        : ImageFetcher.Params.createNoResizing(
                                                new GURL(url),
                                                "random",
                                                desiredWidth,
                                                desiredHeight);
                        ImageFetcher imageFetcher =
                                ImageFetcherFactory.createImageFetcher(
                                        ImageFetcherConfig.NETWORK_ONLY,
                                        ProfileManager.getLastUsedRegularProfile().getProfileKey());
                        imageFetcher.fetchImage(params, callbackWaiter);
                        return null;
                    }
                });
        callbackWaiter.waitForOnly();
        return callbackWaiter.mBitmap;
    }

    /**
     * Test that ImageFetcher#fetchImage() selects the .ico frame based on the passed-in desired
     * downloads size.
     */
    @Test
    @MediumTest
    public void testDesiredFrameSizeFavicon() throws Exception {
        String icoUrl =
                sTestServerRule
                        .getServer()
                        .getURL("/chrome/test/data/android/image_fetcher/icon.ico");

        Bitmap bitmap = fetchImageAndWait(icoUrl, 59, 59, /* shouldResize= */ false);
        assertNotNull(bitmap);
        assertEquals(Color.RED, bitmap.getPixel(0, 0));
        assertEquals(60, bitmap.getWidth());

        bitmap = fetchImageAndWait(icoUrl, 59, 59, /* shouldResize= */ true);
        assertNotNull(bitmap);
        assertEquals(Color.RED, bitmap.getPixel(0, 0));
        assertEquals(59, bitmap.getWidth());

        bitmap = fetchImageAndWait(icoUrl, 120, 120, /* shouldResize= */ false);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
        assertEquals(120, bitmap.getWidth());
    }
}
