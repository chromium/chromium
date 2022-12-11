package com.ark.browser.ui.widget;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.AccelerateDecelerateInterpolator;

import androidx.annotation.IntRange;
import androidx.annotation.Nullable;

public class AnimProgressBar extends View {

    private final int[] colors = { Color.TRANSPARENT, 0xEEFFFFFF, Color.TRANSPARENT };
    private final int[] mProgressColors = { 0xFF22D4FF, 0xFF009DFF };
    private final int mBackgroundCOlor = Color.rgb(248, 249, 251);

    private final Paint mPaint = new Paint();
    private final RectF mViewRect = new RectF();
    private final RectF mProgressRect = new RectF();

    private final Matrix mMatrix = new Matrix();

    private LinearGradient mShimmerGradient;
    private LinearGradient mProgressGradient;

    private int mProgress = 0;

    private final ValueAnimator mShimmerAnimator = ValueAnimator.ofFloat(0f, 1f);

    public AnimProgressBar(Context context) {
        this(context, null);
    }

    public AnimProgressBar(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public AnimProgressBar(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        mPaint.setStyle(Paint.Style.FILL_AND_STROKE);
        mPaint.setStrokeCap(Paint.Cap.ROUND);
        mPaint.setStrokeJoin(Paint.Join.ROUND);

        mShimmerAnimator.setRepeatCount(ValueAnimator.INFINITE);
        mShimmerAnimator.setInterpolator(new AccelerateDecelerateInterpolator());
        mShimmerAnimator.addUpdateListener(animation -> {
            if (getVisibility() != VISIBLE) {
                mShimmerAnimator.cancel();
                mShimmerGradient = null;
                return;
            }
            float percent = (float) animation.getAnimatedValue();

            mMatrix.reset();
            mMatrix.setTranslate((2f - percent) * (mProgressRect.width() + 200) * percent, 0);
            mShimmerGradient = new LinearGradient(-200, getHeight() / 2f,
                    0, getHeight() / 2f,
                    colors, null, Shader.TileMode.CLAMP);
            mShimmerGradient.setLocalMatrix(mMatrix);
            invalidate();
        });
        mShimmerAnimator.setDuration(1500);

    }

    public void setProgress(@IntRange(from = 0, to = 100) int progress, boolean anim) {
        if (this.mProgress == progress && mShimmerAnimator.isRunning() == anim) {
            return;
        }
        this.mProgress = progress;
        if (isAttachedToWindow()) {
            initProgress();
            if (anim) {
                if (!mShimmerAnimator.isRunning()) {
                    mShimmerAnimator.start();
                }
            } else {
                mShimmerAnimator.cancel();
                mShimmerGradient = null;
            }
            postInvalidate();
        }
    }

    private void initProgress() {
        mProgressRect.set(0, 0, (int) (mProgress / 100f * getWidth()), getHeight());
        mProgressGradient = new LinearGradient(0, getHeight() / 2f,
                mProgressRect.right, getHeight() / 2f,
                mProgressColors, null, Shader.TileMode.CLAMP);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        mViewRect.set(0, 0, getWidth(), getHeight());
        initProgress();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        mPaint.setShader(null);
        mPaint.setColor(mBackgroundCOlor);
        canvas.drawRoundRect(mViewRect, getHeight() / 2f, getHeight() / 2f, mPaint);

        if (mProgressGradient != null) {
            mPaint.setShader(mProgressGradient);
            canvas.drawRoundRect(mProgressRect, getHeight() / 2f, getHeight() / 2f, mPaint);
        }

        if (mShimmerGradient != null) {
            mPaint.setShader(mShimmerGradient);
            canvas.drawRect(mProgressRect, mPaint);
        }

    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mShimmerAnimator.cancel();
        mShimmerGradient = null;
    }

}

