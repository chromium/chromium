// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

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
        boolean mSomeCallBackCalled;
        @CompositorStatus
        int mCompositorErrorStatus;

        @Status
        int mCaptureStatus;

        @Override
        public void onCompositorError(@CompositorStatus int status) {
            mCompositorErrorStatus = status;
            mSomeCallBackCalled = true;
        }

        @Override
        public void onBitmapGenerated(Bitmap bitmap) {
            mOnBitmapGeneratedCalled = true;
            mSomeCallBackCalled = true;
            mGeneratedBitmap = bitmap;
        }

        @Override
        public void onCaptureError(@Status int status) {
            mSomeCallBackCalled = true;
            mCaptureStatus = status;
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
            Criteria.checkThat("OnBitmapGenerated callback was not called",
                    mTestListener.mOnBitmapGeneratedCalled, Matchers.is(true));
        }, 10000L, 50L);

        Assert.assertNotNull(mGeneratedBitmap);
    }

    /**
     * Verifies that a Tab's contents are captured.
     * TODO(tgupta): Figure out how to mimic a low memory situation.

    @Test
    @MediumTest
    @Feature({"LongScreenshots"})
    public void testCapturedLowMemory() throws Exception {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTab.loadUrl(new LoadUrlParams(url));
            MemoryPressureListener.notifyMemoryPressure(MemoryPressureLevel.CRITICAL);
            mGenerator.captureScreenshot();

        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("No callback on the listener was called.",
                    mTestListener.mSomeCallBackCalled, Matchers.is(true));
        }, 10000L, 50L);

        Assert.assertNull(mGeneratedBitmap);
        Assert.assertEquals(Status.LOW_MEMORY_DETECTED,
                mTestListener.mCaptureStatus);
        Assert.assertEquals(CompositorStatus.SKIPPED_DUE_TO_MEMORY_PRESSURE,
                mTestListener.mCompositorErrorStatus);
    }
  */
}
