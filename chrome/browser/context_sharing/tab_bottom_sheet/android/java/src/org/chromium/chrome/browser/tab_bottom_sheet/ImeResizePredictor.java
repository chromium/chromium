// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isActivityInactive;
import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isEdgeToEdgeEnabled;

import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewParent;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;

import androidx.annotation.Px;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;

import java.util.List;

/**
 * Predicts and tracks the target height of the Tab Bottom Sheet during soft keyboard (IME)
 * animation transitions, enabling single-step resizing of WebContents.
 */
@NullMarked
class ImeResizePredictor {
    /**
     * Delegate interface implemented by {@link WebViewResizingHelper} for bounds application and
     * state queries.
     */
    interface Delegate {
        /** Sentinel value indicating no height override is requested. */
        int NO_HEIGHT_OVERRIDE = -1;

        /** Sentinel value indicating no safe area override is requested. */
        int NO_SAFE_AREA_OVERRIDE = -1;

        /** Returns whether resizing mode lock is currently held (viewport size fixed). */
        boolean isViewportSizeFixed();

        /**
         * Updates container bounds and returns whether dimensions changed.
         *
         * @param ignoreCache Whether to ignore the previous dimension cache.
         * @param targetHeight Target height in pixels, or {@link #NO_HEIGHT_OVERRIDE} to use
         *     measured height.
         * @param bottomSafeAreaInsetPx Target bottom safe-area inset in pixels, or {@link
         *     #NO_SAFE_AREA_OVERRIDE}.
         * @return True if dimensions changed, false otherwise.
         */
        boolean updateBounds(
                boolean ignoreCache, @Px int targetHeight, @Px int bottomSafeAreaInsetPx);

        /**
         * Updates safe area insets on WebContents and placeholder.
         *
         * @param ignoreCache Whether to ignore cached inset values.
         * @param bottomInsetPx Target bottom inset in pixels.
         */
        void updateSafeAreaInsets(boolean ignoreCache, @Px int bottomInsetPx);

        /**
         * Notifies that post-IME bounds reconciliation completed.
         *
         * @param sizeChanged Whether container bounds changed during reconciliation.
         */
        void onPostResizeReconciled(boolean sizeChanged);

        /**
         * Restores WebContents and placeholder visibility synchronously if awaiting deferred
         * fade-in.
         */
        void restoreVisibilityIfAwaitingFadeIn();
    }

    private static final long IME_ANIMATION_WATCHDOG_MS = 600;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final @Nullable View mContainerView;
    private final @Nullable View mResizingContainer;
    private final @Nullable WindowAndroid mWindowAndroid;
    private final @Nullable InsetObserver mInsetObserver;
    private final @Nullable BottomSheetController mBottomSheetController;
    private final boolean mIsSidePanel;
    private final @Px int mWebUiTopMarginPx;
    private @Nullable Delegate mDelegate;

    private @Nullable WindowInsetsAnimationCompat mActiveImeAnimation;
    private boolean mIsPaused;
    private boolean mIsKeyboardOpen;
    private boolean mIsClosingAnimation;
    private @Px int mImeInsetAtPreparePx;
    private boolean mPendingIgnoreCache;

    private final Runnable mImeWatchdogRunnable;
    private @Nullable OnGlobalLayoutListener mPendingImeEndLayoutListener;
    private @Nullable ViewTreeObserver mPendingImeEndLayoutObserver;

    ImeResizePredictor(
            @Nullable View containerView,
            @Nullable View resizingContainer,
            @Nullable WindowAndroid windowAndroid,
            @Nullable InsetObserver insetObserver,
            @Nullable BottomSheetController bottomSheetController,
            boolean isSidePanel,
            @Px int webUiTopMarginPx,
            @Nullable Delegate delegate) {
        mContainerView = containerView;
        mResizingContainer = resizingContainer;
        mWindowAndroid = windowAndroid;
        mInsetObserver = insetObserver;
        mBottomSheetController = bottomSheetController;
        mIsSidePanel = isSidePanel;
        mWebUiTopMarginPx = webUiTopMarginPx;
        mDelegate = delegate;
        mImeWatchdogRunnable =
                () -> {
                    if (isActivityInactive(mWindowAndroid)
                            || isOtherSheetContentShowing()
                            || isSheetHidden()
                            || isKeyboardInOverlayMode()) {
                        clearPendingImeTransition();
                        return;
                    }
                    clearActiveAnimations(mContainerView);
                    boolean ignoreCache = mPendingIgnoreCache;
                    mPendingIgnoreCache = false;
                    reconcilePostImeBounds(ignoreCache);
                };
    }

