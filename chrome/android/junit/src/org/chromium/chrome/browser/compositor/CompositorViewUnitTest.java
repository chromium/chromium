// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.PixelFormat;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link CompositorView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CompositorViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutRenderHost mLayoutRenderHost;
    @Mock private CompositorSurfaceManager mCompositorSurfaceManager;
    @Mock private CompositorView.Natives mCompositorViewJni;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabContentManager mTabContentManager;

    private CompositorView mCompositorView;

    @Before
    public void setUp() {
        when(mCompositorViewJni.init(any(), any(), any())).thenReturn(1L);
        CompositorViewJni.setInstanceForTesting(mCompositorViewJni);
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mCompositorView = new CompositorView(context, mLayoutRenderHost);
        mCompositorView.setCompositorSurfaceManagerForTesting(mCompositorSurfaceManager);
        mCompositorView.initNativeCompositor(mWindowAndroid, mTabContentManager);
    }

    @Test
    public void testSetXrFullSpaceMode() {
        // Reset the surface manager mock to ignore interactions during setup.
        reset(mCompositorSurfaceManager);

        // Initial state is false, no call to JNI.
        verify(mCompositorViewJni, never()).setOverlayXrFullScreenMode(anyLong(), anyBoolean());

        mCompositorView.setXrFullSpaceMode(true);
        verify(mCompositorViewJni).setOverlayXrFullScreenMode(1L, true);
        verify(mCompositorSurfaceManager).requestSurface(PixelFormat.TRANSLUCENT);

        mCompositorView.setXrFullSpaceMode(false);
        verify(mCompositorViewJni).setOverlayXrFullScreenMode(1L, false);
        verify(mCompositorSurfaceManager).requestSurface(PixelFormat.OPAQUE);

        // Setting the mode again should not trigger another surface request or JNI call.
        mCompositorView.setXrFullSpaceMode(false);
        verify(mCompositorSurfaceManager, times(1)).requestSurface(PixelFormat.OPAQUE);
        verify(mCompositorViewJni, times(1)).setOverlayXrFullScreenMode(1L, false);
    }
}
