package com.ark.browser.ui.widget;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;

import androidx.annotation.Nullable;

import com.zpj.utils.ScreenUtils;

public class SolidArrowView extends View {

    private final Paint mPaint = new Paint();
    private final Path mPath = new Path();

    private int mAngle;
    private int mEndAngle;
    private int mStrokeWidth;
    private int mSize;

    private ValueAnimator mAnimator;

    public SolidArrowView(Context context) {
        this(context, null);
    }

    public SolidArrowView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public SolidArrowView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        mSize = (int) (12 * context.getResources().getDisplayMetrics().density);
        mStrokeWidth = 6;
        mAngle = 70;
        mEndAngle = mAngle;
        mPaint.setStyle(Paint.Style.FILL_AND_STROKE);
        mPaint.setStrokeWidth(mStrokeWidth);
        mPaint.setColor(Color.BLACK);
        mPaint.setAntiAlias(true);
        mPaint.setStrokeCap(Paint.Cap.ROUND);
        mPaint.setStrokeJoin(Paint.Join.ROUND);
    }

    public void setColor(int color) {
        mPaint.setColor(color);
        invalidate();
    }

    public void setSize(int mSize) {
        this.mSize = mSize;
        mStrokeWidth = (int) (ScreenUtils.px2dpInt(mSize) / 2);
        mPaint.setStrokeWidth(mStrokeWidth);
        requestLayout();
    }

    public void switchState() {
        if (mAnimator != null) {
            mAnimator.cancel();
        }
        mEndAngle = 360 - mEndAngle;
        mAnimator = ValueAnimator.ofInt(mAngle, mEndAngle);
        mAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator valueAnimator) {
                mAngle = (int) valueAnimator.getAnimatedValue();
                invalidate();
            }
        });
        mAnimator.setDuration(360);
        mAnimator.start();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
//        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int size = MeasureSpec.makeMeasureSpec(mSize, MeasureSpec.EXACTLY);
        setMeasuredDimension(size, size);
    }

    @Override
    protected void onDraw(Canvas canvas) {
//        super.onDraw(canvas);

        float centerX = getWidth() / 2f;
        float centerY = getHeight() / 2f;

        double ang = Math.toRadians(mAngle / 2f);
        int startX = (int) (Math.abs(Math.round(mStrokeWidth * Math.cos(ang)))) + 4;

        float delta = (float) ((centerX - startX) / Math.tan(ang) / 2);
        Log.d("ArrowIcon", "startX=" + startX + " delta=" + delta + " angle=" + mAngle + " Math.cos(ang)=" + Math.cos(ang));


        mPath.reset();
        mPath.moveTo(startX, centerY - delta);
        mPath.lineTo(centerX, centerY + delta);
        mPath.lineTo(getWidth() - startX, centerY - delta);
        mPath.close();
        canvas.drawPath(mPath, mPaint);

    }
}

