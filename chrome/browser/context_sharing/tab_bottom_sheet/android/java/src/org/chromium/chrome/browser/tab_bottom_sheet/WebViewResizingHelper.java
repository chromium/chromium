// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.ui.animation.AnimationListeners.onAnimationEnd;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.base.WindowAndroid;

/** Helper class for showing placeholders while resizing the Web View in the Tab Bottom Sheet. */
@NullMarked
public class WebViewResizingHelper {
    /** Token to unlock/release a requested resize. */
    @FunctionalInterface
    public interface ResizeLock {
        void unlock();
    }

    private static final int RESIZING_ANIMATION_DURATION_MS = 150;

    private final AnimationHandler mAnimationHandler = new AnimationHandler();

    private final Context mContext;
    private final FrameLayout mResizingContainer;
    private final View mResizingPlaceholder;
    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private final View mExpandedContentGroup;
    private final WindowAndroid mWindowAndroid;

    private boolean mIsViewportSizeFixed;

    /**
     * @param containerView The root view for the co-browse content.
     * @param windowAndroid The WindowAndroid of the activity.
     * @param backgroundColor The background color used for the placeholder.
     */
    public WebViewResizingHelper(
            View containerView, WindowAndroid windowAndroid, @ColorInt int backgroundColor) {
        mContext = containerView.getContext();
        mWindowAndroid = windowAndroid;
        mExpandedContentGroup = containerView.findViewById(R.id.expanded_content_group);

        mResizingContainer = new FrameLayout(mContext);
        mResizingContainer.setClipChildren(true);
        mResizingContainer.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    if (!mIsViewportSizeFixed) {
                        updateBounds();
                    }
                });

        mResizingPlaceholder =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.tab_bottom_sheet_resizing_view, null);
        mResizingContainer.addView(mResizingPlaceholder);
        mResizingPlaceholder.setVisibility(View.GONE);

        ColorDrawable background = new ColorDrawable();
        background.setColor(backgroundColor);
        mResizingPlaceholder.setBackground(background);
    }

    /** Resets the helper to its initial state. */
    public void reset() {
        if (mThinWebView == null) return;
        mResizingContainer.removeAllViews();
        mResizingContainer.addView(mResizingPlaceholder);
        mThinWebView = null;
        mWebContents = null;
    }

    /** Sets the ThinWebView which will be resized. */
    public void setThinWebView(ThinWebView thinWebView, WebContents webContents) {
        reset();
        mThinWebView = thinWebView;
        mWebContents = webContents;

        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, getDecorViewHeight());
        layoutParams.gravity = Gravity.TOP;
        mResizingContainer.addView(mThinWebView.getView(), layoutParams);

        updateBounds();
    }

    private @Px int getDecorViewHeight() {
        Window window = mWindowAndroid.getWindow();
        assert window != null;
        return window.getDecorView().getHeight();
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
        if (mThinWebView == null || mResizingPlaceholder.getVisibility() != View.VISIBLE) {
            return;
        }

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
        if (mThinWebView == null || mWebContents == null) return;

        int newDecorHeight = getDecorViewHeight();
        ViewGroup.LayoutParams params = mThinWebView.getView().getLayoutParams();
        if (params != null && params.height != newDecorHeight) {
            params.height = newDecorHeight;
            mThinWebView.getView().setLayoutParams(params);
        }

        @Px int width = mResizingContainer.getWidth();
        @Px int height = mResizingContainer.getHeight();

        if (width == mWebContents.getWidth()
                && height == mWebContents.getHeight()
                && width != 0
                && height != 0) {
            return;
        }

        mWebContents.setSize(width, height);
    }
}
