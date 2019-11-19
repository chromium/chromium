// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link ExploreSitesBridgeExperimental}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public final class ExploreSitesBridgeExperimentalTest {
    @Rule
    public final ChromeBrowserTestRule mRule = new ChromeBrowserTestRule();

    private static final String TEST_IMAGE = "/chrome/test/data/android/google.png";
    private static final int TIMEOUT_MS = 5000;

    private EmbeddedTestServer mTestServer;
    private Profile mProfile;

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mProfile = Profile.getLastUsedProfile(); });
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private boolean bitmapsEqual(Bitmap expected, Bitmap actual) {
        if (expected.sameAs(actual)) return true;
        // The documentation for Bitmap.sameAs claims that it should return true as long as the
        // images have the same config, dimensions, and pixel data. However, there's a bug of some
        // sort on O+ that makes it sometimes return false even if those conditions are met. As a
        // workaround, fall back to our own comparison logic if sameAs returns false.
        // See https://crbug.com/927014 for more tracking.
        if (expected.getConfig() != actual.getConfig()) return false;
        if (expected.getWidth() != actual.getWidth()
                || expected.getHeight() != actual.getHeight()) {
            return false;
        }
        for (int i = 0; i < expected.getWidth(); i++) {
            for (int j = 0; j < expected.getHeight(); j++) {
                if (expected.getPixel(i, j) != actual.getPixel(i, j)) return false;
            }
        }
        return true;
    }

    @Test
    @SmallTest
    public void testGetIcon() throws Exception {
        Bitmap expectedIcon =
                BitmapFactory.decodeFile(UrlUtils.getIsolatedTestFilePath(TEST_IMAGE));
        String testImageUrl = mTestServer.getURL(TEST_IMAGE);

        final Semaphore semaphore = new Semaphore(0);
        // Use an AtomicReference and assert on the Instrumentation thread so that failures show
        // up as proper failures instead of browser crashes.
        final AtomicReference<Bitmap> actualIcon = new AtomicReference<Bitmap>();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ExploreSitesBridgeExperimental.getIcon(
                                mProfile, testImageUrl, new Callback<Bitmap>() {
                                    @Override
                                    public void onResult(Bitmap icon) {
                                        actualIcon.set(icon);
                                        semaphore.release();
                                    }
                                }));
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertNotNull(actualIcon.get());
        Assert.assertTrue(bitmapsEqual(expectedIcon, actualIcon.get()));
    }
}
