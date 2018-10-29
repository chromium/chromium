// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Matrix.ScaleToFit;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.ImageView;

/**
 * A helper class to simulate {@link View#getForeground()} on older versions of Android.  This class
 * requires specific setup to work properly.  The following methods must be overridden and forwarded
 * from the underly {@link View}:
 *
 * 1) {@link View#draw(Canvas)}.
 * 2) {@link View#onVisibilityChanged(View,boolean)}
 * 3) {@link View#drawableStateChanged()}.
 * 4) {@link View#verifyDrawable(Drawable)}.
 *
 * Here is a rough example of how to use this below to extend an ImageView (replace with any View):
 *
 * public class ForegroundEnabledView extends ImageView {
 *     private final ForegroundDrawableCompat mCompat;
 *
 *     public class ForegroundEnabledView(Context context) {
 *         super(context);
 *         mCompat = new ForegroundDrawableCompat(this);
 *     }
 *
 *     public void someHelperMethod(Drawable drawable) {
 *         mCompat.setDrawable(drawable);
 *     }
 *
 *     // ImageView implementation.
 *     @Override
 *     public void draw(Canvas canvas) {
 *         super.draw(canvas);
 *
 *         // It is important to make sure the foreground drawable draws *after* other view content.
 *         mCompat.draw(canvas);
 *     }
 *
 *     @Override
 *     protected void onVisibilityChanged(View changedView, int visibility) {
 *         super.onVisibilityChanged(changedView, visibility);
 *         mCompat.onVisibilityChanged(changedView, visibility);
 *     }
 *
 *     @Override
 *     protected void drawableStateChanged() {
 *         super.drawableStateChanged();
 *         mCompat.drawableStateChanged();
 *     }
 *
 *     @Override
 *     protected boolean verifyDrawable(Drawable dr) {
 *         return super.verifyDrawable(dr) || mCompat.verifyDrawable(dr);
 *     }
 * }
 */
