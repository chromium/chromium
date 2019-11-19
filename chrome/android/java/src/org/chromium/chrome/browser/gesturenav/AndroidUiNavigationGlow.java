// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Canvas;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EdgeEffect;

/**
 * Handles glow effect using {@link EdgeEffect} API.
 */
public class AndroidUiNavigationGlow extends NavigationGlow {
    /**
     * Amount of time we wait before {@link GlowView} gets detached from parent view
     * after the glow effect is completed.
     */
    private final static int REMOVE_RUNNABLE_DELAY_MS = 500;

    private final Runnable mRemoveGlowViewRunnable;

    /**
     * {@link android.view.View} where the glow effect is rendered. Stays attached to view
     * hierarchy while rendering is in progress only.
     */
    private final GlowView mGlowView;

    public AndroidUiNavigationGlow(ViewGroup parentView) {
        super(parentView);
        mGlowView = new GlowView(mParentView.getContext());
        mGlowView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        mRemoveGlowViewRunnable = () -> mParentView.removeView(mGlowView);
    }

    @Override
    public void prepare(float startX, float startY) {
        mParentView.removeCallbacks(mRemoveGlowViewRunnable);
        if (mGlowView.getParent() == null) mParentView.addView(mGlowView);
    }

    @Override
    public void onScroll(float xDelta) {
        mGlowView.onPull(xDelta / mParentView.getWidth());
    }

    @Override
    public void release() {
        mGlowView.onRelease();
        mGlowView.postInvalidateOnAnimation();
        if (mGlowView.getParent() != null) {
            mParentView.postDelayed(mRemoveGlowViewRunnable, REMOVE_RUNNABLE_DELAY_MS);
        }
    }

    @Override
    public void reset() {
        release();
    }

    @Override
    public void destroy() {
        if (mGlowView.getParent() != null) mParentView.removeView(mGlowView);
    }

    private static class GlowView extends View {
        private final EdgeEffect mEdgeEffectRight;

        public GlowView(Context context) {
            super(context);
            // Sets up edge effects
            mEdgeEffectRight = new EdgeEffect(context);
            setColor();
        }

        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        private void setColor() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                mEdgeEffectRight.setColor(android.R.color.black);
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            drawEdgeEffectsUnclipped(canvas);
        }

        @Override
        protected void onLayout(boolean changed, int l, int t, int r, int b) {
            super.onLayout(changed, l, t, r, b);
            mEdgeEffectRight.setSize(/* width= */ getHeight(), /* height= */ getWidth());
        }

        private void drawEdgeEffectsUnclipped(Canvas canvas) {
            boolean needsInvalidate = false;

            if (!mEdgeEffectRight.isFinished()) {
                // Clips the next few drawing operations to the content area,
                // and removes clipping rectangle afterwards.
                final int restoreCount = canvas.save();
                canvas.translate(getWidth(), 0);
                // Rotate and translate the canvas as needed before drawing the glow,
                // since EdgeEffectCompat always draws at the top of the canvas area.
                canvas.rotate(90, 0, 0);
                if (mEdgeEffectRight.draw(canvas)) needsInvalidate = true;
                canvas.restoreToCount(restoreCount);
            }

            if (needsInvalidate) postInvalidateOnAnimation();
        }

        /**
         * @see EdgeEffect#onPull(float)
         */
        public void onPull(float delta) {
            mEdgeEffectRight.onPull(delta);
        }

        /**
         * @see EdgeEffect#onRelease()
         */
        public void onRelease() {
            mEdgeEffectRight.onRelease();
        }
    }
}
