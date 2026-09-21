// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.graphics.Rect;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

/** Unit tests for {@link AutoPictureInPictureTabHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutoPictureInPictureTabHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PictureInPictureBoundsCacheBridge.Natives mMockNatives;

    private WebContents mWebContents;
    private AutoPictureInPictureTabHelper mHelper;
    private WindowAndroid mWindow;
    private DisplayAndroid mDisplay;

    @Before
    public void setUp() {
        PictureInPictureBoundsCacheBridgeJni.setInstanceForTesting(mMockNatives);

        mWebContents =
                mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));
        mHelper = new AutoPictureInPictureTabHelper(mWebContents);

        mWindow = mock(WindowAndroid.class);
        mDisplay = mock(DisplayAndroid.class);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        when(mWindow.getDisplay()).thenReturn(mDisplay);
        when(mDisplay.getDisplayId()).thenReturn(1);
        when(mDisplay.getDipScale()).thenReturn(1.0f);

        // Mock getOrSetUserData to return our helper instance.
        when(mWebContents.getOrSetUserData(eq(AutoPictureInPictureTabHelper.USER_DATA_KEY), any()))
                .thenReturn(mHelper);
    }

    @Test
    public void testGetCachedWindowBounds_CallsBridge() {
        Rect requested = new Rect(0, 0, 100, 100);
        Rect cached = new Rect(0, 0, 200, 200);

        int[] coords = new int[] {cached.left, cached.top, cached.right, cached.bottom};
        when(mMockNatives.getBoundsForNewWindow(mWebContents, 1, 100, 100)).thenReturn(coords);

        Rect result = mHelper.getCachedWindowBounds(requested);

        assertEquals(cached, result);
        verify(mMockNatives).getBoundsForNewWindow(mWebContents, 1, 100, 100);
    }

    @Test
    public void testGetCachedWindowBounds_NullRequested() {
        Rect cached = new Rect(0, 0, 200, 200);

        int[] coords = new int[] {cached.left, cached.top, cached.right, cached.bottom};
        when(mMockNatives.getBoundsForNewWindow(mWebContents, 1, -1, -1)).thenReturn(coords);

        Rect result = mHelper.getCachedWindowBounds(null);

        assertEquals(cached, result);
        verify(mMockNatives).getBoundsForNewWindow(mWebContents, 1, -1, -1);
    }

    @Test
    public void testGetCachedWindowBounds_ReturnsNull() {
        when(mMockNatives.getBoundsForNewWindow(mWebContents, 1, 100, 100)).thenReturn(null);

        Rect result = mHelper.getCachedWindowBounds(new Rect(0, 0, 100, 100));

        assertNull(result);
    }
}
