// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

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
import org.chromium.chrome.browser.feedback.ScreenshotSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

import java.io.FileNotFoundException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
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
    public void testOpenFileThrowsExceptionForInvalidMode() {
        assertThrows(FileNotFoundException.class, () -> mProvider.openFile(mUri, "w"));
    }

    @Test
    public void testOpenFileThrowsExceptionForInvalidUri() {
        CompletableFuture<ParcelFileDescriptor> future = openFileAsync(Uri.EMPTY, "r");
        Exception exception = assertThrows(Exception.class, () -> getAsync(future));
        assertTrue(exception.getMessage().contains("Invalid URI"));
    }

    @Test
    public void testOpenFileThrowsExceptionForInvalidInvocationId() {
        Uri invalidUri = Uri.parse("content://org.chromium.chrome/invalid_id");
        CompletableFuture<ParcelFileDescriptor> future = openFileAsync(invalidUri, "r");
        Exception exception = assertThrows(Exception.class, () -> getAsync(future));
        assertTrue(exception.getMessage().contains("Invalid or expired invocation ID"));
    }

    private CompletableFuture<ParcelFileDescriptor> openFileAsync(Uri uri, String mode) {
        return CompletableFuture.supplyAsync(
                () -> {
                    try {
                        return mProvider.openFile(uri, mode);
                    } catch (FileNotFoundException e) {
                        throw new RuntimeException(e);
                    }
                });
    }

    private <T> T getAsync(CompletableFuture<T> future) throws Exception {
        return future.get(5, TimeUnit.SECONDS);
    }
}
