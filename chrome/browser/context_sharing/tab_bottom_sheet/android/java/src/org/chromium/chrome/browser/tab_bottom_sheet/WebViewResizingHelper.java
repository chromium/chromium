// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isActivityInactive;
import static org.chromium.ui.animation.AnimationListeners.onAnimationEnd;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.util.CommonOnLayoutChangeListeners;

import java.util.List;

/** Helper class for showing placeholders while resizing the Web View in the Tab Bottom Sheet. */
@NullMarked
public class WebViewResizingHelper {
    /** Token to unlock/release a requested resize. */
    @FunctionalInterface
    public interface ResizeLock {
        void unlock();
    }

    private static final int RESIZING_ANIMATION_DURATION_MS = 150;
    // Epsilon tolerance in pixels to prevent false size-changed detections caused by DP-to-PX
    // integer rounding.
    private static final int EPSILON_PX = 2;

    private final AnimationHandler mAnimationHandler = new AnimationHandler();

    private final WindowInsetsAnimationListener mInsetAnimationListener =
            new WindowInsetsAnimationListener() {
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

    private final ActivityStateObserver mActivityStateObserver =
            new ActivityStateObserver() {
                @Override
                public void onActivityResumed() {
                    updateBounds(/* ignoreCache= */ true);
                }
            };

    private final Context mContext;
    private final FrameLayout mResizingContainer;
    private final View mResizingPlaceholder;
    private final View mExpandedContentGroup;
    private final WindowAndroid mWindowAndroid;
    private final @Nullable InsetObserver mInsetObserver;
    private final boolean mIsSidePanel;
    private final View mResizingContent;
    private final @Px int mResizingFadeOffset;
    private final @Px int mMinHeight;

    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private boolean mIsViewportSizeFixed;
    private boolean mPauseInsetUpdates;

    /**
     * @param containerView The root view for the co-browse content.
     * @param windowAndroid The WindowAndroid of the activity.
     * @param backgroundColor The background color used for the placeholder.
     */
    public WebViewResizingHelper(
            View containerView, WindowAndroid windowAndroid, @ColorInt int backgroundColor) {
        this(containerView, windowAndroid, backgroundColor, /* isSidePanel= */ false);
    }

    public WebViewResizingHelper(
            View containerView,
            WindowAndroid windowAndroid,
            @ColorInt int backgroundColor,
            boolean isSidePanel) {
        mContext = containerView.getContext();
        mWindowAndroid = windowAndroid;
        mIsSidePanel = isSidePanel;

        mInsetObserver = windowAndroid.getInsetObserver();
        mExpandedContentGroup = containerView.findViewById(R.id.expanded_content_group);

        mResizingContainer = new FrameLayout(mContext);
        mResizingContainer.setClipChildren(true);
        mResizingContainer.addOnLayoutChangeListener(
                CommonOnLayoutChangeListeners.createSizeChangedListener(
                        () -> {
                            if (!mIsViewportSizeFixed) {
                                updateBounds();
                            }
                        }));

        mResizingPlaceholder =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.tab_bottom_sheet_resizing_view, null);
        mResizingContent =
                mResizingPlaceholder.findViewById(R.id.tab_bottom_sheet_resizing_content);
        Resources res = mContext.getResources();
        mResizingFadeOffset =
                res.getDimensionPixelSize(R.dimen.tab_bottom_sheet_resizing_fade_offset);
        mMinHeight = res.getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_height_total);
        mResizingContainer.addView(mResizingPlaceholder);
        mResizingPlaceholder.setVisibility(View.GONE);

        ColorDrawable background = new ColorDrawable();
        background.setColor(backgroundColor);
        mResizingPlaceholder.setBackground(background);

