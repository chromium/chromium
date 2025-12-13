// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;

import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.lang.ref.WeakReference;

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
         *     WebView. Note that DIP scale has been applied.
         */
        void setDisplayCutoutSafeArea(Insets insets);

        /**
         * Notify the delegate that bottom IME inset has changed. This usually signifies that the
         * keyboard has either been shown or hidden.
         */
        void bottomImeInsetChanged();
    }

    /**
     * This listener is a separate, static class as the listener will be added to the
     * ViewTreeObserver and may outlive the WebView. It should therefore not hold any strong
     * references to the WebView or the Delegate which would prevent the WebView from being garbage
     * collected.
     */
    private class Listener implements ViewPositionObserver.Listener {
        // The minimum amount of time between subsequent calls to mDelegate.bottomImeInsetChanged()
        // when invoked from this listener.
        private static final long MIN_VIEWPORT_UPDATE_TIME_MILLIS = 200;

        private final WeakReference<Delegate> mDelegate;
        private boolean mUpdatePending;

        private Listener(Delegate delegate) {
            mDelegate = new WeakReference<>(delegate);
        }

        @Override
        public void onPositionChanged(int positionX, int positionY) {
            Delegate delegate = mDelegate.get();
            if (delegate == null || mUpdatePending) return;
            mUpdatePending = true;
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        mUpdatePending = false;
                        AwDisplayCutoutController.this.recalculateBottomImeInset();
                        delegate.bottomImeInsetChanged();
                    },
                    MIN_VIEWPORT_UPDATE_TIME_MILLIS);
        }
    }

    private final Delegate mDelegate;
    private final Listener mViewMovedListener;
    private View mContainerView;
    private ViewPositionObserver mPositionObserver;

    // The amount the IME is currently imposing into the parent Window.
    private int mBottomImeInset;

    // The amount to inset the visual viewport after comparing mBottomImeInset with the WebView's
    // position.
    private int mVisualViewportBottomInset;

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
        mViewMovedListener = new Listener(mDelegate);
        mPositionObserver = new ViewPositionObserver(mContainerView);
        mPositionObserver.addListener(mViewMovedListener);
    }

    /**
     * Register a container view to listen to window insets.
     *
     * <p>Note that you do not need to register the containerView.
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
        mPositionObserver.removeListener(mViewMovedListener);
        mPositionObserver = new ViewPositionObserver(mContainerView);
        mPositionObserver.addListener(mViewMovedListener);
        // Ensure that we get new insets for the new container view.
        mContainerView.requestApplyInsets();
    }

    /**
     * Call this when window insets are first applied or changed.
     *
     * @see View#onApplyWindowInsets(WindowInsets)
     * @param insets The window (display) insets.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public WindowInsets onApplyWindowInsets(final WindowInsets insets) {
        if (DEBUG) Log.i(TAG, "onApplyWindowInsets: " + insets.toString());

        int insetTypes =
                WindowInsetsCompat.Type.displayCutout() | WindowInsetsCompat.Type.systemBars();

        // TODO(crbug.com/40699457): add a throttling logic.
        Insets safeArea = WindowInsetsCompat.toWindowInsetsCompat(insets).getInsets(insetTypes);
        Insets imeInsets =
                WindowInsetsCompat.toWindowInsetsCompat(insets)
                        .getInsets(WindowInsetsCompat.Type.ime());
        if (imeInsets.bottom != 0) {
            // If IME is showing, it will consume the bottom safe area (see crbug.com/457682720).
            safeArea = Insets.of(safeArea.left, safeArea.top, safeArea.right, 0);
        }
        onApplyWindowInsetsInternal(safeArea);
        calculateBottomImeInsetsInternal(imeInsets);

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
    public void onApplyWindowInsetsInternal(Insets displayCutoutInsets) {
        float dipScale = mDelegate.getDipScale();
        // We only apply this logic when webview is occupying the entire screen.
        displayCutoutInsets = adjustInsetsForScale(displayCutoutInsets, dipScale);

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

    public int getBottomImeInset() {
        return mVisualViewportBottomInset;
    }

    @VisibleForTesting
    public void recalculateBottomImeInset() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R || !mContainerView.isAttachedToWindow()) {
            // Unsupported version or view is not attached yet, no insets needed.
            mVisualViewportBottomInset = 0;
            return;
        }

        WindowMetrics wm =
                mContainerView
                        .getContext()
                        .getSystemService(WindowManager.class)
                        .getCurrentWindowMetrics();
        // These are the bounds of the current Window on the screen. These are absolute coordinates.
        Rect windowBounds = wm.getBounds();
        int[] pos = new int[2];
        mContainerView.getLocationInWindow(pos);
        // This represents the size and position of the WebView *relative* to the Window. These are
        // relative coordinates (to the top left corner of the Window).
        Rect viewRectInWindow =
                new Rect(
                        pos[0],
                        pos[1],
                        pos[0] + mContainerView.getWidth(),
                        pos[1] + mContainerView.getHeight());

        // This is the positive difference between the bottom of the WebView and the top of the IME.
        // For cases where the bottom of the WebView is higher than the top of the IME, return 0.
        // Otherwise, calculate the overlap by taking the bottom of the WebView (regardless of
        // whether this is obscured by the visible portion of the Window) and subtract the height of
        // the Window after deducting the IME overlap. This gives us the highest point in the
        // Window's coordinates that the IME reaches. In the case where the IME is not present
        // (mBottomImeInset is 0), this ensures that the visual viewport shows only the part of the
        // WebView that is visible in the Window.
        mVisualViewportBottomInset =
                Math.max(0, (viewRectInWindow.bottom - (windowBounds.height() - mBottomImeInset)));
    }

    private void calculateBottomImeInsetsInternal(Insets insets) {
        mBottomImeInset = insets.bottom;
        recalculateBottomImeInset();
        mDelegate.bottomImeInsetChanged();
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
        mPositionObserver.addListener(mViewMovedListener);
    }

    /**
     * @see View#onDetachedFromWindow()
     */
    public void onDetachedFromWindow() {
        mPositionObserver.removeListener(mViewMovedListener);
    }

    private static Insets adjustInsetsForScale(Insets insets, float dipScale) {
        return Insets.of(
                adjustOneInsetForScale(insets.left, dipScale),
                adjustOneInsetForScale(insets.top, dipScale),
                adjustOneInsetForScale(insets.right, dipScale),
                adjustOneInsetForScale(insets.bottom, dipScale));
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
