// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Log;

/**
 * Display cutout controller for WebView.
 *
 * <p>This object should be constructed in WebView's constructor to support set listener logic for
 * Android P and above.
 *
 * <p>This controller is responsible for providing the values for the safe-area-insets CSS
 * properties for WebViews that occupy the entire screen height and width. The safe-area-insets
 * include the space for the display cutout as well as system bars.
 *
 * <p>To test the proper application of insets to web content, you can load a page such as
 * https://dpogue.ca/safe-area-inset-test/ that uses safe-area-inset CSS properties in the WebView
 * Shell app and toggle it to display in fullscreen.
 */
@Lifetime.WebView
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

        public Insets(int left, int top, int right, int bottom) {
            set(left, top, right, bottom);
        }

        public Insets(androidx.core.graphics.Insets insets) {
            set(insets.left, insets.top, insets.right, insets.bottom);
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
        public boolean equals(Object o) {
            if (!(o instanceof Insets)) return false;
            Insets i = (Insets) o;
            return left == i.left && top == i.top && right == i.right && bottom == i.bottom;
        }

        @Override
        public String toString() {
            return "Insets: (" + left + ", " + top + ")-(" + right + ", " + bottom + ")";
        }

        @Override
        public int hashCode() {
            return 3 * left + 5 * top + 7 * right + 11 * bottom;
        }
    }

    private final Delegate mDelegate;
    private View mContainerView;
    private final boolean mIncludeSystemBars;

    /**
     * Creates the {@link AwDisplayCutoutController} if required.
     *
     * <p>Display cutouts were added in Android P, this method returns null before that.
     */
    @Nullable
    public static AwDisplayCutoutController maybeCreate(Delegate delegate, View containerView) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return new AwDisplayCutoutController(delegate, containerView);
        } else {
            return null;
        }
    }

    /**
     * Constructor for AwDisplayCutoutController.
     *
     * @param delegate The delegate.
     * @param containerView The container view (WebView).
     */
    @RequiresApi(Build.VERSION_CODES.P)
    public AwDisplayCutoutController(Delegate delegate, View containerView) {
        mDelegate = delegate;
        mContainerView = containerView;
        mIncludeSystemBars =
                AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_SAFE_AREA_INCLUDES_SYSTEM_BARS);
        registerContainerView(containerView);
    }

    /**
     * Register a container view to listen to window insets.
     *
     * <p>Note that you do not need to register the containerView.
     *
     * @param containerView A container View, such as fullscreen view.
     */
    @RequiresApi(Build.VERSION_CODES.P)
    public void registerContainerView(View containerView) {
        if (DEBUG) Log.i(TAG, "registerContainerView");
        // For Android P~R, we set the listener in WebView's constructor.
        // Once we set the listener, we will no longer get View#onApplyWindowInsets(WindowInsets).
        // If the app sets its own listener after WebView's constructor, then the app can override
        // our logic, which seems like a natural behavior.
        // For Android S, WebViewChromium can get onApplyWindowInsets(WindowInsets) call, so we do
        // not need to set the listener.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) return;

        containerView.setOnApplyWindowInsetsListener(
                (view, insets) -> {
                    // Ignore if this is not the current container view.
                    if (view == mContainerView) {
                        return AwDisplayCutoutController.this.onApplyWindowInsets(insets);
                    } else {
                        if (DEBUG) Log.i(TAG, "Ignoring onApplyWindowInsets on View: " + view);
                        return insets;
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

        int insetTypes = WindowInsetsCompat.Type.displayCutout();
        if (mIncludeSystemBars) {
            insetTypes |= WindowInsetsCompat.Type.systemBars();
        }

        // TODO(crbug.com/40699457): add a throttling logic.
        Insets safeArea =
                new Insets(WindowInsetsCompat.toWindowInsetsCompat(insets).getInsets(insetTypes));
        onApplyWindowInsetsInternal(safeArea);

        return insets;
    }

    /**
     * Call this when window insets are first applied or changed.
     *
     * <p>Similar to {@link #onApplyWindowInsets(WindowInsets)}, but accepts Rect as input.
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
