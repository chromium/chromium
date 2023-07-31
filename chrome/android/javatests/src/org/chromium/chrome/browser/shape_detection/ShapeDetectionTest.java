// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shape_detection;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
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
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String BARCODE_TEST_EXPECTED_TAB_TITLE = "https://chromium.org";
    private static final String TEXT_TEST_EXPECTED_TAB_TITLE =
            "The quick brown fox jumped over the lazy dog. Helvetica Neue 36.";

    /**
     * Verifies that QR codes are detected correctly.
     */
    @Test
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @Feature({"ShapeDetection"})
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    @DisabledTest(message = "https://crbug.com/1139470")
    public void testBarcodeDetection() throws TimeoutException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, BARCODE_TEST_EXPECTED_TAB_TITLE);
        mActivityTestRule.loadUrl(
                testServer.getURL("/chrome/test/data/android/barcode_detection.html"));
        titleObserver.waitForTitleUpdate(10);
        Assert.assertEquals(BARCODE_TEST_EXPECTED_TAB_TITLE, tab.getTitle());
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
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, TEXT_TEST_EXPECTED_TAB_TITLE);
        mActivityTestRule.loadUrl(
                testServer.getURL("/chrome/test/data/android/text_detection.html"));
        titleObserver.waitForTitleUpdate(10);
        Assert.assertEquals(TEXT_TEST_EXPECTED_TAB_TITLE, ChromeTabUtils.getTitleOnUiThread(tab));
    }

    /**
     * We need to allow a looser policy due to the Google Play Services internals.
     */
    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }
}
