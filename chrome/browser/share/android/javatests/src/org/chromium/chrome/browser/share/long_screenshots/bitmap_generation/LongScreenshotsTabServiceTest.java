// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Rect;

import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.paint_preview.common.proto.PaintPreview.PaintPreviewProto;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for the Paint Preview Tab Manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LongScreenshotsTabServiceTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private Tab mTab;
    private LongScreenshotsTabService mLongScreenshotsTabService;
    private TestCaptureProcessor mProcessor;

    class TestCaptureProcessor implements LongScreenshotsTabService.CaptureProcessor {
        private PaintPreviewProto mActualReponse;
        @Status
        private int mActualStatus;
        private boolean mProcessCapturedTabCalled;

        public PaintPreviewProto getResponse() {
            return mActualReponse;
        }

        public @Status int getStatus() {
            return mActualStatus;
        }

        public boolean getProcessCapturedTabCalled() {
            return mProcessCapturedTabCalled;
        }

        @Override
        public void processCapturedTab(PaintPreviewProto response, @Status int status) {
            mProcessCapturedTabCalled = true;
            mActualReponse = response;
            mActualStatus = status;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mProcessor = new TestCaptureProcessor();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLongScreenshotsTabService = LongScreenshotsTabServiceFactory.getServiceInstance();
            mLongScreenshotsTabService.setCaptureProcessor(mProcessor);
        });
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
            mLongScreenshotsTabService.captureTab(mTab, new Rect(0, 0, 100, 100));
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Callback was not called", mProcessor.getProcessCapturedTabCalled(),
                    Matchers.is(true));
        });

        Assert.assertEquals(Status.OK, mProcessor.getStatus());
        Assert.assertNotNull(mProcessor.getResponse());
        Assert.assertFalse(mProcessor.getResponse().getRootFrame().getFilePath().isEmpty());
    }
}
