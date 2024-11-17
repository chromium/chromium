// Copyright 2020 The Chromium Authors
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Tests for the Paint Preview Tab Manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LongScreenshotsTabServiceTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private Tab mTab;
    private LongScreenshotsTabService mLongScreenshotsTabService;
    private TestCaptureProcessor mProcessor;

    static class TestCaptureProcessor implements LongScreenshotsTabService.CaptureProcessor {
        @Status private int mActualStatus;
        private boolean mProcessCapturedTabCalled;
        private long mNativeCaptureResultPtr;

        public @Status int getStatus() {
            return mActualStatus;
        }

        public boolean getProcessCapturedTabCalled() {
            return mProcessCapturedTabCalled;
        }

        public long getNativeCaptureResultPtr() {
            return mNativeCaptureResultPtr;
        }

        @Override
        public void processCapturedTab(long nativeCaptureResultPtr, @Status int status) {
            mProcessCapturedTabCalled = true;
            mNativeCaptureResultPtr = nativeCaptureResultPtr;
            mActualStatus = status;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/about.html"));
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mProcessor = new TestCaptureProcessor();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLongScreenshotsTabService =
                            LongScreenshotsTabServiceFactory.getServiceInstance();
                    mLongScreenshotsTabService.setCaptureProcessor(mProcessor);
                });
    }

    /** Verifies that a Tab's contents are captured. */
    @Test
    @MediumTest
    @Feature({"LongScreenshots"})
    public void testCapturedFilesystem() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLongScreenshotsTabService.captureTab(
                            mTab, new Rect(0, 0, 100, 100), /* inMemory= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Callback was not called",
                            mProcessor.getProcessCapturedTabCalled(),
                            Matchers.is(true));
                });

        Assert.assertEquals(Status.OK, mProcessor.getStatus());
        Assert.assertNotEquals(0, mProcessor.getNativeCaptureResultPtr());
        mLongScreenshotsTabService.releaseNativeCaptureResultPtr(
                mProcessor.getNativeCaptureResultPtr());
        mLongScreenshotsTabService.longScreenshotsClosed();
    }

    /** Verifies that a Tab's contents are captured in-memory. */
    @Test
    @MediumTest
    @Feature({"LongScreenshots"})
    public void testCapturedMemory() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLongScreenshotsTabService.captureTab(
                            mTab, new Rect(0, 0, 100, 100), /* inMemory= */ true);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Callback was not called",
                            mProcessor.getProcessCapturedTabCalled(),
                            Matchers.is(true));
                });

        Assert.assertEquals(Status.OK, mProcessor.getStatus());
        Assert.assertNotEquals(0, mProcessor.getNativeCaptureResultPtr());
        mLongScreenshotsTabService.releaseNativeCaptureResultPtr(
                mProcessor.getNativeCaptureResultPtr());
        mLongScreenshotsTabService.longScreenshotsClosed();
    }
}
