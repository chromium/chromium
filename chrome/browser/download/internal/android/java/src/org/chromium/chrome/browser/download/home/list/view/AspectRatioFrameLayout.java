// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.download.internal.R;

/**
 * Helper FrameLayout that knows how to make children fit certain aspect ratios.
 * TODO(dtrainor): Remove this once we get proper support for constraint layout.
 *
 * Below is an example usage to make an ImageView a square based on the width.  Note that you do not
 * need to set both layout_width and layout_height if setting layout_aspectRatio.
 *
 * <org.chromium.chrome.browser.download.home.list.view.AspectRatioFrameLayout
 *     xmlns:android="http://schemas.android.com/apk/res/android"
 *     xmlns:app="http://schemas.android.com/apk/res-auto"
 *     android:layout_width="match_parent"
 *     android:layout_height="wrap_content">
 *     <ImageView
 *         android:layout_width="match_parent"
 *         app:layout_aspectRatio="100%" />
 * </org.chromium.chrome.browser.download.home.list.view.AspectRatioFrameLayout>
 */
public class AspectRatioFrameLayout extends FrameLayout {
    /** Creates an instance of {@link AspectRatioFrameLayout}. */
    public AspectRatioFrameLayout(Context context) {
        this(context, null);
    }

    /** Creates an instance of {@link AspectRatioFrameLayout}. */
    public AspectRatioFrameLayout(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    /** Creates an instance of {@link AspectRatioFrameLayout}. */
    public AspectRatioFrameLayout(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    // FrameLayout implementation.
    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    }

    @Override
    public LayoutParams generateLayoutParams(AttributeSet attrs) {
        return new LayoutParams(getContext(), attrs);
    }

    @Override
    @SuppressWarnings("DrawAllocation")
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width =
                View.MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight();
        int height =
                View.MeasureSpec.getSize(heightMeasureSpec) - getPaddingTop() - getPaddingBottom();

        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);

            if (!(view.getLayoutParams() instanceof LayoutParams)) continue;
            LayoutParams params = (LayoutParams) view.getLayoutParams();

            params.mRestorableWidth = params.mOriginalWidth;
            params.mRestorableHeight = params.mOriginalHeight;

            float aspectRatio = params.aspectRatio;
            if (aspectRatio <= 0.f) continue;

            boolean widthMovable = params.mOverrodeWidth || params.mRestorableWidth == 0;
            boolean heightMovable = params.mOverrodeHeight || params.mRestorableHeight == 0;

            if (widthMovable) {
                int childHeight =
                        params.height == LayoutParams.MATCH_PARENT ? height : params.height;
                params.width = Math.round(childHeight * aspectRatio);
                params.mOverrodeWidth = true;
            }
            if (heightMovable) {
                int childWidth = params.width == LayoutParams.MATCH_PARENT ? width : params.width;
                params.height = Math.round(childWidth / aspectRatio);
                params.mOverrodeHeight = true;
            }
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);

            if (!(view.getLayoutParams() instanceof LayoutParams)) continue;
            LayoutParams params = (LayoutParams) view.getLayoutParams();
            if (params.mOverrodeWidth) params.width = params.mRestorableWidth;
            if (params.mOverrodeHeight) params.height = params.mRestorableHeight;
            params.mOverrodeWidth = false;
            params.mOverrodeHeight = false;
        }
    }

    /** A set of layout parameters for {@link AspectRatioFrameLayout}. */
    public static class LayoutParams extends FrameLayout.LayoutParams {
        /** The aspect ratio to use and enforce. */
        public float aspectRatio;

        // Restorable parameters.
        private boolean mOverrodeWidth;
        private boolean mOverrodeHeight;
        private int mRestorableWidth;
        private int mRestorableHeight;
        private int mOriginalWidth;
        private int mOriginalHeight;

        public LayoutParams(Context context, AttributeSet attrs) {
            super(context, attrs);

            TypedArray array =
                    context.obtainStyledAttributes(
                            attrs, R.styleable.AspectRatioFrameLayout_Layout);
            aspectRatio =
                    array.getFraction(
                            R.styleable.AspectRatioFrameLayout_Layout_layout_aspectRatio,
                            1,
                            1,
                            0.f);
            array.recycle();
        }

        public LayoutParams(int width, int height) {
            super(width, height);
        }

        public LayoutParams(ViewGroup.LayoutParams source) {
            super(source);
        }

        public LayoutParams(MarginLayoutParams source) {
            super(source);
        }

        public LayoutParams(FrameLayout.LayoutParams source) {
            super((MarginLayoutParams) source);
            gravity = source.gravity;
        }

        @Override
        protected void setBaseAttributes(TypedArray a, int widthAttr, int heightAttr) {
            // Do not throw errors if width or height aren't supplied.
            width = a.getLayoutDimension(widthAttr, 0);
            height = a.getLayoutDimension(heightAttr, 0);
            mOriginalWidth = width;
            mOriginalHeight = height;
        }
    }
}