    boolean isPaused() {
        return mIsPaused;
    }

    boolean isAnimationRunning() {
        return mActiveImeAnimation != null;
    }

    boolean isClosingAnimation() {
        return mIsClosingAnimation;
    }

    boolean isTargetImeVisible() {
        if (mIsClosingAnimation) {
            return false;
        }
        return getRawImeBottom() > 0 || mIsKeyboardOpen;
    }

    boolean isPendingLayoutListenerAttached() {
        return mPendingImeEndLayoutListener != null;
    }

    private boolean isLayoutPending() {
        View target = mResizingContainer != null ? mResizingContainer : mContainerView;
        if (target == null) return false;
        if (target.isInLayout() || target.isLayoutRequested()) return true;
        ViewParent parent = target.getParent();
        while (parent instanceof View) {
            if (((View) parent).isLayoutRequested()) return true;
            parent = parent.getParent();
        }
        return false;
    }

    @Px
    int getRawImeBottom() {
        InsetObserver insetObserver = mInsetObserver;
        if (insetObserver == null) {
            return 0;
        }
        WindowInsetsCompat insets = insetObserver.getLastRawWindowInsets();
        return insets != null ? insets.getInsets(WindowInsetsCompat.Type.ime()).bottom : 0;
    }

    private boolean isKeyboardShowing(@Nullable View view) {
        if (mInsetObserver == null) {
            return false;
        }
        WindowInsetsCompat insets = mInsetObserver.getLastRawWindowInsets();
        if (insets == null && view != null) {
            insets = ViewCompat.getRootWindowInsets(view);
        }
        return insets != null && insets.isVisible(WindowInsetsCompat.Type.ime());
    }

    private boolean isKeyboardInOverlayMode() {
        return mInsetObserver != null && mInsetObserver.isKeyboardInOverlayMode();
    }

    boolean isOtherSheetContentShowing() {
        if (mIsSidePanel || mBottomSheetController == null || mContainerView == null) {
            return false;
        }
        var currentContent = mBottomSheetController.getCurrentSheetContent();
        return currentContent != null
                && currentContent.getContentView() != null
                && currentContent.getContentView() != mContainerView;
    }

    boolean onPrepare(@Nullable WindowInsetsAnimationCompat animation) {
        if (animation == null
                || !isImeAnimation(animation)
                || isActivityInactive(mWindowAndroid)
                || isOtherSheetContentShowing()
                || isSheetHidden()
                || isKeyboardInOverlayMode()) {
            return false;
        }
        mActiveImeAnimation = animation;
        mImeInsetAtPreparePx = getRawImeBottom();
        return true;
    }

    private static boolean areBoundsValid(@Nullable BoundsCompat bounds) {
        if (bounds == null) {
            return false;
        }
        @Px int lower = bounds.getLowerBound().bottom;
        @Px int upper = bounds.getUpperBound().bottom;
        if (lower == 0 && upper == 0) {
            return false;
        }
        if (lower == upper && upper > 0) {
            return false;
        }
        return true;
    }

    @Px
    int onStart(@Nullable WindowInsetsAnimationCompat animation, @Nullable BoundsCompat bounds) {
        if (animation == null || !isImeAnimation(animation)) {
            return 0;
        }
        if (!areBoundsValid(bounds)) {
            return abortStartIfInactive();
        }
        if (isActivityInactive(mWindowAndroid)
                || isOtherSheetContentShowing()
                || isSheetHidden()
                || isKeyboardInOverlayMode()) {
            return abortStartIfInactive();
        }

        assert bounds != null;
        @Px int lower = bounds.getLowerBound().bottom;
        @Px int upper = bounds.getUpperBound().bottom;

        mActiveImeAnimation = animation;
        mIsPaused = true;

        @Px int currentRawIme = getRawImeBottom();
        boolean isClosing =
                lower == 0
                        && (mIsKeyboardOpen || mImeInsetAtPreparePx > 0)
                        && (currentRawIme == 0 || upper == mImeInsetAtPreparePx || upper == 0);
        mIsClosingAnimation = isClosing;
        if (!isClosing && upper > 0) {
            mIsKeyboardOpen = true;
        }

        return getProjectedWebUiHeight();
    }

