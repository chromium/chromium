// Copyright 2022 The Chromium Authors. All rights reserved.
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
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link ImageFetcher}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.UNIT_TESTS)
public class ImageFetcherIntegrationTest {
    @ClassRule
    public static final ChromeBrowserTestRule sRule = new ChromeBrowserTestRule();

    @ClassRule
    public static final EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    private class TestImageFetcherCallback extends CallbackHelper implements Callback<Bitmap> {
        public Bitmap mBitmap;

        @Override
        public void onResult(Bitmap bitmap) {
            mBitmap = bitmap;
            notifyCalled();
        }
    }

    /**
     * Fetches image from ImageFetcher and waits for callback.
     */
    private static void fetchImageAndWait(
            String imageUrl, int size, TestImageFetcherCallback callbackWaiter) throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
            @Override
            public Void call() throws TimeoutException {
                ImageFetcher imageFetcher =
                        ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.NETWORK_ONLY,
                                Profile.getLastUsedRegularProfile().getProfileKey());
                ImageFetcher.Params params =
                        ImageFetcher.Params.create(imageUrl, "random", size, size);
                imageFetcher.fetchImage(params, callbackWaiter);
                return null;
            }
        });
        callbackWaiter.waitForFirst();
    }

    /**
     * Test that ImageFetcher#fetchImage() selects the .ico frame based on the passed-in desired
     * downloads size.
     */
    @Test
    @MediumTest
    public void testDesiredFrameSizeFavicon() throws Exception {
        String icoUrl = sTestServerRule.getServer().getURL(
                "/chrome/test/data/android/image_fetcher/icon.ico");
        {
            TestImageFetcherCallback imageFetcherCallback = new TestImageFetcherCallback();
            fetchImageAndWait(icoUrl, 60, imageFetcherCallback);
            assertNotNull(imageFetcherCallback.mBitmap);
            assertEquals(Color.RED, imageFetcherCallback.mBitmap.getPixel(0, 0));
        }

        {
            TestImageFetcherCallback imageFetcherCallback = new TestImageFetcherCallback();
            fetchImageAndWait(icoUrl, 120, imageFetcherCallback);
            assertNotNull(imageFetcherCallback.mBitmap);
            assertEquals(Color.GREEN, imageFetcherCallback.mBitmap.getPixel(0, 0));
        }
    }
}
