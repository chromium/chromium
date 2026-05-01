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
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.ui.animation.AnimationHandler;

/** Helper class for showing placeholders while resizing the Web View in the Tab Bottom Sheet. */
@NullMarked
public class WebViewResizingHelper {
    private static final int RESIZING_ANIMATION_DURATION_MS = 150;

    private final AnimationHandler mAnimationHandler = new AnimationHandler();

    private final Context mContext;
    private final FrameLayout mResizingContainer;
    private final View mResizingPlaceholder;
    private @Nullable ThinWebView mThinWebView;
    private final View mExpandedContentGroup;

    /**
     * @param containerView The root view for the co-browse content.
     * @param backgroundColor The background color used for the placeholder.
     */
    public WebViewResizingHelper(View containerView, @ColorInt int backgroundColor) {
        mContext = containerView.getContext();
        mExpandedContentGroup = containerView.findViewById(R.id.expanded_content_group);

        mResizingContainer = new FrameLayout(mContext);
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
    }

    /** Sets the ThinWebView which will be resized. */
    public void setThinWebView(ThinWebView thinWebView) {
        reset();
        mThinWebView = thinWebView;
        makeWebViewResizable();

        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        layoutParams.gravity = Gravity.BOTTOM;
        mResizingContainer.addView(mThinWebView.getView(), layoutParams);
    }

    /** Returns the resizing container. This holds the ThinWebView and the placeholder. */
    public View getResizingContainer() {
        return mResizingContainer;
    }

    /**
     * Sets whether the web view is resizing. If true, the placeholder will be shown. If false, the
     * placeholder will be hidden.
     */
    public void setIsResizing(boolean isResizing) {
        if (mThinWebView == null) return;

        if (isResizing) {
            enableResizingMode();
        } else {
            disableResizingMode();
        }
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
        assert mThinWebView != null;

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

        makeWebViewFixedSize();

        mResizingPlaceholder.setVisibility(View.VISIBLE);
        mResizingPlaceholder.setAlpha(0f);
    }

    private void disableResizingMode() {
        assert mThinWebView != null;
        if (mResizingPlaceholder.getVisibility() != View.VISIBLE) return;

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

        makeWebViewResizable();

        webView.setAlpha(0f);
        webView.setVisibility(View.VISIBLE);
    }

    private void makeWebViewFixedSize() {
        assert mThinWebView != null;
        View webView = mThinWebView.getView();
        ViewGroup.LayoutParams params = webView.getLayoutParams();
        if (params != null) {
            params.height = webView.getHeight();
            webView.setLayoutParams(params);
        }
    }

    private void makeWebViewResizable() {
        assert mThinWebView != null;
        View webView = mThinWebView.getView();
        ViewGroup.LayoutParams params = webView.getLayoutParams();
        if (params != null) {
            params.height = ViewGroup.LayoutParams.MATCH_PARENT;
            webView.setLayoutParams(params);
        }
    }
}
