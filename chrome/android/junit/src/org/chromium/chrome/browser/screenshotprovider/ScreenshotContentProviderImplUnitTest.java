// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.net.Uri;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.feedback.ScreenshotSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

import java.util.function.Supplier;

/** Unit tests for {@link ScreenshotContentProviderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowContentResolver.class})
public class ScreenshotContentProviderImplUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContentsImpl mWebContents;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ScreenshotSource mMockScreenshotSource;
    @Mock private Activity mActivity;
    @Mock private Supplier<Tab> mTabSupplier;

    private ScreenshotContentProviderImpl mProvider;
    private Uri mUri;

    @Before
    public void setUp() {
        mProvider =
                new ScreenshotContentProviderImpl() {
                    @Override
                    protected ScreenshotSource createScreenshotSource(Activity activity) {
                        return mMockScreenshotSource;
                    }
                };
        ContextUtils.initApplicationContextForTests(
                androidx.test.core.app.ApplicationProvider.getApplicationContext());
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getContext()).thenReturn(mActivity);
        when(mRenderCoordinates.getScrollXPixInt()).thenReturn(0);
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(0);

        // Default mock behavior for ScreenshotSource: simulate success with a small bitmap
        doAnswer(
                        invocation -> {
                            Runnable callback = invocation.getArgument(0);
                            Bitmap screenshotBitmap =
                                    Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
                            when(mMockScreenshotSource.getScreenshot())
                                    .thenReturn(screenshotBitmap);
                            if (callback != null) {
                                callback.run();
                            }
                            return null;
                        })
                .when(mMockScreenshotSource)
                .capture(any());

        // Set up the URI
        when(mTabSupplier.get()).thenReturn(mTab);
        mUri = ScreenshotUriProvider.getScreenshotUriForCurrentTab(mTabSupplier, "com.test");

        ShadowLooper.idleMainLooper();
    }

    @After
    public void tearDown() {
        ShadowLooper.shadowMainLooper().runToEndOfTasks();
    }

    @Test
    public void testQueryReturnsNull() {
        assertNull(mProvider.query(Uri.EMPTY, null, null, null, null));
    }

    @Test
    public void testGetTypeReturnsImagePng() {
        assertEquals("image/png", mProvider.getType(Uri.EMPTY));
    }

    @Test
    public void testInsertReturnsNull() {
        assertNull(mProvider.insert(Uri.EMPTY, null));
    }

    @Test
    public void testDeleteReturnsZero() {
        assertEquals(0, mProvider.delete(Uri.EMPTY, null, null));
    }

    @Test
    public void testUpdateReturnsZero() {
        assertEquals(0, mProvider.update(Uri.EMPTY, null, null, null));
    }

    @Test
    public void testOpenFileLogsSuccessMetricsAndLatency() throws Exception {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_SUCCEEDED_RETURNED_CAPTURED)
                        .expectAnyRecord(
                                "Android.ScreenshotContentProvider.Latency.CreateToCaptureStart")
                        .expectAnyRecord(
                                "Android.ScreenshotContentProvider.Latency.CaptureStartToEnd")
                        .expectAnyRecord("Android.ScreenshotContentProvider.Latency.TotalLatency")
                        .build();

        mProvider.openFile(mUri, "r");
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsInvalidModeMetric() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_INVALID_MODE)
                        .build();
        try {
            mProvider.openFile(mUri, "w");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsInvalidUriMetric() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_INVALID_URI)
                        .build();
        try {
            mProvider.openFile(Uri.EMPTY, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsInvalidIdMetric() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_INVALID_ID)
                        .build();
        Uri invalidUri = Uri.parse("content://org.chromium.chrome/invalid_id");
        try {
            mProvider.openFile(invalidUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsTabNullMetric() {
        when(mTabSupplier.get()).thenReturn(null);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_TO_GET_CURRENT_TAB)
                        .build();
        try {
            mProvider.openFile(mUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsTabDestroyedMetric() {
        when(mTab.isDestroyed()).thenReturn(true);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_TO_GET_CURRENT_TAB)
                        .build();
        try {
            mProvider.openFile(mUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsUrlNullMetric() {
        when(mTab.getUrl()).thenReturn(null);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_CURRENT_TAB_NULL_URL)
                        .build();
        try {
            mProvider.openFile(mUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsUrlMismatchMetric() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_CURRENT_TAB_CHANGED)
                        .build();
        try {
            mProvider.openFile(mUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsWindowAndroidNullMetric() {
        when(mTab.getWindowAndroid()).thenReturn(null);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_INVALID_WINDOW_ANDROID)
                        .build();
        try {
            mProvider.openFile(mUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsContextNullMetric() {
        when(mTab.getContext()).thenReturn(null);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_INVALID_CONTEXT)
                        .build();
        try {
            mProvider.openFile(mUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsActivityNullMetric() {
        when(mTab.getContext())
                .thenReturn(androidx.test.core.app.ApplicationProvider.getApplicationContext());
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_INVALID_ACTIVITY)
                        .build();
        try {
            mProvider.openFile(mUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }

    @Test
    public void testOpenFileLogsCaptureFailedMetric() {
        doAnswer(
                        invocation -> {
                            Runnable callback = invocation.getArgument(0);
                            when(mMockScreenshotSource.getScreenshot()).thenReturn(null);
                            if (callback != null) {
                                callback.run();
                            }
                            return null;
                        })
                .when(mMockScreenshotSource)
                .capture(any());
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_STARTED)
                        .expectIntRecord(
                                "Android.ScreenshotContentProvider.Events",
                                ScreenshotContentProviderMetrics.ScreenshotContentProviderEvent
                                        .REQUEST_FAILED_EMPTY_BITMAP)
                        .build();
        try {
            mProvider.openFile(mUri, "r");
        } catch (Exception ignored) {
        }
        watcher.assertExpected();
    }
}
