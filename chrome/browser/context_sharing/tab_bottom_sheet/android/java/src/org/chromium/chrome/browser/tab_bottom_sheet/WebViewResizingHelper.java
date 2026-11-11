// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isActivityInactive;
import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isEdgeToEdgeEnabled;
import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isSmoothKeyboardResizeEnabled;
import static org.chromium.ui.animation.AnimationListeners.onAnimationEnd;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Rect;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.util.CommonOnLayoutChangeListeners;
import org.chromium.ui.util.TimeoutRunnable;

import java.util.List;

/** Helper class for showing placeholders while resizing the Web View in the Tab Bottom Sheet. */
@NullMarked
public class WebViewResizingHelper implements ImeResizePredictor.Delegate {
    /** Token to unlock/release a requested resize. */
    @FunctionalInterface
    public interface ResizeLock {
        void unlock();
    }

    private static final int RESIZING_ANIMATION_DURATION_MS = 150;
    private static final int TIMEOUT_RUNNABLE_TIMEOUT_MS = 150;
    private static final int FADE_ANIMATION_DELAY_MS = 250;
    // Epsilon tolerance in pixels to prevent false size-changed detections caused by DP-to-PX
    // integer rounding.
    private static final int EPSILON_PX = 2;
    public static final int NO_HEIGHT_OVERRIDE = -1;
    public static final int NO_SAFE_AREA_OVERRIDE = -1;

    private final AnimationHandler mAnimationHandler = new AnimationHandler();
    private final WindowInsetsAnimationListener mInsetAnimationListener;
    private final ActivityStateObserver mActivityStateObserver;
    private final WindowInsetObserver mWindowInsetObserver;
    private final BottomSheetObserver mBottomSheetObserver;
    private final @Nullable ImeResizePredictor mImePredictor;

    private final Context mContext;
    private final View mContainerView;
    private final FrameLayout mResizingContainer;
    private final View mExpandedContentGroup;
    private final WindowAndroid mWindowAndroid;
    private final @Nullable BottomSheetController mBottomSheetController;
    private final @Nullable InsetObserver mInsetObserver;
    private final boolean mIsSidePanel;
    private final @Px int mWebUiTopMarginPx;
    private final Rect mLastSafeAreaInsets = new Rect(0, 0, 0, -1);
    private final View.OnLayoutChangeListener mOnLayoutChangeListener;
    private final @Nullable CoBrowseComponentProvider mComponentProvider;
    private final @ColorInt int mBackgroundColor;
    private final @ColorInt int mPlaceholderElemColor;

    private @Nullable View mResizingPlaceholder;
    private @Nullable ResizingPlaceholderCoordinator mPlaceholderCoordinator;
    private boolean mPlaceholderInitialized;
    private @Px int mLastPlaceholderHeight = -1;
    private @Px int mLastPlaceholderBottomInsetPx = -1;
    private @Nullable Callback<Integer> mBottomSafeAreaInsetObserver;
    private @Px int mLastObservedBottomInsetPx = -1;
    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private boolean mIsViewportSizeFixed;
    private boolean mPauseInsetUpdates;
    private @Nullable TimeoutRunnable mPendingTimeoutRunnable;
    private boolean mAwaitingDeferredFadeIn;

    /**
     * Constructs a {@link WebViewResizingHelper} with a nullable {@link BottomSheetController} and
     * {@link CoBrowseComponentProvider}.
     *
     * @param containerView The parent view containing the bottom sheet WebUI.
     * @param windowAndroid The window hosting the sheet.
     * @param bottomSheetController The controller for the bottom sheet, or null.
     * @param backgroundColor The background color of the placeholder.
     * @param placeholderElemColor The element color of the placeholder skeleton items.
     * @param isSidePanel Whether the container is hosted in a side panel.
     * @param componentProvider Provider for custom co-browse components, or null.
     */
    public WebViewResizingHelper(
            View containerView,
            WindowAndroid windowAndroid,
            @Nullable BottomSheetController bottomSheetController,
            @ColorInt int backgroundColor,
            @ColorInt int placeholderElemColor,
            boolean isSidePanel,
            @Nullable CoBrowseComponentProvider componentProvider) {
        mContext = containerView.getContext();
        mContainerView = containerView;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mIsSidePanel = isSidePanel;
        mComponentProvider = componentProvider;
        mBackgroundColor = backgroundColor;
        mPlaceholderElemColor = placeholderElemColor;
        mWebUiTopMarginPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_web_ui_top_margin);