    boolean onEnd(@Nullable WindowInsetsAnimationCompat animation, @Nullable View view) {
        if (animation != null && !isImeAnimation(animation)) {
            return false;
        }
        if (mActiveImeAnimation != null && animation != null && animation != mActiveImeAnimation) {
            return false;
        }
        mActiveImeAnimation = null;
        boolean wasClosing = mIsClosingAnimation;
        mIsPaused = false;
        mIsClosingAnimation = false;
        mImeInsetAtPreparePx = 0;
        mIsKeyboardOpen = !wasClosing && isKeyboardShowing(view) && getRawImeBottom() > 0;
        return wasClosing;
    }

    void clearActiveAnimations(@Nullable View view) {
        mActiveImeAnimation = null;
        mIsPaused = false;
        mIsClosingAnimation = false;
        mImeInsetAtPreparePx = 0;
        mIsKeyboardOpen = isKeyboardShowing(view) && getRawImeBottom() > 0;
    }

    void reset() {
        clearPendingImeTransition();
    }

    void destroy() {
        reset();
        mDelegate = null;
    }

    void onPrepareAnimation(@Nullable WindowInsetsAnimationCompat animation) {
        if (onPrepare(animation)) {
            scheduleImeWatchdog();
        }
    }

    void onStartAnimation(
            @Nullable WindowInsetsAnimationCompat animation, @Nullable BoundsCompat bounds) {
        if (mDelegate == null) {
            return;
        }
        if (animation == null || !isImeAnimation(animation)) {
            return;
        }
        if (isActivityInactive(mWindowAndroid)
                || isOtherSheetContentShowing()
                || isSheetHidden()
                || isKeyboardInOverlayMode()) {
            clearPendingImeTransition();
            return;
        }
        if (!areBoundsValid(bounds)) {
            clearPendingImeTransition();
            return;
        }
        cancelPendingImeCallbacks();

        @Px int projectedHeight = onStart(animation, bounds);
        @Px int targetBottomSafeInset = resolveTargetBottomSafeInsetPx();
        if (projectedHeight > 0 && !isViewportSizeFixed()) {
            mDelegate.updateBounds(/* ignoreCache= */ true, projectedHeight, targetBottomSafeInset);
        }
        if (isAnimationRunning()) {
            scheduleImeWatchdog();
        } else {
            mDelegate.restoreVisibilityIfAwaitingFadeIn();
        }
    }

    void onProgressAnimation(@Nullable List<WindowInsetsAnimationCompat> runningAnimations) {
        // In 1-step resize, bounds are applied in onStartAnimation.
    }

    void onEndAnimation(@Nullable WindowInsetsAnimationCompat animation) {
        if (isKeyboardInOverlayMode()
                || isActivityInactive(mWindowAndroid)
                || isOtherSheetContentShowing()
                || isSheetHidden()) {
            clearPendingImeTransition();
            return;
        }
        if (animation != null && !isImeAnimation(animation)) {
            return;
        }
        if (mActiveImeAnimation != null && animation != null && animation != mActiveImeAnimation) {
            return;
        }
        cancelImeWatchdog();
        boolean wasClosing = onEnd(animation, mContainerView);
        @Px
        int targetBottomSafeInset =
                wasClosing
                                && isEdgeToEdgeEnabled()
                                && !mIsSidePanel
                                && mBottomSheetController != null
                        ? mBottomSheetController.getEdgeToEdgeBottomInset()
                        : resolveTargetBottomSafeInsetPx();
        boolean ignoreCache = mPendingIgnoreCache;
        mPendingIgnoreCache = false;
        if (isLayoutPending()
                && attachPendingImeEndLayoutListener(
                        ignoreCache,
                        Delegate.NO_HEIGHT_OVERRIDE,
                        targetBottomSafeInset,
                        wasClosing)) {
            scheduleImeWatchdog();
            return;
        }
        cancelImeWatchdog();
        reconcilePostImeBounds(ignoreCache, Delegate.NO_HEIGHT_OVERRIDE, targetBottomSafeInset);
    }

    void onActivityResumed() {
        if (mDelegate == null) {
            return;
        }
        clearPendingImeTransition();
        if (isOtherSheetContentShowing() || isSheetHidden()) {
            return;
        }
        if (mIsSidePanel || mBottomSheetController == null) {
            if (!isViewportSizeFixed()) {
                boolean sizeChanged =
                        mDelegate.updateBounds(
                                /* ignoreCache= */ true,
                                Delegate.NO_HEIGHT_OVERRIDE,
                                Delegate.NO_SAFE_AREA_OVERRIDE);
                mDelegate.onPostResizeReconciled(sizeChanged);
            } else {
                mDelegate.restoreVisibilityIfAwaitingFadeIn();
            }
            return;
        }
        if (!isViewportSizeFixed()) {
            reconcilePostImeBounds(/* ignoreCache= */ true);
        }
    }

