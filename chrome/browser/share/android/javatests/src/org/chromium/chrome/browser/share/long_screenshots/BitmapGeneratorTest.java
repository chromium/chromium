// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.paint_preview.PaintPreviewCompositorUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for the LongScreenshotsEntryTest. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BitmapGeneratorTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private Tab mTab;
    private BitmapGenerator mGenerator;
    private TestListener mTestListener;
    private Bitmap mGeneratedBitmap;

    class TestListener implements BitmapGenerator.GeneratorCallBack {
        boolean mOnBitmapGeneratedCalled;

        @Override
        public void onCompositorError(@CompositorStatus int status) {}

        @Override
        public void onBitmapGenerated(Bitmap bitmap) {
            mOnBitmapGeneratedCalled = true;
            mGeneratedBitmap = bitmap;
        }

        @Override
        public void onCaptureError(@Status int status) {}

        boolean getOnBitmapGeneratedCalled() {
            return mOnBitmapGeneratedCalled;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mTestListener = new TestListener();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mGenerator = new BitmapGenerator(
                    mActivityTestRule.getActivity(), mTab, new Rect(0, 0, 100, 100), mTestListener);
            PaintPreviewCompositorUtils.warmupCompositor();
        });
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mGenerator.destroy(); });
    }

    /**
     * Verifies that a Tab's contents are captured.
     */
    @Test
    @MediumTest
    @Feature({"LongScreenshots"})
    public void testCaptured() throws Exception {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTab.loadUrl(new LoadUrlParams(url));
            mGenerator.captureScreenshot();
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Callback was not called",
                    mTestListener.getOnBitmapGeneratedCalled(), Matchers.is(true));
        }, 10000L, 50L);

        Assert.assertNotNull(mGeneratedBitmap);
    }
}