public class ForegroundDrawableCompat
        implements OnAttachStateChangeListener, OnLayoutChangeListener {
    private final RectF mTempSrc = new RectF();
    private final RectF mTempDst = new RectF();
    private final Matrix mDrawMatrix = new Matrix();

    private final View mView;

    private boolean mOnBoundsChanged;
    private Drawable mDrawable;

    private ImageView.ScaleType mScaleType = ImageView.ScaleType.FIT_CENTER;

    /**
     * Builds a {@link ForegroundDrawableCompat} around {@code View}.  This will enable setting a
     * {@link Drawable} to draw on top of {@link View} at draw time.
     * @param view The {@link View} to add foreground {@link Drawable} support to.
     */
    public ForegroundDrawableCompat(View view) {
        mView = view;
        mView.addOnAttachStateChangeListener(this);
        mView.addOnLayoutChangeListener(this);
    }

    /**
     * Sets {@code drawable} to draw on top of the {@link View}.
     * @param drawable The {@link Drawable} to set as a foreground {@link Drawable} on the
     *                 {@link View}.
     */
    public void setDrawable(Drawable drawable) {
        if (mDrawable == drawable) return;

        if (mDrawable != null) {
            if (ViewCompat.isAttachedToWindow(mView)) mDrawable.setVisible(false, false);
            mDrawable.setCallback(null);
            mView.unscheduleDrawable(mDrawable);
            mDrawable = null;
        }

        mDrawable = drawable;

        if (mDrawable != null) {
            mOnBoundsChanged = true;

            DrawableCompat.setLayoutDirection(mDrawable, ViewCompat.getLayoutDirection(mView));
            if (mDrawable.isStateful()) mDrawable.setState(mView.getDrawableState());

            // TODO(dtrainor): Support tint?

            if (ViewCompat.isAttachedToWindow(mView)) {
                mDrawable.setVisible(
                        mView.getWindowVisibility() == View.VISIBLE && mView.isShown(), false);
            }
            mDrawable.setCallback(mView);
        }
        mView.requestLayout();
        mView.invalidate();
    }

    /** @return The currently set foreground {@link Drawable}. */
    public Drawable getDrawable() {
        return mDrawable;
    }

    /**
     * Determines how the foreground {@code Drawable} will be drawn in front of the {@link View}.
     * Right now the only supported types are FIT_* types and the CENTER type.
     *
     * TODO(dtrainor): Add support for more scale types.
     *
     * @param type The type of scale to apply to the {@Drawable} (see {@link ImageView.ScaleType}).
     */
    public void setScaleType(ImageView.ScaleType type) {
        if (mScaleType == type) return;
        mScaleType = type;
        mOnBoundsChanged = true;

        if (mDrawable != null) mView.invalidate();
    }

    /** Meant to be called from {@link View#onDraw(Canvas)}. */
    public void draw(Canvas canvas) {
        if (mDrawable == null) return;

        computeBounds();

        if (mDrawMatrix.isIdentity()) {
            mDrawable.draw(canvas);
        } else {
            final int saveCount = canvas.getSaveCount();
            canvas.save();
            canvas.concat(mDrawMatrix);
            mDrawable.draw(canvas);
            canvas.restoreToCount(saveCount);
        }
    }

    /** Meant to be called from {@link View#onVisibilityChanged(View,visibility)}. */
    public void onVisibilityChanged(View view, int visibility) {
        if (mView != view || mDrawable == null) return;

        ViewParent parent = mView.getParent();
        boolean parentVisible = parent != null
                && (!(parent instanceof ViewGroup) || ((ViewGroup) parent).isShown());

        if (parentVisible && mView.getWindowVisibility() == View.VISIBLE) {
            mDrawable.setVisible(visibility == View.VISIBLE, false);
        }
    }

    /** Meant to be called from {@link View#drawableStateChanged()}. */
    public void drawableStateChanged() {
        if (mDrawable == null) return;
        if (mDrawable.setState(mView.getDrawableState())) mView.invalidate();
    }

    /** Meant to be called from {@link View#verifyDrawable(Drawable)}. */
    public boolean verifyDrawable(Drawable drawable) {
        return drawable != null && mDrawable == drawable;
    }

    // OnAttachStateChangeListener implementation.
    @Override
    public void onViewAttachedToWindow(View v) {
        if (mDrawable == null) return;
        if (mView.isShown() && mView.getWindowVisibility() != View.GONE) {
            mDrawable.setVisible(mView.getVisibility() == View.VISIBLE, false);
        }
    }

    @Override
    public void onViewDetachedFromWindow(View v) {
        if (mDrawable == null) return;
        if (mView.isShown() && mView.getWindowVisibility() != View.GONE) {
            mDrawable.setVisible(false, false);
        }
    }

    // OnLayoutChangeListener implementation.
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mDrawable == null) return;

        int width = right - left;
        int height = bottom - top;

        if (width != mDrawable.getBounds().width() || height != mDrawable.getBounds().height()) {
            mOnBoundsChanged = true;
        }
    }

    private void computeBounds() {
        if (mDrawable == null || !mOnBoundsChanged) return;

        mDrawMatrix.reset();

        int drawableWidth = mDrawable.getIntrinsicWidth();
        int drawableHeight = mDrawable.getIntrinsicHeight();

        int viewWidth = mView.getWidth();
        int viewHeight = mView.getHeight();

        mTempSrc.set(0, 0, drawableWidth, drawableHeight);
        mTempDst.set(0, 0, viewWidth, viewHeight);

        if (mScaleType == ImageView.ScaleType.FIT_START) {
            mDrawMatrix.setRectToRect(mTempSrc, mTempDst, ScaleToFit.START);
            mDrawable.setBounds(0, 0, drawableWidth, drawableHeight);
        } else if (mScaleType == ImageView.ScaleType.FIT_CENTER) {
            mDrawMatrix.setRectToRect(mTempSrc, mTempDst, ScaleToFit.CENTER);
            mDrawable.setBounds(0, 0, drawableWidth, drawableHeight);
        } else if (mScaleType == ImageView.ScaleType.FIT_END) {
            mDrawMatrix.setRectToRect(mTempSrc, mTempDst, ScaleToFit.END);
            mDrawable.setBounds(0, 0, drawableWidth, drawableHeight);
        } else if (mScaleType == ImageView.ScaleType.CENTER) {
            mDrawMatrix.setTranslate(Math.round((viewWidth - drawableWidth) * 0.5f),
                    Math.round((viewHeight - drawableHeight) * 0.5f));
            mDrawable.setBounds(0, 0, drawableWidth, drawableHeight);
        } else {
            mDrawable.setBounds(0, 0, viewWidth, viewHeight);
        }

        mOnBoundsChanged = false;
    }
}