    void onKeyboardInsetChanged() {
        if (isKeyboardInOverlayMode()
                || isActivityInactive(mWindowAndroid)
                || isOtherSheetContentShowing()
                || isSheetHidden()) {
            return;
        }
        if (!isAnimationRunning() && !isPendingLayoutListenerAttached()) {
            mIsKeyboardOpen = isKeyboardShowing(mContainerView) && getRawImeBottom() > 0;
        }
        if (!isAnimationRunning() && !isPendingLayoutListenerAttached()) {
            if (mDelegate != null && !isViewportSizeFixed()) {
                mDelegate.updateSafeAreaInsets(
                        /* ignoreCache= */ false, resolveTargetBottomSafeInsetPx());
            }
        }
    }

    private boolean isSheetHidden() {
        return mBottomSheetController != null
                && (mBottomSheetController.getSheetState() == SheetState.HIDDEN
                        || mBottomSheetController.getSheetState() == SheetState.PEEK);
    }

    void onSheetContentChanged() {
        clearPendingImeTransition();
        if (isOtherSheetContentShowing() || isSheetHidden()) {
            return;
        }
        if (mDelegate != null && !isViewportSizeFixed()) {
            mDelegate.updateBounds(
                    /* ignoreCache= */ true,
                    Delegate.NO_HEIGHT_OVERRIDE,
                    resolveTargetBottomSafeInsetPx());
        }
    }

    void onSheetStateChanged(@SheetState int newState) {
        if (newState == SheetState.HIDDEN || newState == SheetState.PEEK) {
            clearPendingImeTransition();
        } else if (newState == SheetState.HALF || newState == SheetState.FULL) {
            if (!isAnimationRunning() && !isPendingLayoutListenerAttached()) {
                mIsKeyboardOpen = isKeyboardShowing(mContainerView) && getRawImeBottom() > 0;
            }
        }
    }

    void onDisableResizingMode() {
        if (isActivityInactive(mWindowAndroid)
                || isOtherSheetContentShowing()
                || isSheetHidden()
                || isKeyboardInOverlayMode()) {
            clearPendingImeTransition();
            return;
        }
        removePendingImeEndLayoutListener();
        @Px int targetBottomSafeInset = resolveTargetBottomSafeInsetPx();
        @Px int targetHeight = Delegate.NO_HEIGHT_OVERRIDE;
        if (mIsPaused) {
            @Px int projectedHeight = getProjectedWebUiHeight();
            targetHeight = projectedHeight > 0 ? projectedHeight : Delegate.NO_HEIGHT_OVERRIDE;
        }
        if (isLayoutPending()
                && attachPendingImeEndLayoutListener(
                        /* ignoreCache= */ true,
                        targetHeight,
                        targetBottomSafeInset,
                        mIsClosingAnimation)) {
            scheduleImeWatchdog();
            return;
        }
        if (mIsPaused || isAnimationRunning()) {
            scheduleImeWatchdog();
        } else {
            cancelImeWatchdog();
        }
        reconcilePostImeBounds(/* ignoreCache= */ true, targetHeight, targetBottomSafeInset);
    }

    boolean shouldSuppressBottomMarginUpdate() {
        return isAnimationRunning() || isPendingLayoutListenerAttached();
    }

    void clearPendingImeTransition() {
        cancelPendingImeCallbacks();
        clearActiveAnimations(mContainerView);
        mPendingIgnoreCache = false;
        mImeInsetAtPreparePx = 0;
        if (mDelegate != null) {
            mDelegate.restoreVisibilityIfAwaitingFadeIn();
        }
    }

    boolean reconcilePostImeBounds(boolean ignoreCache) {
        return reconcilePostImeBounds(
                ignoreCache, Delegate.NO_HEIGHT_OVERRIDE, resolveTargetBottomSafeInsetPx());
    }

    boolean reconcilePostImeBounds(
            boolean ignoreCache, @Px int targetHeight, @Px int targetBottomSafeInset) {
        removePendingImeEndLayoutListener();
        if (mDelegate == null || isViewportSizeFixed()) {
            return false;
        }
        boolean sizeChanged =
                mDelegate.updateBounds(ignoreCache, targetHeight, targetBottomSafeInset);
        mDelegate.onPostResizeReconciled(sizeChanged);
        return sizeChanged;
    }