        if (mInsetObserver != null) {
            mInsetObserver.addWindowInsetsAnimationListener(mInsetAnimationListener);
        }
        mWindowAndroid.addActivityStateObserver(mActivityStateObserver);
    }

    /**
     * Updates the height of the resizing placeholder to match the visible height of the sheet.
     *
     * @param visibleHeight The visible height of the sheet in pixels.
     */
    public void updatePlaceholderHeight(@Px int visibleHeight) {
        ViewGroup.LayoutParams params = mResizingPlaceholder.getLayoutParams();
        if (params != null && params.height != visibleHeight) {
            params.height = visibleHeight;
            mResizingPlaceholder.setLayoutParams(params);
        }

        float minHeight = mMinHeight;
        int contentHeight = mResizingContent.getMeasuredHeight();
        float maxHeight = contentHeight + mResizingFadeOffset;

        float alpha;
        if (visibleHeight <= minHeight) {
            alpha = 0.0f;
        } else if (maxHeight <= minHeight || visibleHeight >= maxHeight) {
            alpha = 1.0f;
        } else {
            alpha = (visibleHeight - minHeight) / (maxHeight - minHeight);
        }
        if (mResizingContent.getAlpha() != alpha) {
            mResizingContent.setAlpha(alpha);
        }
    }

    /** Destroys the helper and releases the WebContents. */
    public void destroy() {
        reset();
        mWebContents = null;
        if (mInsetObserver != null) {
            mInsetObserver.removeWindowInsetsAnimationListener(mInsetAnimationListener);
        }
        mWindowAndroid.removeActivityStateObserver(mActivityStateObserver);
    }

    /** Resets the helper to its initial state without resetting the WebContents. */
    public void reset() {
        mResizingContainer.removeAllViews();
        mResizingContainer.addView(mResizingPlaceholder);
        mResizingPlaceholder.setVisibility(View.GONE);
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

        mWebContents = webContents;
        mThinWebView = thinWebView;

        updateBounds();
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

    private void enableResizingMode() {
        if (mThinWebView == null) return;

        View webView = mThinWebView.getView();

        ValueAnimator valueAnimator = ValueAnimator.ofFloat(1.f, 0.f);
        valueAnimator.setDuration(RESIZING_ANIMATION_DURATION_MS);
        valueAnimator.addUpdateListener(
                animator -> {
                    float value = (float) animator.getAnimatedValue();
                    mResizingPlaceholder.setAlpha(1f - value);
                    webView.setAlpha(value);
                });
        valueAnimator.addListener(
                onAnimationEnd(
                        () -> {
                            webView.setVisibility(View.INVISIBLE);
                            webView.setAlpha(1f);
                        }));

        mAnimationHandler.startAnimation(valueAnimator);

        mIsViewportSizeFixed = true;

        mResizingPlaceholder.setVisibility(View.VISIBLE);
        mResizingPlaceholder.setAlpha(0f);
    }

    private void disableResizingMode() {
        if (mThinWebView == null) return;

        View webView = mThinWebView.getView();

        ValueAnimator valueAnimator = ValueAnimator.ofFloat(0.f, 1.f);
        valueAnimator.setDuration(RESIZING_ANIMATION_DURATION_MS);
        valueAnimator.addUpdateListener(
                animator -> {
                    float value = (float) animator.getAnimatedValue();
                    mResizingPlaceholder.setAlpha(1f - value);
                    webView.setAlpha(value);
                });
        valueAnimator.addListener(
                onAnimationEnd(() -> mResizingPlaceholder.setVisibility(View.GONE)));

        mAnimationHandler.startAnimation(valueAnimator);

        mIsViewportSizeFixed = false;
        updateBounds();

        webView.setAlpha(0f);
        webView.setVisibility(View.VISIBLE);
    }

    private void updateBounds() {
        updateBounds(/* ignoreCache= */ false);
    }

    private void updateBounds(boolean ignoreCache) {
        if (mPauseInsetUpdates || isActivityInactive(mWindowAndroid)) {
            return;
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

        if (mWebContents == null || mWebContents.isDestroyed()) {
            return;
        }

        @Px int resizingContainerWidth = mResizingContainer.getMeasuredWidth();
        @Px int resizingContainerHeight = mResizingContainer.getMeasuredHeight();

        // TODO(crbug.com/524719583): Make this feature-agnostic.
        if (mIsSidePanel) {
            if (resizingContainerWidth == 0) {
                resizingContainerWidth =
                        ViewUtils.dpToPx(
                                mContext, SidePanelContainerCoordinator.WIDE_SIDE_PANEL_WIDTH_DP);
            }
            if (resizingContainerHeight == 0) {
                resizingContainerHeight = getDecorViewHeight();
            }
        } else if (resizingContainerWidth == 0 || resizingContainerHeight == 0) {
            return;
        }

        @Px int webContentsWidth = ViewUtils.dpToPx(mContext, mWebContents.getWidth());
        @Px int webContentsHeight = ViewUtils.dpToPx(mContext, mWebContents.getHeight());

        if (!ignoreCache
                && isApproxEqual(resizingContainerWidth, webContentsWidth, EPSILON_PX)
                && isApproxEqual(resizingContainerHeight, webContentsHeight, EPSILON_PX)) {
            return;
        }

        if (mThinWebView != null) {
            mThinWebView.resizeWebContents(resizingContainerWidth, resizingContainerHeight);
        } else {
            mWebContents.setSize(resizingContainerWidth, resizingContainerHeight);
        }
    }

    private static boolean isApproxEqual(int a, int b, int epsilon) {
        return Math.abs(a - b) <= epsilon;
    }
}