        mInsetObserver = windowAndroid.getInsetObserver();
        mExpandedContentGroup = containerView.findViewById(R.id.expanded_content_group);

        mResizingContainer = new FrameLayout(mContext);
        mResizingContainer.setClipChildren(true);

        mImePredictor =
                !mIsSidePanel && isSmoothKeyboardResizeEnabled()
                        ? new ImeResizePredictor(
                                mContainerView,
                                mResizingContainer,
                                mWindowAndroid,
                                mInsetObserver,
                                mBottomSheetController,
                                mIsSidePanel,
                                mWebUiTopMarginPx,
                                this)
                        : null;
        mOnLayoutChangeListener =
                CommonOnLayoutChangeListeners.createSizeChangedListener(
                        () -> {
                            if (!mIsViewportSizeFixed) {
                                updateBounds();
                            }
                        });
        mResizingContainer.addOnLayoutChangeListener(mOnLayoutChangeListener);

        mInsetAnimationListener = createInsetAnimationListener();
        mActivityStateObserver = createActivityStateObserver();
        mWindowInsetObserver = createWindowInsetObserver();
        mBottomSheetObserver = createBottomSheetObserver();

        if (mInsetObserver != null) {
            mInsetObserver.addWindowInsetsAnimationListener(mInsetAnimationListener);
        }
        if (!mIsSidePanel && (isEdgeToEdgeEnabled() || isSmoothKeyboardResizeEnabled())) {
            if (mInsetObserver != null) {
                mInsetObserver.addObserver(mWindowInsetObserver);
            }
            if (mBottomSheetController != null) {
                mBottomSheetController.addObserver(mBottomSheetObserver);
            }
        }
        mWindowAndroid.addActivityStateObserver(mActivityStateObserver);
    }

    /**
     * Updates the height of the resizing placeholder to match the visible height of the sheet.
     *
     * @param visibleHeight The visible height of the sheet in pixels.
     */
    public void updatePlaceholderHeight(@Px int visibleHeight) {
        mLastPlaceholderHeight = Math.max(0, visibleHeight);
        if (mPlaceholderCoordinator != null) {
            mPlaceholderCoordinator.updateVisibleHeight(visibleHeight);
        }
    }

    private void ensurePlaceholderCoordinator() {
        if (mPlaceholderInitialized) {
            return;
        }
        mPlaceholderInitialized = true;
        if (mComponentProvider == null) {
            return;
        }
        mPlaceholderCoordinator =
                mComponentProvider.createResizingPlaceholderCoordinator(
                        mContext, mBackgroundColor, mPlaceholderElemColor);
        if (mPlaceholderCoordinator != null) {
            mResizingPlaceholder = mPlaceholderCoordinator.getView();
            if (mResizingPlaceholder != null) {
                mResizingPlaceholder.setVisibility(View.INVISIBLE);
                mResizingPlaceholder.setAlpha(1f);
                mResizingContainer.addView(mResizingPlaceholder, /* index= */ 0);
            }
            if (isEdgeToEdgeEnabled() && !mIsSidePanel) {
                @Px
                int bottomInsetPx =
                        mLastPlaceholderBottomInsetPx >= 0
                                ? mLastPlaceholderBottomInsetPx
                                : getBottomSafeAreaInsetPx();
                mLastPlaceholderBottomInsetPx = bottomInsetPx;
                mPlaceholderCoordinator.updateBottomInset(bottomInsetPx);
            }
            if (mLastPlaceholderHeight >= 0) {
                mPlaceholderCoordinator.updateVisibleHeight(mLastPlaceholderHeight);
            }
        }
    }

    /** Destroys the helper and releases the WebContents. */
    public void destroy() {
        if (mPlaceholderCoordinator != null) {
            mPlaceholderCoordinator.destroy();
        }
        mLastPlaceholderHeight = -1;
        mLastPlaceholderBottomInsetPx = -1;
        mLastSafeAreaInsets.set(0, 0, 0, -1);
        mBottomSafeAreaInsetObserver = null;
        resetInternal(/* reattachPlaceholder= */ false);
        mWebContents = null;
        if (mInsetObserver != null) {
            mInsetObserver.removeWindowInsetsAnimationListener(mInsetAnimationListener);
            mInsetObserver.removeObserver(mWindowInsetObserver);
        }
        if (!mIsSidePanel && mBottomSheetController != null) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
        if (mImePredictor != null) {
            mImePredictor.destroy();
        }
        mWindowAndroid.removeActivityStateObserver(mActivityStateObserver);
        mResizingContainer.removeOnLayoutChangeListener(mOnLayoutChangeListener);
    }

    /** Resets the helper to its initial state without resetting the WebContents. */
    public void reset() {
        resetInternal(/* reattachPlaceholder= */ true);
        updateSafeAreaInsets(/* ignoreCache= */ false);
    }

    private void resetInternal(boolean reattachPlaceholder) {
        if (mPlaceholderCoordinator != null) {
            mPlaceholderCoordinator.setIsResizing(false);
        }
        if (mPendingTimeoutRunnable != null) {
            mPendingTimeoutRunnable.cancel();
            mPendingTimeoutRunnable = null;
        }
        mAnimationHandler.forceFinishAnimation();
        mResizingContainer.removeAllViews();
        if (mResizingPlaceholder != null) {
            mResizingPlaceholder.setVisibility(View.INVISIBLE);
            mResizingPlaceholder.setAlpha(1f);
            if (reattachPlaceholder) {
                mResizingContainer.addView(mResizingPlaceholder, /* index= */ 0);
            }
        }
        if (mImePredictor != null) {
            mImePredictor.reset();
        }
        mAwaitingDeferredFadeIn = false;
        mThinWebView = null;
        mIsViewportSizeFixed = false;
        mPauseInsetUpdates = false;
    }

    /** Sets the ThinWebView and WebContents which will be resized. */
    public void setThinWebView(
            @Nullable ThinWebView thinWebView, @Nullable WebContents webContents) {
        reset();

        if (thinWebView != null && mThinWebView != thinWebView) {
            // Use MATCH_PARENT so the initial layout uses the container height instead of the
            // decor height, which can cause incorrect sizing.
            FrameLayout.LayoutParams layoutParams =
                    new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT);
            layoutParams.gravity = Gravity.TOP;
            mResizingContainer.addView(thinWebView.getView(), layoutParams);
        }

        mLastSafeAreaInsets.set(0, 0, 0, -1);
        mWebContents = webContents;
        mThinWebView = thinWebView;

        updateBounds();
    }

    /**
     * Sets the observer of the WebUI's bottom safe area inset, in px. In edge-to-edge bottom
     * sheets, it is called now and when the inset changes. Pass null to clear.
     */
    void setBottomSafeAreaInsetObserver(@Nullable Callback<Integer> observer) {
        mBottomSafeAreaInsetObserver = observer;
        mLastObservedBottomInsetPx = -1;
        if (observer == null || !isEdgeToEdgeEnabled() || mIsSidePanel) return;
        mLastObservedBottomInsetPx = getBottomSafeAreaInsetPx();
        observer.onResult(mLastObservedBottomInsetPx);
    }

    private void updateSafeAreaInsets(boolean ignoreCache) {
        @Px
        int bottomSafeAreaOverridePx =
                mImePredictor != null
                        ? mImePredictor.resolveTargetBottomSafeInsetPx()
                        : NO_SAFE_AREA_OVERRIDE;
        updateSafeAreaInsets(ignoreCache, bottomSafeAreaOverridePx);
    }

    /**
     * Updates safe area insets on WebContents and placeholder views, optionally overriding the
     * bottom safe area inset.
     *
     * @param ignoreCache Whether to bypass the cached inset check.
     * @param bottomSafeAreaOverridePx Explicit bottom safe area inset override in pixels, or {@link
     *     #NO_SAFE_AREA_OVERRIDE} to use the default inset from window insets.
     */
    @Override
    public void updateSafeAreaInsets(boolean ignoreCache, @Px int bottomSafeAreaOverridePx) {
        if (!isEdgeToEdgeEnabled() || mIsSidePanel || isActivityInactive(mWindowAndroid)) {
            return;
        }

        @Px
        int bottomInsetPx =
                bottomSafeAreaOverridePx >= 0
                        ? bottomSafeAreaOverridePx
                        : getBottomSafeAreaInsetPx();
        if (mPlaceholderCoordinator != null
                && (ignoreCache || mLastPlaceholderBottomInsetPx != bottomInsetPx)) {
            mLastPlaceholderBottomInsetPx = bottomInsetPx;
            mPlaceholderCoordinator.updateBottomInset(bottomInsetPx);
        }
        if (mBottomSafeAreaInsetObserver != null
                && (ignoreCache || mLastObservedBottomInsetPx != bottomInsetPx)) {
            mLastObservedBottomInsetPx = bottomInsetPx;
            mBottomSafeAreaInsetObserver.onResult(bottomInsetPx);
        }

        if (mWebContents == null || mWebContents.isDestroyed()) {
            return;
        }

        float dipScale = mWindowAndroid.getDisplay().getDipScale();
        int bottomInsetDip = (int) Math.ceil(bottomInsetPx / dipScale);
        if (!ignoreCache && mLastSafeAreaInsets.bottom == bottomInsetDip) {
            return;
        }

        int topInsetDip = (int) Math.ceil(mWebUiTopMarginPx / dipScale);
        mLastSafeAreaInsets.set(0, topInsetDip, 0, bottomInsetDip);
        mWebContents.setDisplayCutoutSafeArea(mLastSafeAreaInsets);
    }

    private @Px int getBottomSafeAreaInsetPx() {
        if (mBottomSheetController == null) return 0;
        return mBottomSheetController.getEdgeToEdgeBottomInset();
    }

    private @Px int getDecorViewHeight() {
        Window window = mWindowAndroid.getWindow();
        if (window == null) {
            return 0;
        }
        return window.getDecorView().getHeight();
    }

    private @Px int getDecorViewWidth() {
        Window window = mWindowAndroid.getWindow();
        if (window == null) {
            return 0;
        }
        return window.getDecorView().getWidth();
    }

    /** Returns the resizing container. This holds the ThinWebView and the placeholder. */
    public View getResizingContainer() {
        return mResizingContainer;
    }

    /** Requests resizing mode and shows the placeholder. */
    public @Nullable ResizeLock requestResize() {
        if (mThinWebView == null) return null;

        enableResizingMode();
        return this::disableResizingMode;
    }

    /** Sets the sheet to flexible height. */
    public void setToFlexibleHeight() {
        ViewGroup.LayoutParams sheetContentParams = mExpandedContentGroup.getLayoutParams();
        if (sheetContentParams.height != ViewGroup.LayoutParams.MATCH_PARENT) {
            sheetContentParams.height = ViewGroup.LayoutParams.MATCH_PARENT;
            mExpandedContentGroup.setLayoutParams(sheetContentParams);
        }
    }

    /**
     * Sets the sheet to fixed height.
     *
     * @param height The height to set.
     */
    public void setToFixedHeight(int height) {
        ViewGroup.LayoutParams sheetContentParams = mExpandedContentGroup.getLayoutParams();
        if (sheetContentParams.height != height) {
            sheetContentParams.height = height;
            mExpandedContentGroup.setLayoutParams(sheetContentParams);
        }
    }

    @Override
    public boolean isViewportSizeFixed() {
        return mIsViewportSizeFixed;
    }

    @Override
    public void onPostResizeReconciled(boolean sizeChanged) {
        if (mAwaitingDeferredFadeIn) {
            mAwaitingDeferredFadeIn = false;
            armPostResizeFadeIn(sizeChanged);
        }
    }

    @Override
    public void restoreVisibilityIfAwaitingFadeIn() {
        if (mAwaitingDeferredFadeIn) {
            mAwaitingDeferredFadeIn = false;
            if (mThinWebView != null) {
                View webView = mThinWebView.getView();
                webView.setAlpha(1f);
                webView.setVisibility(View.VISIBLE);
            }
            if (mResizingPlaceholder != null) {
                mResizingPlaceholder.setVisibility(View.INVISIBLE);
                mResizingPlaceholder.setAlpha(1f);
            }
        }
    }

    private void enableResizingMode() {
        if (mThinWebView == null) return;

        ensurePlaceholderCoordinator();

        View webView = mThinWebView.getView();

        ValueAnimator valueAnimator = ValueAnimator.ofFloat(1.f, 0.f);
        valueAnimator.setDuration(RESIZING_ANIMATION_DURATION_MS);
        valueAnimator.addUpdateListener(
                animator -> {
                    float value = (float) animator.getAnimatedValue();
                    if (mResizingPlaceholder != null) {
                        mResizingPlaceholder.setAlpha(1f - value);
                    }
                    webView.setAlpha(value);
                });
        valueAnimator.addListener(
                onAnimationEnd(
                        () -> {
                            if (!mIsViewportSizeFixed) return;
                            webView.setVisibility(View.INVISIBLE);
                            webView.setAlpha(1f);
                        }));

        mAnimationHandler.startAnimation(valueAnimator);

        mIsViewportSizeFixed = true;
        if (mImePredictor != null) {
            mImePredictor.removePendingImeEndLayoutListener();
        }
        mAwaitingDeferredFadeIn = false;
        if (mPlaceholderCoordinator != null) {
            mPlaceholderCoordinator.setIsResizing(true);
        }

        if (mResizingPlaceholder != null) {
            mResizingPlaceholder.setVisibility(View.VISIBLE);
            mResizingPlaceholder.setAlpha(0f);
        }
    }

    private void disableResizingMode() {
        if (mThinWebView == null) return;

        mIsViewportSizeFixed = false;
        if (mPlaceholderCoordinator != null) {
            mPlaceholderCoordinator.setIsResizing(false);
        }
        mAnimationHandler.forceFinishAnimation();

        if (mImePredictor != null) {
            mAwaitingDeferredFadeIn = true;
            mImePredictor.onDisableResizingMode();
            return;
        }

        boolean sizeChanged = updateBounds(/* ignoreCache= */ true);
        armPostResizeFadeIn(sizeChanged);
    }

    private void armPostResizeFadeIn(boolean sizeChanged) {
        if (mThinWebView == null) return;

        View webView = mThinWebView.getView();
        webView.setAlpha(0f);
        webView.setVisibility(View.VISIBLE);

        if (sizeChanged) {
            mPendingTimeoutRunnable =
                    new TimeoutRunnable(
                            this::onNextFrameAfterResize,
                            this::onNextFrameAfterResize,
                            TIMEOUT_RUNNABLE_TIMEOUT_MS);
            mPendingTimeoutRunnable.startTimeout();
            mThinWebView.runOnNextFrame(mPendingTimeoutRunnable);
        } else {
            onNextFrameAfterResize();
        }
    }

    private void onNextFrameAfterResize() {
        if (mThinWebView == null || mIsViewportSizeFixed) return;

        View webView = mThinWebView.getView();

        ValueAnimator valueAnimator = ValueAnimator.ofFloat(0.f, 1.f);
        // TODO(crbug.com/540352621): Fix this via a synchronization mechanism between the WebUI and
        // the Android UI.
        // Delay to allow CSS/JS layout pass and reflow to settle.
        valueAnimator.setStartDelay(FADE_ANIMATION_DELAY_MS);
        valueAnimator.setDuration(RESIZING_ANIMATION_DURATION_MS);
        valueAnimator.addUpdateListener(
                animator -> {
                    float value = (float) animator.getAnimatedValue();
                    if (mResizingPlaceholder != null) {
                        mResizingPlaceholder.setAlpha(1f - value);
                    }
                    webView.setAlpha(value);
                });
        valueAnimator.addListener(
                onAnimationEnd(
                        () -> {
                            if (mResizingPlaceholder != null) {
                                mResizingPlaceholder.setVisibility(View.INVISIBLE);
                            }
                            webView.setVisibility(View.VISIBLE);
                            webView.setAlpha(1f);
                        }));

        mAnimationHandler.startAnimation(valueAnimator);
    }

    /**
     * Called when focus enters or leaves the WebContents' view. In the side panel, the WebContents
     * only shrinks for the keyboard while it has focus, so the bounds need to be recomputed.
     */
    public void onFocusChanged() {
        if (mIsSidePanel && !mIsViewportSizeFixed) {
            updateBounds();
        }
    }

    private boolean updateBounds() {
        return updateBounds(/* ignoreCache= */ false);
    }

    private boolean updateBounds(boolean ignoreCache) {
        if (mImePredictor != null) {
            if (mImePredictor.isPaused()
                    || mImePredictor.isAnimationRunning()
                    || mImePredictor.isPendingLayoutListenerAttached()) {
                return false;
            }
        }
        @Px
        int bottomSafeAreaOverridePx =
                mImePredictor != null
                        ? mImePredictor.resolveTargetBottomSafeInsetPx()
                        : NO_SAFE_AREA_OVERRIDE;
        if (mImePredictor != null && mIsViewportSizeFixed) {
            if (!mPauseInsetUpdates && !isActivityInactive(mWindowAndroid)) {
                updateSafeAreaInsets(ignoreCache, bottomSafeAreaOverridePx);
            }
            return false;
        }
        return updateBounds(ignoreCache, NO_HEIGHT_OVERRIDE, bottomSafeAreaOverridePx);
    }

    /**
     * Updates container bounds, optionally applying height and bottom safe-area overrides.
     *
     * @param ignoreCache Whether to ignore the previous dimension cache.
     * @param heightOverridePx Explicit height override in pixels, or {@link #NO_HEIGHT_OVERRIDE} to
     *     use measured container height.
     * @param bottomSafeAreaOverridePx Explicit bottom safe area inset override in pixels, or {@link
     *     #NO_SAFE_AREA_OVERRIDE} to use the default inset from window insets.
     * @return True if container dimensions changed, false otherwise.
     */
    @Override
    public boolean updateBounds(
            boolean ignoreCache, @Px int heightOverridePx, @Px int bottomSafeAreaOverridePx) {
        if (mPauseInsetUpdates || isActivityInactive(mWindowAndroid)) {
            return false;
        }

        @Px int resizingContainerWidth = mResizingContainer.getMeasuredWidth();
        @Px
        int resizingContainerHeight =
                heightOverridePx != NO_HEIGHT_OVERRIDE
                        ? heightOverridePx
                        : mResizingContainer.getMeasuredHeight();

        // TODO(crbug.com/524719583): Make this feature-agnostic.
        if (mIsSidePanel) {
            if (resizingContainerWidth == 0) {
                resizingContainerWidth =
                        ViewUtils.dpToPx(
                                mContext, SidePanelContainerCoordinator.WIDE_SIDE_PANEL_WIDTH_DP);
            }
            if (heightOverridePx == NO_HEIGHT_OVERRIDE) {
                if (resizingContainerHeight == 0) {
                    resizingContainerHeight = getDecorViewHeight();
                } else if (!hasWebContentsFocus()) {
                    // The keyboard was opened by something else (the page or another app in split
                    // screen). Keep the WebContents at its no-keyboard height and let the container
                    // clip it, rather than pushing the Glic UI up.
                    resizingContainerHeight += getKeyboardInsetPx();
                }
            }
        }

        if (mThinWebView != null) {
            @Px int newDecorHeight = getDecorViewHeight();
            @Px int newDecorWidth = getDecorViewWidth();
            ViewGroup.LayoutParams params = mThinWebView.getView().getLayoutParams();
            if (params != null
                    && (params.height != newDecorHeight || params.width != newDecorWidth)) {
                params.height = newDecorHeight;
                params.width = newDecorWidth;
                mThinWebView.getView().setLayoutParams(params);
            }
        }

        updateSafeAreaInsets(ignoreCache, bottomSafeAreaOverridePx);

        if (mWebContents == null || mWebContents.isDestroyed()) {
            return false;
        }

        if (resizingContainerWidth <= 0 || resizingContainerHeight <= 0) {
            return false;
        }

        @Px int currentWidth = ViewUtils.dpToPx(mContext, mWebContents.getWidth());
        @Px int currentHeight = ViewUtils.dpToPx(mContext, mWebContents.getHeight());

        boolean sizeChanged =
                !isApproxEqual(resizingContainerWidth, currentWidth, EPSILON_PX)
                        || !isApproxEqual(resizingContainerHeight, currentHeight, EPSILON_PX);

        if (!ignoreCache && !sizeChanged) {
            return false;
        }

        if (mThinWebView != null) {
            mThinWebView.resizeWebContents(resizingContainerWidth, resizingContainerHeight);
        } else {
            mWebContents.setSize(resizingContainerWidth, resizingContainerHeight);
        }

        return sizeChanged;
    }

    private boolean hasWebContentsFocus() {
        if (mThinWebView != null) {
            return mThinWebView.getView().hasFocus();
        }
        if (mWebContents != null && mWebContents.getViewAndroidDelegate() != null) {
            View containerView = mWebContents.getViewAndroidDelegate().getContainerView();
            return containerView != null && containerView.hasFocus();
        }
        return false;
    }

    /**
     * Returns the keyboard height that shrinks the side panel container. Mirrors {@code
     * ApplicationViewportInsetTracker#computeInsets()}: in overlay mode the IME inset is consumed
     * and doesn't shrink the container. The accessory bar isn't included because it only insets the
     * CompositorViewHolder.
     */
    private @Px int getKeyboardInsetPx() {
        if (mInsetObserver == null || mInsetObserver.isKeyboardInOverlayMode()) return 0;
        return mInsetObserver.getSupplierForKeyboardInset().get();
    }

    private WindowInsetsAnimationListener createInsetAnimationListener() {
        if (mImePredictor != null) {
            return createSmoothResizeInsetAnimationListener();
        }
        return new WindowInsetsAnimationListener() {
            @Override
            public void onPrepare(WindowInsetsAnimationCompat animation) {
                mPauseInsetUpdates = true;
            }

            @Override
            public void onStart(
                    WindowInsetsAnimationCompat animation,
                    WindowInsetsAnimationCompat.BoundsCompat bounds) {}

            @Override
            public void onProgress(
                    WindowInsetsCompat insets, List<WindowInsetsAnimationCompat> list) {}

            @Override
            public void onEnd(WindowInsetsAnimationCompat animation) {
                mPauseInsetUpdates = false;
                updateBounds();
            }
        };
    }

    private ActivityStateObserver createActivityStateObserver() {
        if (mImePredictor != null) {
            return createSmoothResizeActivityStateObserver();
        }
        return new ActivityStateObserver() {
            @Override
            public void onActivityResumed() {
                updateBounds(/* ignoreCache= */ true);
            }
        };
    }

    private WindowInsetObserver createWindowInsetObserver() {
        if (mImePredictor != null) {
            return createSmoothResizeWindowInsetObserver();
        }
        return new WindowInsetObserver() {
            @Override
            public void onInsetChanged() {
                updateBounds();
            }

            @Override
            public void onKeyboardInsetChanged(int inset) {
                updateBounds();
            }
        };
    }

    private BottomSheetObserver createBottomSheetObserver() {
        if (mImePredictor != null) {
            return createSmoothResizeBottomSheetObserver();
        }
        return new BottomSheetObserver() {
            @Override
            public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                updateBounds();
            }

            @Override
            public void onContainerBottomMarginChanged(@Px int bottomMargin) {
                updateBounds();
            }
        };
    }

    private WindowInsetsAnimationListener createSmoothResizeInsetAnimationListener() {
        return new WindowInsetsAnimationListener() {
            @Override
            public void onPrepare(WindowInsetsAnimationCompat animation) {
                if (mImePredictor != null) {
                    mImePredictor.onPrepareAnimation(animation);
                }
            }

            @Override
            public void onStart(
                    WindowInsetsAnimationCompat animation,
                    WindowInsetsAnimationCompat.BoundsCompat bounds) {
                if (mImePredictor != null) {
                    mImePredictor.onStartAnimation(animation, bounds);
                }
            }

            @Override
            public void onProgress(
                    WindowInsetsCompat insets, List<WindowInsetsAnimationCompat> list) {
                if (mImePredictor != null) {
                    mImePredictor.onProgressAnimation(list);
                }
            }

            @Override
            public void onEnd(WindowInsetsAnimationCompat animation) {
                if (mImePredictor != null) {
                    mImePredictor.onEndAnimation(animation);
                }
            }
        };
    }

    private ActivityStateObserver createSmoothResizeActivityStateObserver() {
        return new ActivityStateObserver() {
            @Override
            public void onActivityResumed() {
                if (mImePredictor != null) {
                    mImePredictor.onActivityResumed();
                }
            }
        };
    }

    private WindowInsetObserver createSmoothResizeWindowInsetObserver() {
        return new WindowInsetObserver() {
            @Override
            public void onInsetChanged() {
                if (mImePredictor != null && mImePredictor.isOtherSheetContentShowing()) {
                    return;
                }
                updateBounds();
            }

            @Override
            public void onKeyboardInsetChanged(int inset) {
                if (mImePredictor != null) {
                    mImePredictor.onKeyboardInsetChanged();
                }
                if (!isEdgeToEdgeEnabled()) {
                    if (mImePredictor != null && mImePredictor.isOtherSheetContentShowing()) {
                        return;
                    }
                    updateBounds();
                }
            }
        };
    }

    private BottomSheetObserver createSmoothResizeBottomSheetObserver() {
        return new BottomSheetObserver() {
            @Override
            public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                if (mImePredictor != null) {
                    mImePredictor.onSheetContentChanged();
                }
            }

            @Override
            public void onSheetStateChanged(int newState, int reason) {
                if (mImePredictor != null) {
                    mImePredictor.onSheetStateChanged(newState);
                }
            }

            @Override
            public void onContainerBottomMarginChanged(@Px int bottomMargin) {
                if (mImePredictor != null
                        && (mImePredictor.shouldSuppressBottomMarginUpdate()
                                || mImePredictor.isOtherSheetContentShowing())) {
                    return;
                }
                updateBounds();
            }
        };
    }

    private static boolean isApproxEqual(int a, int b, int epsilon) {
        return Math.abs(a - b) <= epsilon;
    }

    /** Returns the {@link CoBrowseComponentProvider}. */
    @Nullable CoBrowseComponentProvider getComponentProviderForTesting() {
        return mComponentProvider;
    }

    /** Returns the {@link ResizingPlaceholderCoordinator} managing the placeholder view. */
    @Nullable ResizingPlaceholderCoordinator getPlaceholderCoordinator() {
        return mPlaceholderCoordinator;
    }

    @Nullable TimeoutRunnable getPendingTimeoutRunnableForTesting() {
        return mPendingTimeoutRunnable;
    }

    WindowInsetObserver getWindowInsetObserverForTesting() {
        return mWindowInsetObserver;
    }
}