    void scheduleImeWatchdog() {
        cancelImeWatchdog();
        mHandler.postDelayed(mImeWatchdogRunnable, IME_ANIMATION_WATCHDOG_MS);
    }

    void cancelImeWatchdog() {
        mHandler.removeCallbacks(mImeWatchdogRunnable);
    }

    void cancelPendingImeCallbacks() {
        cancelImeWatchdog();
        removePendingImeEndLayoutListener();
    }

    private boolean attachPendingImeEndLayoutListener(
            boolean ignoreCache,
            @Px int targetHeight,
            @Px int targetBottomSafeInset,
            boolean wasClosing) {
        View target = mResizingContainer != null ? mResizingContainer : mContainerView;
        if (target == null) {
            return false;
        }
        if (target != mContainerView
                && (target.getParent() == null || !target.isAttachedToWindow())) {
            return false;
        }
        ViewTreeObserver observer = target.getViewTreeObserver();
        if (observer == null || !observer.isAlive()) {
            return false;
        }
        mPendingIgnoreCache |= ignoreCache;
        removePendingImeEndLayoutListener();
        final boolean effectiveIgnoreCache = mPendingIgnoreCache;
        mPendingImeEndLayoutListener =
                new OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        removePendingImeEndLayoutListener();
                        mPendingIgnoreCache = false;
                        if (isActivityInactive(mWindowAndroid)
                                || isOtherSheetContentShowing()
                                || isSheetHidden()
                                || isKeyboardInOverlayMode()) {
                            clearPendingImeTransition();
                            return;
                        }
                        if (isAnimationRunning() || mIsPaused) {
                            scheduleImeWatchdog();
                        } else {
                            cancelImeWatchdog();
                            clearActiveAnimations(mContainerView);
                        }
                        @Px
                        int safeInset =
                                wasClosing
                                                && isEdgeToEdgeEnabled()
                                                && !mIsSidePanel
                                                && mBottomSheetController != null
                                        ? mBottomSheetController.getEdgeToEdgeBottomInset()
                                        : targetBottomSafeInset;
                        reconcilePostImeBounds(effectiveIgnoreCache, targetHeight, safeInset);
                    }
                };
        mPendingImeEndLayoutObserver = observer;
        mPendingImeEndLayoutObserver.addOnGlobalLayoutListener(mPendingImeEndLayoutListener);
        return true;
    }

    void removePendingImeEndLayoutListener() {
        if (mPendingImeEndLayoutListener != null && mPendingImeEndLayoutObserver != null) {
            if (mPendingImeEndLayoutObserver.isAlive()) {
                mPendingImeEndLayoutObserver.removeOnGlobalLayoutListener(
                        mPendingImeEndLayoutListener);
            }
            mPendingImeEndLayoutListener = null;
            mPendingImeEndLayoutObserver = null;
        }
    }

    @Px
    int resolveTargetBottomSafeInsetPx() {
        if (!isEdgeToEdgeEnabled() || mIsSidePanel) {
            return Delegate.NO_SAFE_AREA_OVERRIDE;
        }
        if (isTargetImeVisible()) {
            return 0;
        }
        return mBottomSheetController != null
                ? mBottomSheetController.getEdgeToEdgeBottomInset()
                : 0;
    }

    private int abortStartIfInactive() {
        cancelImeWatchdog();
        mActiveImeAnimation = null;
        mIsPaused = false;
        mIsClosingAnimation = false;
        mImeInsetAtPreparePx = 0;
        return 0;
    }

    private static boolean isImeAnimation(@Nullable WindowInsetsAnimationCompat animation) {
        return animation != null && (animation.getTypeMask() & WindowInsetsCompat.Type.ime()) != 0;
    }

    @Px
    int getProjectedWebUiHeight() {
        if (mIsSidePanel || mBottomSheetController == null) {
            return 0;
        }
        @Px int projectedContainerHeight = mBottomSheetController.getProjectedContentHeightForIme();
        if (projectedContainerHeight <= 0) {
            return 0;
        }
        return Math.max(
                0, projectedContainerHeight - (isEdgeToEdgeEnabled() ? 0 : mWebUiTopMarginPx));
    }

    boolean isKeyboardOpen() {
        return mIsKeyboardOpen;
    }

    private boolean isViewportSizeFixed() {
        return mDelegate != null && mDelegate.isViewportSizeFixed();
    }
}
