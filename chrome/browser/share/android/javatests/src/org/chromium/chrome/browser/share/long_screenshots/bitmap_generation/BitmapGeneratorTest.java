// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.paint_preview.PaintPreviewCompositorUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for the LongScreenshotsEntryTest. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BitmapGeneratorTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private Tab mTab;
    private BitmapGenerator mGenerator;
    private boolean mBitmapCreated;

    @Before
    public void setUp() throws Exception {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityWithURL(url);
        mTab = mActivityTestRule.getActivity().getActivityTab();
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mGenerator != null) {
                        mGenerator.destroy();
                    }
                });
    }

    /** Verifies that a Tab's contents are captured and rastered. */
    @Test
    @LargeTest
    @Feature({"LongScreenshots"})
    public void testCapturedNewOne() throws Exception {
        Runnable onErrorCallback =
                new Runnable() {
                    @Override
                    public void run() {
                        Assert.fail("Error should not be thrown");
                    }
                };

        Callback<Bitmap> onBitmapGenerated =
                new Callback<Bitmap>() {
                    @Override
                    public void onResult(Bitmap result) {
                        Assert.assertNotNull(result);
                        mBitmapCreated = true;
                    }
                };

        class Listener implements BitmapGenerator.GeneratorCallBack {
            @Override
            public void onCompositorResult(@CompositorStatus int status) {
                Assert.assertEquals(CompositorStatus.OK, status);
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            mGenerator.compositeBitmap(
                                    new Rect(0, 0, 100, 100), onErrorCallback, onBitmapGenerated);
                        });
            }

            @Override
            public void onCaptureResult(@Status int status) {
                Assert.assertEquals(Status.OK, status);
            }
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mGenerator =
                            new BitmapGenerator(
                                    mTab,
                                    new ScreenshotBoundsManager(
                                            mActivityTestRule.getActivity(), mTab),
                                    new Listener());
                    PaintPreviewCompositorUtils.warmupCompositor();
                    mGenerator.captureTab(/* inMemory= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mGenerator.getContentSize(), Matchers.notNullValue());
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mGenerator.getScrollOffset(), Matchers.notNullValue());
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mBitmapCreated, Matchers.equalTo(true));
                });
    }
}
