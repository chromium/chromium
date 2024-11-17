// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;

class StatusIndicatorMediator
        implements BrowserControlsStateProvider.Observer,
                View.OnLayoutChangeListener,
                TabObscuringHandler.Observer {
    private static final int STATUS_BAR_COLOR_TRANSITION_DURATION_MS = 200;
    private static final int FADE_TEXT_DURATION_MS = 150;
    private static final int UPDATE_COLOR_TRANSITION_DURATION_MS = 400;

    private PropertyModel mModel;
    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    private HashSet<StatusIndicatorCoordinator.StatusIndicatorObserver> mObservers =
            new HashSet<>();
    private final TabObscuringHandler mTabObscuringHandler;
    private Supplier<Integer> mStatusBarWithoutIndicatorColorSupplier;
    private Runnable mOnShowAnimationEnd;
    private Runnable mRegisterResource;
    private Runnable mUnregisterResource;
    private Supplier<Boolean> mCanAnimateNativeBrowserControls;
    private Callback<Runnable> mInvalidateCompositorView;
    private Runnable mRequestLayout;

    private ValueAnimator mStatusBarAnimation;
    private ValueAnimator mTextFadeInAnimation;
    private AnimatorSet mUpdateAnimatorSet;
    private AnimatorSet mHideAnimatorSet;

    private int mIndicatorHeight;
    private int mJavaLayoutHeight;
    private boolean mIsHiding;

    /**
     * Constructs the status indicator mediator.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} to listen to
     *                                     for the changes in controls offsets.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param statusBarWithoutIndicatorColorSupplier A supplier that will get the status bar color
     *                                               without taking the status indicator into
     *                                               account.
     * @param canAnimateNativeBrowserControls Will supply a boolean denoting whether the native
     *                                        browser controls can be animated. This will be false
     *                                        where we can't have a reliable cc::BCOM instance, e.g.
     *                                        tab switcher.
     */
    StatusIndicatorMediator(
            BrowserControlsStateProvider browserControlsStateProvider,
            TabObscuringHandler tabObscuringHandler,
            Supplier<Integer> statusBarWithoutIndicatorColorSupplier,
            Supplier<Boolean> canAnimateNativeBrowserControls) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabObscuringHandler = tabObscuringHandler;
        mStatusBarWithoutIndicatorColorSupplier = statusBarWithoutIndicatorColorSupplier;
        mCanAnimateNativeBrowserControls = canAnimateNativeBrowserControls;
    }

    /**
     * Initialize the mediator before first #animateShow().
     * @param model The {@link PropertyModel} for the status indicator.
     * @param registerResource A {@link Runnable} to register the view resource for the compositor
     *                         view.
     * @param unregisterResource A {@link Runnable} to unregister the view resource for the
     *                           compositor view.
     * @param invalidateCompositorView Callback to invalidate the compositor texture.
     * @param requestLayout Runnable to request layout for the view.
     */
    void initialize(
            PropertyModel model,
            Runnable registerResource,
            Runnable unregisterResource,
            Callback<Runnable> invalidateCompositorView,
            Runnable requestLayout) {
        mModel = model;
        mRegisterResource = registerResource;
        mUnregisterResource = unregisterResource;
        mInvalidateCompositorView = invalidateCompositorView;
        mRequestLayout = requestLayout;
        mTabObscuringHandler.addObserver(this);
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        onOffsetChanged(topControlsMinHeightOffset);
    }

    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        // Wait for first valid height while showing indicator.
        if (mIsHiding || mJavaLayoutHeight != 0 || v.getHeight() <= 0) return;

        mInvalidateCompositorView.onResult(null);
        mJavaLayoutHeight = v.getHeight();
        updateVisibility(false);
    }

    void destroy() {
        if (mStatusBarAnimation != null) mStatusBarAnimation.cancel();
        if (mTextFadeInAnimation != null) mTextFadeInAnimation.cancel();
        if (mUpdateAnimatorSet != null) mUpdateAnimatorSet.cancel();
        if (mHideAnimatorSet != null) mHideAnimatorSet.cancel();
        mBrowserControlsStateProvider.removeObserver(this);
        mTabObscuringHandler.removeObserver(this);
    }

    void addObserver(StatusIndicatorCoordinator.StatusIndicatorObserver observer) {
        mObservers.add(observer);
    }

    void removeObserver(StatusIndicatorCoordinator.StatusIndicatorObserver observer) {
        mObservers.remove(observer);
    }

    // Animations

    // TODO(sinansahin): We might want to end the running animations if we need to hide before we're
    // done showing/updating and vice versa.

    /**
     * Transitions the status bar color to the expected status indicator color background. Also,
     * initializes other properties, e.g. status text, status icon, and colors.
     *
     * These animations are transitioning the status bar color to the provided background color
     * (skipped if the background is the same as the current status bar color), then sliding in the
     * status indicator, and then fading in the status text with the icon.
     *
     * The animation timeline looks like this:
     *
     * Status bar transitions |*--------*
     * Indicator slides in    |         *--------*
     * Text fades in          |                  *------*
     *
     * @param statusText Status text to show.
     * @param statusIcon Compound drawable to show next to text.
     * @param backgroundColor Background color for the indicator.
     * @param textColor Status text color.
     * @param iconTint Compound drawable tint.
     */
    void animateShow(
            @NonNull String statusText,
            Drawable statusIcon,
            @ColorInt int backgroundColor,
            @ColorInt int textColor,
            @ColorInt int iconTint) {
        mRegisterResource.run();

        // TODO(sinansahin): Look into returning back to the right state earlier, ideally in
        // #onOffsetChanged after the view is hidden. It's currently challenging if the status
        // indicator is shown while a tab modal dialog is showing because the compositor
        // animation is blocked, and we don't get any signal to know if the indicator is hidden.
        mIsHiding = false;
        mJavaLayoutHeight = 0;

        Runnable initializeProperties =
                () -> {
                    mModel.set(StatusIndicatorProperties.STATUS_TEXT, statusText);
                    mModel.set(StatusIndicatorProperties.STATUS_ICON, statusIcon);
                    mModel.set(StatusIndicatorProperties.TEXT_ALPHA, 0.f);
                    mModel.set(StatusIndicatorProperties.BACKGROUND_COLOR, backgroundColor);
                    mModel.set(StatusIndicatorProperties.TEXT_COLOR, textColor);
                    mModel.set(StatusIndicatorProperties.ICON_TINT, iconTint);
                    mModel.set(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY, View.INVISIBLE);
                    mModel.set(StatusIndicatorProperties.IS_OBSCURED, false);
                    mOnShowAnimationEnd = () -> animateTextFadeIn();
                };

        final int statusBarColor = mStatusBarWithoutIndicatorColorSupplier.get();
        // If we aren't changing the status bar color, skip the status bar color animation and
        // continue with the rest of the animations.
        if (statusBarColor == backgroundColor) {
            initializeProperties.run();
            return;
        }

        mStatusBarAnimation = ValueAnimator.ofInt(statusBarColor, backgroundColor);
        mStatusBarAnimation.setEvaluator(new ArgbEvaluator());
        mStatusBarAnimation.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        mStatusBarAnimation.setDuration(STATUS_BAR_COLOR_TRANSITION_DURATION_MS);
        mStatusBarAnimation.addUpdateListener(
                anim -> {
                    for (StatusIndicatorCoordinator.StatusIndicatorObserver observer : mObservers) {
                        observer.onStatusIndicatorColorChanged((int) anim.getAnimatedValue());
                    }
                });
        mStatusBarAnimation.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        initializeProperties.run();
                        mStatusBarAnimation = null;
                    }
                });
        mStatusBarAnimation.start();
    }

    private void animateTextFadeIn() {
        mTextFadeInAnimation = ValueAnimator.ofFloat(0.f, 1.f);
        mTextFadeInAnimation.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        mTextFadeInAnimation.setDuration(FADE_TEXT_DURATION_MS);
        mTextFadeInAnimation.addUpdateListener(
                anim -> {
                    final float currentAlpha = (float) anim.getAnimatedValue();
                    mModel.set(StatusIndicatorProperties.TEXT_ALPHA, currentAlpha);
                });
        mTextFadeInAnimation.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onStart(Animator animation) {
                        mRequestLayout.run();
                    }

                    @Override
                    public void onEnd(Animator animator) {
                        mTextFadeInAnimation = null;
                        notifyShowAnimationEnd();
                    }
                });
        mTextFadeInAnimation.start();
    }

    // TODO(sinansahin): See if/how we can skip some of the animations if the properties didn't
    // change. This might require UX guidance.

    /**
     * Updates the contents and background of the status indicator with animations.
     *
     * These animations are transitioning the status bar and the background color to the provided
     * background color while fading out the status text with the icon, and then fading in the new
     * status text with the icon (with the provided color and tint).
     *
     * The animation timeline looks like this:
     *
     * Old text fades out               |*------*
     * Background/status bar transition |*------------------*
     * New text fades in                |                   *------*
     *
     * @param statusText New status text to show.
     * @param statusIcon New compound drawable to show next to text.
     * @param backgroundColor New background color for the indicator.
     * @param textColor New status text color.
     * @param iconTint New compound drawable tint.
     * @param animationCompleteCallback Callback to run after the animation is done.
     */
    void animateUpdate(
            @NonNull String statusText,
            Drawable statusIcon,
            @ColorInt int backgroundColor,
            @ColorInt int textColor,
            @ColorInt int iconTint,
            Runnable animationCompleteCallback) {
        final boolean changed =
                !statusText.equals(mModel.get(StatusIndicatorProperties.STATUS_TEXT))
                        || statusIcon != mModel.get(StatusIndicatorProperties.STATUS_ICON)
                        || backgroundColor != mModel.get(StatusIndicatorProperties.BACKGROUND_COLOR)
                        || textColor != mModel.get(StatusIndicatorProperties.TEXT_COLOR)
                        || iconTint != mModel.get(StatusIndicatorProperties.ICON_TINT);
        assert changed
                : "#animateUpdate() shouldn't be called without any change to the status"
                        + " indicator.";

        // 1. Fade out old text.
        ValueAnimator fadeOldOut = ValueAnimator.ofFloat(1.f, 0.f);
        fadeOldOut.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        fadeOldOut.setDuration(FADE_TEXT_DURATION_MS);
        fadeOldOut.addUpdateListener(
                anim -> {
                    final float currentAlpha = (float) anim.getAnimatedValue();
                    mModel.set(StatusIndicatorProperties.TEXT_ALPHA, currentAlpha);
                });
        fadeOldOut.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        mModel.set(StatusIndicatorProperties.STATUS_TEXT, statusText);
                        mModel.set(StatusIndicatorProperties.STATUS_ICON, statusIcon);
                        mModel.set(StatusIndicatorProperties.TEXT_COLOR, textColor);
                        mModel.set(StatusIndicatorProperties.ICON_TINT, iconTint);
                    }
                });

        // 2. Simultaneously transition the background.
        ValueAnimator colorAnimation =
                ValueAnimator.ofInt(
                        mModel.get(StatusIndicatorProperties.BACKGROUND_COLOR), backgroundColor);
        colorAnimation.setEvaluator(new ArgbEvaluator());
        colorAnimation.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        colorAnimation.setDuration(UPDATE_COLOR_TRANSITION_DURATION_MS);
        colorAnimation.addUpdateListener(
                anim -> {
                    final int currentColor = (int) anim.getAnimatedValue();
                    mModel.set(StatusIndicatorProperties.BACKGROUND_COLOR, currentColor);
                    notifyColorChange(currentColor);
                });

        // 3. Fade in new text, after #1 and #2 are done.
        ValueAnimator fadeNewIn = ValueAnimator.ofFloat(0.f, 1.f);
        fadeNewIn.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        fadeNewIn.setDuration(FADE_TEXT_DURATION_MS);
        fadeNewIn.addUpdateListener(
                anim -> {
                    final float currentAlpha = (float) anim.getAnimatedValue();
                    mModel.set(StatusIndicatorProperties.TEXT_ALPHA, currentAlpha);
                });

        mUpdateAnimatorSet = new AnimatorSet();
        mUpdateAnimatorSet.play(fadeOldOut).with(colorAnimation);
        mUpdateAnimatorSet.play(fadeNewIn).after(colorAnimation);
        mUpdateAnimatorSet.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        animationCompleteCallback.run();
                        mUpdateAnimatorSet = null;
                    }
                });
        mUpdateAnimatorSet.start();
    }

    /**
     * Hide the status indicator with animations.
     *
     * These animations are transitioning the status bar and background color to the system color
     * while fading the text out, and then sliding the indicator out.
     *
     * The animation timeline looks like this:
     *
     * Status bar and background transition |*--------*
     * Text fades out                       |*------*
     * Indicator slides out                 |         *--------*
     */
    void animateHide() {
        // 1. Transition the background.
        ValueAnimator colorAnimation =
                ValueAnimator.ofInt(
                        mModel.get(StatusIndicatorProperties.BACKGROUND_COLOR),
                        mStatusBarWithoutIndicatorColorSupplier.get());
        colorAnimation.setEvaluator(new ArgbEvaluator());
        colorAnimation.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        colorAnimation.setDuration(STATUS_BAR_COLOR_TRANSITION_DURATION_MS);
        colorAnimation.addUpdateListener(
                anim -> {
                    final int currentColor = (int) anim.getAnimatedValue();
                    mModel.set(StatusIndicatorProperties.BACKGROUND_COLOR, currentColor);
                    notifyColorChange(currentColor);
                });
        colorAnimation.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        notifyColorChange(Color.TRANSPARENT);
                    }
                });

        // 2. Fade out the text simultaneously with #1.
        ValueAnimator fadeOut = ValueAnimator.ofFloat(1.f, 0.f);
        fadeOut.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        fadeOut.setDuration(FADE_TEXT_DURATION_MS);
        fadeOut.addUpdateListener(
                anim ->
                        mModel.set(
                                StatusIndicatorProperties.TEXT_ALPHA,
                                (float) anim.getAnimatedValue()));

        mHideAnimatorSet = new AnimatorSet();
        mHideAnimatorSet.play(colorAnimation).with(fadeOut);
        mHideAnimatorSet.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        if (mCanAnimateNativeBrowserControls.get()) {
                            mInvalidateCompositorView.onResult(() -> updateVisibility(true));
                        } else {
                            updateVisibility(true);
                        }
                        mHideAnimatorSet = null;
                    }
                });
        mHideAnimatorSet.start();
    }

    // Observer notifiers

    private void notifyHeightChange(int height) {
        for (StatusIndicatorCoordinator.StatusIndicatorObserver observer : mObservers) {
            observer.onStatusIndicatorHeightChanged(height);
        }
    }

    private void notifyColorChange(@ColorInt int color) {
        for (StatusIndicatorCoordinator.StatusIndicatorObserver observer : mObservers) {
            observer.onStatusIndicatorColorChanged(color);
        }
    }

    private void notifyShowAnimationEnd() {
        for (StatusIndicatorCoordinator.StatusIndicatorObserver observer : mObservers) {
            observer.onStatusIndicatorShowAnimationEnd();
        }
    }

    // Other internal methods

    /**
     * Call to kick off height change when status indicator is shown/hidden.
     * @param hiding Whether the status indicator is hiding.
     */
    private void updateVisibility(boolean hiding) {
        mIsHiding = hiding;
        mIndicatorHeight = hiding ? 0 : mJavaLayoutHeight;

        if (!mIsHiding) {
            mBrowserControlsStateProvider.addObserver(this);
        }

        notifyHeightChange(mIndicatorHeight);
    }

    private void onOffsetChanged(int topControlsMinHeightOffset) {
        final boolean indicatorVisible = topControlsMinHeightOffset > 0;
        // Composited view should be visible if we have a positive top min-height offset (or current
        // min-height) and we're running the animations in native.
        mModel.set(
                StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE,
                indicatorVisible && mCanAnimateNativeBrowserControls.get());

        mModel.set(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT, topControlsMinHeightOffset);

        final boolean isCompletelyShown =
                indicatorVisible && topControlsMinHeightOffset == mIndicatorHeight;
        // If we're running the animations in native, the Android view should only be visible when
        // the indicator is fully shown. Otherwise, the Android view will be visible if it's within
        // screen boundaries.
        mModel.set(
                StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY,
                mIsHiding && (mCanAnimateNativeBrowserControls.get() || !indicatorVisible)
                        ? View.GONE
                        : (isCompletelyShown || !mCanAnimateNativeBrowserControls.get()
                                ? View.VISIBLE
                                : View.INVISIBLE));

        if (mOnShowAnimationEnd != null && isCompletelyShown) {
            mOnShowAnimationEnd.run();
            mOnShowAnimationEnd = null;
        }

        final boolean doneHiding = !indicatorVisible && mIsHiding;
        if (doneHiding) {
            // This block is currently never executed if the status indicator is shown and hidden
            // while a modal dialog is visible. |mIsHiding| and |mJavaLayoutHeight| are also reset
            // in #animateShow as a precaution in case this happens.
            mBrowserControlsStateProvider.removeObserver(this);
            mIsHiding = false;
            mJavaLayoutHeight = 0;
            mUnregisterResource.run();
        }
    }

    void updateVisibilityForTesting(boolean hiding) {
        updateVisibility(hiding);
    }

    void finishAnimationsForTesting() {
        if (mStatusBarAnimation != null && mStatusBarAnimation.isRunning()) {
            mStatusBarAnimation.end();
        }
        if (mTextFadeInAnimation != null && mTextFadeInAnimation.isRunning()) {
            mTextFadeInAnimation.end();
        }
        if (mUpdateAnimatorSet != null && mUpdateAnimatorSet.isRunning()) {
            mUpdateAnimatorSet.end();
        }
        if (mHideAnimatorSet != null && mHideAnimatorSet.isRunning()) {
            mHideAnimatorSet.end();
        }
    }

    @Override
    public void updateObscured(boolean obscureTabContent, boolean obscureToolbar) {
        mModel.set(StatusIndicatorProperties.IS_OBSCURED, obscureToolbar);
    }
}
