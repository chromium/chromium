// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shape_detection;

import android.os.StrictMode;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 *  Testing of the Shape Detection API. This API has three parts: QR/Barcodes,
 *  Text and Faces. Only the first two are tested here since Face detection
 *  is based on android.media.FaceDetector and doesn't need special treatment,
 *  hence is tested via content_browsertests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShapeDetectionTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String BARCODE_TEST_EXPECTED_TAB_TITLE = "https://chromium.org";
    private static final String TEXT_TEST_EXPECTED_TAB_TITLE =
            "The quick brown fox jumped over the lazy dog. Helvetica Neue 36.";
    private StrictMode.ThreadPolicy mOldPolicy;

    /**
     * Verifies that QR codes are detected correctly.
     */
    @Test
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @Feature({"ShapeDetection"})
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testBarcodeDetection() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        try {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            TabTitleObserver titleObserver =
                    new TabTitleObserver(tab, BARCODE_TEST_EXPECTED_TAB_TITLE);
            mActivityTestRule.loadUrl(
                    testServer.getURL("/chrome/test/data/android/barcode_detection.html"));
            titleObserver.waitForTitleUpdate(10);

            Assert.assertEquals(BARCODE_TEST_EXPECTED_TAB_TITLE, tab.getTitle());
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * Verifies that text is detected correctly.
     */
    @Test
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @Feature({"ShapeDetection"})
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testTextDetection() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        try {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            TabTitleObserver titleObserver =
                    new TabTitleObserver(tab, TEXT_TEST_EXPECTED_TAB_TITLE);
            mActivityTestRule.loadUrl(
                    testServer.getURL("/chrome/test/data/android/text_detection.html"));
            titleObserver.waitForTitleUpdate(10);

            Assert.assertEquals(TEXT_TEST_EXPECTED_TAB_TITLE, tab.getTitle());
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * We need to allow a looser policy due to the Google Play Services internals.
     */
    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mOldPolicy = StrictMode.allowThreadDiskWrites(); });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> StrictMode.setThreadPolicy(mOldPolicy));
    }
}
