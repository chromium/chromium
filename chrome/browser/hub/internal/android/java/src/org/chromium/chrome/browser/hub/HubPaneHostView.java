// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.swipe.HubPaneSwipeCoordinator;

/** Holds the current pane's {@link View}. */
@NullMarked
public class HubPaneHostView extends FrameLayout {
    /** A provider of adjacent pane views during a swipe drag gesture. */
    public interface PaneViewProvider {
        /**
         * Prepares the adjacent pane for display (warming resources) and returns its root view, or
         * null if there is no adjacent pane to switch to.
         */
        @Nullable View prepareAndGetAdjacentPaneView(boolean isSwipeLeft);

        /**
         * Notifies that the swipe drag has completed successfully, indicating the Hub should switch
         * focus to the adjacent pane.
         */
        void onSwipeSwitchComplete(boolean isSwipeLeft);

        /** Notifies that the swipe drag was cancelled or settled back to the original pane. */
        default void onSwipeSwitchCancel(boolean isSwipeLeft) {}

        /** Notifies of the active drag displacement progress (fraction from 0.0 to 1.0). */
        default void onSwipeDragProgress(float progress, boolean isSwipeLeft) {}
    }

    /** A checker to see if a touch is on an interactive element. */
    public interface InteractiveElementChecker {
        /**
         * Returns whether the touch at (x, y) (relative to the pane's root view) is interactive.
         */
        boolean isTouchOnInteractiveElement(float x, float y);
    }

    private final HubColorMixerRegistrationHelper mColorMixerHelper =
            new HubColorMixerRegistrationHelper();
    private FrameLayout mPaneFrame;
    private ViewGroup mSnackbarContainer;
    private @Nullable NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private @MonotonicNonNull HubPaneSwipeCoordinator mSwipeCoordinator;

    /** Default {@link FrameLayout} constructor called by inflation. */
    public HubPaneHostView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPaneFrame = findViewById(R.id.pane_frame);
        mSnackbarContainer = findViewById(R.id.pane_host_view_snackbar_container);
        mSwipeCoordinator = new HubPaneSwipeCoordinator(this, mPaneFrame);

        Context context = getContext();
        mColorMixerHelper.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> getBackgroundColor(context, colorScheme),
                        mPaneFrame::setBackgroundColor));
    }

    public void setPaneViewProvider(@Nullable PaneViewProvider provider) {
        assert mSwipeCoordinator != null;
        mSwipeCoordinator.setPaneViewProvider(provider);
    }

    public void setInteractiveElementChecker(@Nullable InteractiveElementChecker checker) {
        assert mSwipeCoordinator != null;
        mSwipeCoordinator.setInteractiveElementChecker(checker);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent motionEvent) {
        assert mSwipeCoordinator != null;
        return mSwipeCoordinator.onInterceptTouchEvent(motionEvent);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        assert mSwipeCoordinator != null;
        boolean handled = mSwipeCoordinator.onTouchEvent(event);
        if (handled && event.getActionMasked() == MotionEvent.ACTION_UP) {
            performClick();
        }
        return handled;
    }

    @Override
    public boolean performClick() {
        // This is a no-op, but we need to override it for accessibility.
        super.performClick();
        return true;
    }

    /**
     * Sets the root view for the pane host, animating the transition if both old and new views are
     * non-null.
     *
     * @param newRootView The new root view to display.
     * @param isSlideAnimationLeftToRight Whether the animation should slide from left-to-right
     *     (true) or right-to-left (false), only when slide animation is enabled.
     */
    void setRootView(@Nullable View newRootView, boolean isSlideAnimationLeftToRight) {
        assert mSwipeCoordinator != null;
        mSwipeCoordinator.setRootView(newRootView, isSlideAnimationLeftToRight);
    }

    void setColorMixer(HubColorMixer mixer) {
        mColorMixerHelper.setColorMixer(mixer);
    }

    void setSnackbarContainerConsumer(Callback<ViewGroup> consumer) {
        consumer.onResult(mSnackbarContainer);
    }

    private @ColorInt int getBackgroundColor(Context context, @HubColorScheme int colorScheme) {
        boolean isXrFullSpaceMode =
                mXrSpaceModeObservableSupplier != null && mXrSpaceModeObservableSupplier.get();
        return HubColors.getBackgroundColor(context, colorScheme, isXrFullSpaceMode);
    }

    public void setXrSpaceModeObservableSupplier(
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
    }

    public void destroy() {
        if (mSwipeCoordinator != null) {
            mSwipeCoordinator.destroy();
        }
        mColorMixerHelper.destroy();
        if (mPaneFrame != null) {
            mPaneFrame.removeAllViews();
        }
    }
}
