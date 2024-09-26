// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.graphics.Rect;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for the Long Screenshot Tab Service Test. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LongScreenshotsTabServiceJUnitTest {
    public static final long FAKE_NATIVE_ADDR = 345L;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Tab mTab;
    private LongScreenshotsTabService mLongScreenshotsTabService;
    @Mock private LongScreenshotsTabService.Natives mLongScreenshotsTabServiceJniMock;
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
    public void setUp() {
        initMocks(this);
        when(mTab.getWebContents()).thenReturn(null);
        mJniMocker.mock(LongScreenshotsTabServiceJni.TEST_HOOKS, mLongScreenshotsTabServiceJniMock);
        mProcessor = new TestCaptureProcessor();
        mLongScreenshotsTabService = new LongScreenshotsTabService(FAKE_NATIVE_ADDR);
        mLongScreenshotsTabService.setCaptureProcessor(mProcessor);
        assertEquals(FAKE_NATIVE_ADDR, mLongScreenshotsTabService.getNativeBaseService());
    }

    @After
    public void tearDown() {
        mLongScreenshotsTabService.onNativeDestroyed();
        assertEquals(0, mLongScreenshotsTabService.getNativeBaseService());
    }

    /** Verifies that an error status is propagated. */
    @Test
    public void testCapturedTabHasErrorStatus() {
        mLongScreenshotsTabService.processCaptureTabStatus(Status.CAPTURE_FAILED);
        assertTrue(mProcessor.getProcessCapturedTabCalled());
        assertEquals(Status.CAPTURE_FAILED, mProcessor.getStatus());
        assertEquals(0, mProcessor.getNativeCaptureResultPtr());
    }

    /** Verifies that a response won't crash if there is no processor. */
    @Test
    public void testCapturedNoProcessor() {
        final long fakeAddr = 123L;
        mLongScreenshotsTabService.setCaptureProcessor(null);
        mLongScreenshotsTabService.processPaintPreviewResponse(fakeAddr);
        assertFalse(mProcessor.getProcessCapturedTabCalled());

        verify(mLongScreenshotsTabServiceJniMock, times(1)).releaseCaptureResultPtr(eq(fakeAddr));
    }

    /**
     * Verifies that a capture won't be attempted if native is destroyed or the web contents is
     * gone.
     */
    @Test
    public void testCaptureTabError() {
        mLongScreenshotsTabService.captureTab(mTab, new Rect(), false);
        assertTrue(mProcessor.getProcessCapturedTabCalled());
        assertEquals(Status.WEB_CONTENTS_GONE, mProcessor.getStatus());
        assertEquals(0, mProcessor.getNativeCaptureResultPtr());

        mLongScreenshotsTabService.onNativeDestroyed();
        mProcessor = new TestCaptureProcessor();
        mLongScreenshotsTabService.setCaptureProcessor(mProcessor);
        mLongScreenshotsTabService.captureTab(mTab, new Rect(), false);

        assertEquals(0, mLongScreenshotsTabService.getNativeBaseService());
        assertTrue(mProcessor.getProcessCapturedTabCalled());
        assertEquals(Status.NATIVE_SERVICE_NOT_INITIALIZED, mProcessor.getStatus());
        assertEquals(0, mProcessor.getNativeCaptureResultPtr());

        mLongScreenshotsTabService.longScreenshotsClosed();
        verify(mLongScreenshotsTabServiceJniMock, never()).longScreenshotsClosedAndroid(anyInt());
    }
}
