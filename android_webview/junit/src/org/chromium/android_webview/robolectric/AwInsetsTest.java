// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.android_webview.AwDisplayCutoutController;
import org.chromium.android_webview.AwViewAndroidDelegate;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Tests for the inset code in AwViewAndroidDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
public class AwInsetsTest {
    @Test
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
    @Feature({"AndroidWebView"})
    public void webViewIsBelowWindowBottomNoIme() {
        Rect windowBounds = new Rect(20, 50, 520, 850);
        // WebView is 200 below the bottom of the Window.
        Rect webViewBounds = new Rect(0, 400, 0, 1000);
        WindowInsets insets = new WindowInsets.Builder().build();
        runInsetTest(windowBounds, webViewBounds, insets, 200);
    }

    @Test
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
        Context context = ApplicationProvider.getApplicationContext();
        ViewGroup view = new FrameLayout(context);
        // Attach the view to a window so that it is attached and has a location in the window.
        FrameLayout windowRoot = new FrameLayout(context);
        windowRoot.addView(view);
        context.getSystemService(WindowManager.class)
                .addView(windowRoot, new WindowManager.LayoutParams());
        ShadowLooper.idleMainLooper();
        view.layout(
                webViewBounds.left, webViewBounds.top, webViewBounds.right, webViewBounds.bottom);

        WindowMetrics wm = mock(WindowMetrics.class);
        doReturn(insets).when(wm).getWindowInsets();
        doReturn(windowBounds).when(wm).getBounds();
        WindowManager manager = mock(WindowManager.class);
        doReturn(wm).when(manager).getCurrentWindowMetrics();
        // Set after constructing the View so that View construction uses the real WindowManager.
        ShadowApplication shadowApplication = Shadow.extract(context);
        shadowApplication.setSystemService(Context.WINDOW_SERVICE, manager);

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
        // Dispatches to the OnApplyWindowInsetsListener registered by AwDisplayCutoutController.
        view.dispatchApplyWindowInsets(insets);
        Assert.assertEquals(expected, viewAndroidDelegate.getViewportInsetBottom());
    }
}
