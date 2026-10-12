// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.back_press.BackPressMetrics.CaptureNativeViewResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.dynamics.CaptureResult;
import org.chromium.url.GURL;

/** Unit tests for {@link NativePageBitmapCapturer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NativePageBitmapCapturerUnitTest {
    private static final String RESULT_HISTOGRAM =
            "Android.PredictiveNavigationTransition.CaptureNativeViewResult";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getLastCommittedUrl()).thenReturn(new GURL(UrlConstants.NTP_URL));
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
    }

    @Test
    public void testNullWebContents() {
        when(mTab.getWebContents()).thenReturn(null);

        assertFalse(
                NativePageBitmapCapturer.maybeCaptureNativeView(
                        mTab, result -> {}, CaptureResult.Destination.BITMAP));
        assertNull(
                NativePageBitmapCapturer.maybeCaptureNativeViewSync(
                        mTab, /* topControlsHeight= */ 0));
    }

    @Test
    public void testNullWindowAndroid() {
        when(mTab.getWindowAndroid()).thenReturn(null);
        assertFallbackUx(CaptureNativeViewResult.NULL_WINDOW_ANDROID);
    }

    @Test
    public void testNullView() {
        when(mTab.getView()).thenReturn(null);
        assertFallbackUx(CaptureNativeViewResult.VIEW_NOT_LAID_OUT);
    }

    @Test
    public void testUnlaidOutView() {
        View view = new View(ApplicationProvider.getApplicationContext());
        when(mTab.getView()).thenReturn(view);
        assertFallbackUx(CaptureNativeViewResult.VIEW_NOT_LAID_OUT);
    }

    private void assertFallbackUx(@CaptureNativeViewResult int expectedResult) {
        CallbackHelper callbackHelper = new CallbackHelper();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(RESULT_HISTOGRAM, expectedResult)) {
            assertTrue(
                    NativePageBitmapCapturer.maybeCaptureNativeView(
                            mTab,
                            result -> {
                                assertNull(result);
                                callbackHelper.notifyCalled();
                            },
                            CaptureResult.Destination.BITMAP));
        }
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(1, callbackHelper.getCallCount());

        assertNull(
                NativePageBitmapCapturer.maybeCaptureNativeViewSync(
                        mTab, /* topControlsHeight= */ 0));
    }
}
