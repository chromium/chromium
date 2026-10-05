// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.PixelFormat;
import android.view.ContextThemeWrapper;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

import androidx.core.view.WindowInsetsCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;

import java.lang.ref.WeakReference;

/** Unit tests for {@link CompositorView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CompositorViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutRenderHost mLayoutRenderHost;
    @Mock private CompositorSurfaceManager mCompositorSurfaceManager;
    @Mock private CompositorView.Natives mCompositorViewJni;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabContentManager mTabContentManager;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock private InsetObserver mInsetObserver;

    private CompositorView mCompositorView;

    @Before
    public void setUp() {
        when(mCompositorViewJni.init(any(), any(), any())).thenReturn(1L);
        CompositorViewJni.setInstanceForTesting(mCompositorViewJni);
        lenient().when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardDelegate);
        lenient().when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(null));
        lenient().when(mWindowAndroid.getInsetObserver()).thenReturn(mInsetObserver);
        lenient()
                .when(mInsetObserver.getLastRawWindowInsets())
                .thenReturn(new WindowInsetsCompat.Builder().build());

        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mCompositorView = new CompositorView(context, mLayoutRenderHost);
        mCompositorView.setCompositorSurfaceManagerForTesting(mCompositorSurfaceManager);
        mCompositorView.initNativeCompositor(mWindowAndroid, mTabContentManager);
        mCompositorView.setRootView(new FrameLayout(context));
    }

    private void measure(int width, int height) {
        mCompositorView.measure(
                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
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

    @Test
    @EnableFeatures(ChromeFeatureList.COMPOSITOR_VIEW_SHRINK_WHEN_KEYBOARD_HIDDEN)
    public void testOnMeasure_shrinksWhenKeyboardHidden() {
        when(mKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);

        // Warm-up measure because mPreviousWindowTop starts at -1.
        measure(1080, 1704);
        mCompositorView.forceLayout();

        // The layout expands while the keyboard is showing (e.g. the bottom inset is dropped).
        when(mKeyboardDelegate.isKeyboardShowing(any())).thenReturn(true);
        measure(1080, 1848);
        assertEquals(1848, mCompositorView.getMeasuredHeight());

        // Shrinks back down when requested once the keyboard is hidden.
        when(mKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);
        mCompositorView.forceLayout();
        measure(1080, 1704);
        assertEquals(1704, mCompositorView.getMeasuredHeight());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.COMPOSITOR_VIEW_SHRINK_WHEN_KEYBOARD_HIDDEN)
    public void testOnMeasure_retainsHeightWhileKeyboardShowing() {
        when(mKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);

        // Warm-up measure.
        measure(1080, 1704);

        // Keyboard shows; height should be retained (legacy optimization).
        when(mKeyboardDelegate.isKeyboardShowing(any())).thenReturn(true);
        mCompositorView.forceLayout();
        measure(1080, 1000);
        assertEquals(1704, mCompositorView.getMeasuredHeight());

        // Layout grows while the keyboard is up (e.g. the bottom inset is dropped); a larger spec
        // always wins.
        mCompositorView.forceLayout();
        measure(1080, 1848);
        assertEquals(1848, mCompositorView.getMeasuredHeight());

        // Keyboard hides; height shrinks back to requested size from the larger retained height.
        when(mKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);
        mCompositorView.forceLayout();
        measure(1080, 1704);
        assertEquals(1704, mCompositorView.getMeasuredHeight());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.COMPOSITOR_VIEW_SHRINK_WHEN_KEYBOARD_HIDDEN)
    public void testOnMeasure_legacyRetainsHeightWhenFlagDisabled() {
        when(mKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);

        // Warm-up measure.
        measure(1080, 1704);
        mCompositorView.forceLayout();

        // Expands.
        measure(1080, 1848);
        assertEquals(1848, mCompositorView.getMeasuredHeight());

        // When flag is disabled, legacy behavior retains the larger height even with keyboard
        // hidden.
        mCompositorView.forceLayout();
        measure(1080, 1704);
        assertEquals(1848, mCompositorView.getMeasuredHeight());
    }
}
