// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Rect;
import android.os.Build;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Log;

/**
 * Display cutout controller for WebView.
 *
 * This object should be constructed in WebView's constructor to support set listener logic for
 * Android P and above.
 */
@Lifetime.WebView
@RequiresApi(Build.VERSION_CODES.P)
public class AwDisplayCutoutController {
    private static final boolean DEBUG = false;
    private static final String TAG = "DisplayCutout";

    /** This is a delegate that the embedder needs to implement. */
    public interface Delegate {
        /** @return The DIP scale. */
        float getDipScale();

        /**
         * Set display cutout safe area such that webpage can read safe-area-insets CSS properties.
         * Note that this can be called with the same parameter repeatedly, and the embedder needs
         * to check / throttle as necessary.
         *
         * @param insets A placeholder to store left, top, right, and bottom insets in regards to
         *               WebView. Note that DIP scale has been applied.
         */
        void setDisplayCutoutSafeArea(Insets insets);
    }

    /**
     * A placeholder for insets.
     *
     * android.graphics.Insets is available from Q, while we support display cutout from P and
     * above, so adding a new class.
     */
    public static final class Insets {
        public int left;
        public int top;
        public int right;
        public int bottom;

        public Insets() {}

        public Insets(int left, int top, int right, int bottom) {
            set(left, top, right, bottom);
        }

        public Rect toRect(Rect rect) {
            rect.set(left, top, right, bottom);
            return rect;
        }

        public void set(Insets insets) {
            left = insets.left;
            top = insets.top;
            right = insets.right;
            bottom = insets.bottom;
        }

        public void set(int left, int top, int right, int bottom) {
            this.left = left;
            this.top = top;
            this.right = right;
            this.bottom = bottom;
        }

        @Override
        public final boolean equals(Object o) {
            if (!(o instanceof Insets)) return false;
            Insets i = (Insets) o;
            return left == i.left && top == i.top && right == i.right && bottom == i.bottom;
        }

        @Override
        public final String toString() {
            return "Insets: (" + left + ", " + top + ")-(" + right + ", " + bottom + ")";
        }

        @Override
        public final int hashCode() {
            return 3 * left + 5 * top + 7 * right + 11 * bottom;
        }
    }

    private Delegate mDelegate;
    private View mContainerView;

    /**
     * Constructor for AwDisplayCutoutController.
     *
     * @param delegate The delegate.
     * @param containerView The container view (WebView).
     */
    public AwDisplayCutoutController(Delegate delegate, View containerView) {
        mDelegate = delegate;
        mContainerView = containerView;
        registerContainerView(containerView);
    }

    /**
     * Register a container view to listen to window insets.
     *
     * Note that you do not need to register the containerView.
     *
     * @param containerView A container View, such as fullscreen view.
     */
    public void registerContainerView(View containerView) {
        if (DEBUG) Log.i(TAG, "registerContainerView");
        // For Android P~R, we set the listener in WebView's constructor.
        // Once we set the listener, we will no longer get View#onApplyWindowInsets(WindowInsets).
        // If the app sets its own listener after WebView's constructor, then the app can override
        // our logic, which seems like a natural behavior.
        // For Android S, WebViewChromium can get onApplyWindowInsets(WindowInsets) call, so we do
        // not need to set the listener.
        // TODO(crbug.com/40699457): do not set listener and plumb WebViewChromium to handle
        // onApplyWindowInsets in S and above.
        containerView.setOnApplyWindowInsetsListener(
                new View.OnApplyWindowInsetsListener() {
                    @Override
                    public WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                        // Ignore if this is not the current container view.
                        if (view == mContainerView) {
                            return AwDisplayCutoutController.this.onApplyWindowInsets(insets);
                        } else {
                            if (DEBUG) Log.i(TAG, "Ignoring onApplyWindowInsets on View: " + view);
                            return insets;
                        }
                    }
                });
    }

    /**
     * Set the current container view.
     *
     * @param containerView The current container view.
     */
    public void setCurrentContainerView(View containerView) {
        if (DEBUG) Log.i(TAG, "setCurrentContainerView: " + containerView);
        mContainerView = containerView;
        // Ensure that we get new insets for the new container view.
        mContainerView.requestApplyInsets();
    }

    /**
     * Call this when window insets are first applied or changed.
     *
     * @see View#onApplyWindowInsets(WindowInsets)
     * @param insets The window (display) insets.
     */
    @VisibleForTesting
    public WindowInsets onApplyWindowInsets(final WindowInsets insets) {
        if (DEBUG) Log.i(TAG, "onApplyWindowInsets: " + insets.toString());
        // TODO(crbug.com/40699457): add a throttling logic.
        DisplayCutout cutout = insets.getDisplayCutout();
        // DisplayCutout can be null if there is no notch, or layoutInDisplayCutoutMode is DEFAULT
        // (before R) or consumed in the parent view.
        if (cutout != null) {
            Insets displayCutoutInsets =
                    new Insets(
                            cutout.getSafeInsetLeft(),
                            cutout.getSafeInsetTop(),
                            cutout.getSafeInsetRight(),
                            cutout.getSafeInsetBottom());
            onApplyWindowInsetsInternal(displayCutoutInsets);
        }
        return insets;
    }

    /**
     * Call this when window insets are first applied or changed.
     *
     * Similar to {@link onApplyWindowInsets(WindowInsets)}, but accepts
     * Rect as input.
     *
     * @param displayCutoutInsets Insets to store left, top, right, bottom insets.
     */
    @VisibleForTesting
    public void onApplyWindowInsetsInternal(final Insets displayCutoutInsets) {
        float dipScale = mDelegate.getDipScale();
        // We only apply this logic when webview is occupying the entire screen.
        adjustInsetsForScale(displayCutoutInsets, dipScale);

        if (DEBUG) {
            Log.i(
                    TAG,
                    "onApplyWindowInsetsInternal. insets: "
                            + displayCutoutInsets
                            + ", dip scale: "
                            + dipScale);
        }
        // Note that internally we apply this logic only when the display is in fullscreen mode.
        // See AwDisplayModeController for more details on how we check the fullscreen mode.
        mDelegate.setDisplayCutoutSafeArea(displayCutoutInsets);
    }

    private void onUpdateWindowInsets() {
        mContainerView.requestApplyInsets();
    }

    /** @see View#onSizeChanged(int, int, int, int) */
    public void onSizeChanged() {
        if (DEBUG) Log.i(TAG, "onSizeChanged");
        onUpdateWindowInsets();
    }

    /** @see View#onAttachedToWindow() */
    public void onAttachedToWindow() {
        if (DEBUG) Log.i(TAG, "onAttachedToWindow");
        onUpdateWindowInsets();
    }

    private static void adjustInsetsForScale(Insets insets, float dipScale) {
        insets.left = adjustOneInsetForScale(insets.left, dipScale);
        insets.top = adjustOneInsetForScale(insets.top, dipScale);
        insets.right = adjustOneInsetForScale(insets.right, dipScale);
        insets.bottom = adjustOneInsetForScale(insets.bottom, dipScale);
    }

    /**
     * Adjusts a WindowInset inset to a CSS pixel value.
     *
     * @param inset The inset as an integer.
     * @param dipScale The devices dip scale as an integer.
     * @return The CSS pixel value adjusted for scale.
     */
    private static int adjustOneInsetForScale(int inset, float dipScale) {
        return (int) Math.ceil(inset / dipScale);
    }
}
