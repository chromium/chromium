// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.Uri;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.function.Supplier;

/** Unit tests for {@link ScreenshotUriProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ScreenshotUriProviderUnitTest {
    private static final String TARGET_PACKAGE = "com.example.app";

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    @Mock private Context mContext;
    private Supplier<@Nullable Tab> mTabSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabSupplier = () -> mTab;
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(JUnitTestGURLs.URL_1).when(mTab).getUrl();
        when(mRenderCoordinates.getScrollXPixInt()).thenReturn(100);
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(200);

        ContextUtils.initApplicationContextForTests(mContext);
        when(mContext.getPackageName()).thenReturn("org.chromium.chrome.test");
    }

    @After
    public void tearDown() {
        ScreenshotUriProvider.clearCachedContent(null);
    }

    @Test
    public void testGetScreenshotUri_Success() {
        Uri uri = ScreenshotUriProvider.getScreenshotUriForCurrentTab(mTabSupplier, TARGET_PACKAGE);
        assertNotNull(uri);
        verify(mContext).grantUriPermission(eq(TARGET_PACKAGE), eq(uri), anyInt());
    }

    @Test
    public void testGetScreenshotUri_Reuse() {
        Uri uri1 =
                ScreenshotUriProvider.getScreenshotUriForCurrentTab(mTabSupplier, TARGET_PACKAGE);
        assertNotNull(uri1);

        Uri uri2 =
                ScreenshotUriProvider.getScreenshotUriForCurrentTab(mTabSupplier, TARGET_PACKAGE);
        assertSame(uri1, uri2);

        verify(mContext, times(1)).grantUriPermission(any(), any(), anyInt());
    }

    @Test
    public void testGetScreenshotUri_NewStateOnUrlChange() {
        Uri uri1 =
                ScreenshotUriProvider.getScreenshotUriForCurrentTab(mTabSupplier, TARGET_PACKAGE);

        doReturn(JUnitTestGURLs.URL_2).when(mTab).getUrl();
        Uri uri2 =
                ScreenshotUriProvider.getScreenshotUriForCurrentTab(mTabSupplier, TARGET_PACKAGE);

        assertNotNull(uri2);
        assertNotNull(uri1);
        assertEquals(false, uri1.equals(uri2));
        verify(mContext, times(2)).grantUriPermission(eq(TARGET_PACKAGE), any(), anyInt());
    }

    @Test
    public void testGetInvocationState() {
        Uri uri = ScreenshotUriProvider.getScreenshotUriForCurrentTab(mTabSupplier, TARGET_PACKAGE);
        String invocationId = uri.getLastPathSegment();

        ScreenshotInvocationState state = ScreenshotUriProvider.getInvocationState(invocationId);
        assertNotNull(state);
        assertEquals(invocationId, state.getInvocationId());
    }

    @Test
    public void testGetInvocationState_NotFound() {
        assertNull(ScreenshotUriProvider.getInvocationState("non-existent"));
    }

    @Test
    public void testClearCachedContent() {
        Uri uri = ScreenshotUriProvider.getScreenshotUriForCurrentTab(mTabSupplier, TARGET_PACKAGE);
        String invocationId = uri.getLastPathSegment();

        ScreenshotUriProvider.clearCachedContent(invocationId);

        assertNull(ScreenshotUriProvider.getInvocationState(invocationId));
        verify(mContext).revokeUriPermission(eq(uri), anyInt());
    }
}
