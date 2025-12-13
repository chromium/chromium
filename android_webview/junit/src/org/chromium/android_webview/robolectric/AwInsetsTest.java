// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwDisplayCutoutController;
import org.chromium.android_webview.AwViewAndroidDelegate;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Tests for the inset code in AwViewAndroidDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
public class AwInsetsTest {
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void webViewIsAlignedWithBottomOfWindow() {
        Rect windowBounds = new Rect(20, 50, 520, 850);
        Rect webViewBounds = new Rect(40, 100, 40, 800);
        WindowInsets insets =
                new WindowInsets.Builder()
                        .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, 20))
                        .build();
        runInsetTest(windowBounds, webViewBounds, insets, 20);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void webViewIsAboveWindowBottomNoImeOverlap() {
        Rect windowBounds = new Rect(20, 50, 520, 850);
        Rect webViewBounds = new Rect(0, 0, 0, 400);
        WindowInsets insets =
                new WindowInsets.Builder()
                        .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, 400))
                        .build();
        runInsetTest(windowBounds, webViewBounds, insets, 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void webViewIsAboveWindowBottomImeGap() {
        Rect windowBounds = new Rect(20, 50, 520, 850);
        Rect webViewBounds = new Rect(0, 0, 0, 300);
        WindowInsets insets =
                new WindowInsets.Builder()
                        .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, 400))
                        .build();
        runInsetTest(windowBounds, webViewBounds, insets, 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void webViewIsAboveWindowBottomImeOverlap() {
        Rect windowBounds = new Rect(20, 50, 520, 850);
        Rect webViewBounds = new Rect(0, 0, 0, 723);
        WindowInsets insets =
                new WindowInsets.Builder()
                        .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, 321))
                        .build();
        // Expected: 321 - ((850 - 50) - 723)
        runInsetTest(windowBounds, webViewBounds, insets, 244);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void webViewIsBelowWindowBottomNoIme() {
        Rect windowBounds = new Rect(20, 50, 520, 850);
        // WebView is 200 below the bottom of the Window.
        Rect webViewBounds = new Rect(0, 400, 0, 1000);
        WindowInsets insets = new WindowInsets.Builder().build();
        runInsetTest(windowBounds, webViewBounds, insets, 200);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void webViewIsBelowWindowBottomIme() {
        Rect windowBounds = new Rect(20, 50, 520, 850);
        // WebView is 200 below the bottom of the Window.
        Rect webViewBounds = new Rect(0, 400, 0, 1000);
        WindowInsets insets =
                new WindowInsets.Builder()
                        .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, 100))
                        .build();
        runInsetTest(windowBounds, webViewBounds, insets, 300);
    }

    private static class InsetsListenerHolder {
        public View.OnApplyWindowInsetsListener listener;
    }

    /**
     * Runs an inset test with the following parameters.
     *
     * @param windowBounds A rectangle representing the size and position of the enclosing Window.
     * @param webViewBounds A rectangle representing the size and position of the WebView relative
     *     to the parent Window.
     * @param insets The insets that should be passed to onApplyWindowInsets before calculating the
     *     bottom inset of the web contents.
     */
    private void runInsetTest(
            Rect windowBounds, Rect webViewBounds, WindowInsets insets, int expected) {
        final InsetsListenerHolder holder = new InsetsListenerHolder();
        WindowMetrics wm = mock(WindowMetrics.class);
        doReturn(insets).when(wm).getWindowInsets();
        doReturn(windowBounds).when(wm).getBounds();
        ViewGroup view = mock(ViewGroup.class);
        Context context = mock(Context.class);
        WindowManager manager = mock(WindowManager.class);
        doReturn(wm).when(manager).getCurrentWindowMetrics();
        doReturn(manager).when(context).getSystemService(WindowManager.class);
        doAnswer(
                        invocation -> {
                            holder.listener =
                                    (View.OnApplyWindowInsetsListener) invocation.getArguments()[0];
                            return null;
                        })
                .when(view)
                .setOnApplyWindowInsetsListener(any(View.OnApplyWindowInsetsListener.class));
        doReturn(context).when(view).getContext();
        doReturn(webViewBounds.width()).when(view).getWidth();
        doReturn(webViewBounds.height()).when(view).getHeight();
        doAnswer(
                        invocation -> {
                            int[] out = (int[]) invocation.getArguments()[0];
                            out[0] = webViewBounds.left;
                            out[1] = webViewBounds.top;
                            return null;
                        })
                .when(view)
                .getLocationInWindow(any(int[].class));
        doReturn(true).when(view).isAttachedToWindow();
        doReturn(mock(ViewTreeObserver.class)).when(view).getViewTreeObserver();
        AwDisplayCutoutController awDisplayCutoutController =
                new AwDisplayCutoutController(
                        new AwDisplayCutoutController.Delegate() {
                            @Override
                            public float getDipScale() {
                                return 1.0f;
                            }

                            @Override
                            public void setDisplayCutoutSafeArea(
                                    androidx.core.graphics.Insets insets) {}

                            @Override
                            public void bottomImeInsetChanged() {}
                        },
                        view);
        AwViewAndroidDelegate viewAndroidDelegate =
                new AwViewAndroidDelegate(view, null, null, awDisplayCutoutController);
        Assert.assertNotNull(holder.listener);
        holder.listener.onApplyWindowInsets(view, insets);
        Assert.assertEquals(expected, viewAndroidDelegate.getViewportInsetBottom());
    }
}
