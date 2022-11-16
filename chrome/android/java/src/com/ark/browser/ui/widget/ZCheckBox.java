package com.ark.browser.ui.widget;

import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PathMeasure;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Checkable;

import org.chromium.chrome.R;

/**
 * 自定义CheckBox
 * @author Z-P-J
 */
public class ZCheckBox extends View implements Checkable {

    /**
     * 画笔
     */
    private final Paint mPaint = new Paint();

    /**
     * 勾的路径
     */
    private final Path mTickPath = new Path();

    /**
     * 当前勾的路径，用于动画
     */
    private final Path mCurrentTickPath = new Path();

    /**
     * 打勾动画路径计算
     */
    private final PathMeasure mPathMeasure = new PathMeasure();

    /**
     * 颜色渐变计算
     */
    private final ArgbEvaluator mArgbEvaluator = new ArgbEvaluator();

    /**
     * 最小大小，默认24dp
     */
    private final int minSize;

    /**
     * 中心横坐标
     */
    private float mCenterX;
    /**
     * 中心纵坐标
     */
    private float mCenterY;

    /**
     * checkable状态
     */
    private boolean mChecked;

    /**
     * CheckBox大小的一半
     */
    private float mSize;

    /**
     * 半径大小，mRadius = mSize - mBorderSize / 2;
     */
    private float mRadius;

    /**
     * 背景颜色半径，mBgRadius = mSize - mBorderSize;
     */
    private float mBgRadius;

    /**
     * 边框大小，默认mBorderSize = mSize / 4f;
     */
    private float mBorderSize;

    /**
     * 当前边框大小
     */
    private float mCurrentBorderSize;

    /**
     * 勾的大小，默认mBorderSize = mSize / 5f;
     */
    private float mTickSize;

    /**
     * check状态的背景颜色
     */
    private int mCheckedColor;

    /**
     * uncheck状态的背景颜色
     */
    private int mUnCheckedColor;

    /**
     * 勾的颜色
     */
    private int mTickColor;

    /**
     * 边框颜色
     */
    private int mBorderColor;

    /**
     * 当前边框的渐变颜色
     */
    private int mCurrentBorderColor;

    /**
     * check状态切换属性动画
     */
    private ValueAnimator mAnimator;

    /**
     * 动画时长
     */
    private int mDuration;

    /**
     * 监听check状态
     */
    private OnCheckedChangeListener mOnCheckedChangeListener;

    public interface OnCheckedChangeListener {
        void onCheckedChanged(ZCheckBox checkBox, boolean isChecked);
    }

    public ZCheckBox(Context context) {
        this(context, null);
    }

    public ZCheckBox(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ZCheckBox(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        minSize = (int) (getResources().getDisplayMetrics().density * 24);

        TypedArray ta = context.obtainStyledAttributes(attrs, R.styleable.ZCheckBox);

        mCheckedColor = ta.getColor(R.styleable.ZCheckBox_z_checkbox_checked_color, getPrimaryColor(context));
        mUnCheckedColor = ta.getColor(R.styleable.ZCheckBox_z_checkbox_unchecked_color, Color.WHITE);
        mTickColor = ta.getColor(R.styleable.ZCheckBox_z_checkbox_tick_color, Color.WHITE);
        mBorderColor = ta.getColor(R.styleable.ZCheckBox_z_checkbox_border_color, Color.LTGRAY);

        mTickSize = ta.getDimensionPixelSize(R.styleable.ZCheckBox_z_checkbox_tick_size, 0);
        mBorderSize = ta.getDimensionPixelSize(R.styleable.ZCheckBox_z_checkbox_border_size, 0);

        mDuration = ta.getInt(R.styleable.ZCheckBox_z_checkbox_anim_duration, 360);

        ta.recycle();

        mPaint.setAntiAlias(true);
        mPaint.setStrokeJoin(Paint.Join.ROUND);
        mPaint.setStrokeCap(Paint.Cap.ROUND);
        mPaint.setStyle(Paint.Style.STROKE);

        setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                setChecked(!mChecked);
            }
        });
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        Log.e("ZCheckBox", "onMeasure");
        int width = measureSize(widthMeasureSpec, true);
        int height = measureSize(heightMeasureSpec, false);
        setMeasuredDimension(width, height);

        int paddingStart = getPaddingStart();
        int paddingTop = getPaddingTop();
        int w = Math.max(width - paddingStart - getPaddingEnd(), 0);
        int h = Math.max(height - paddingTop - getPaddingBottom(), 0);

        // 中心坐标
        mCenterX = w / 2f + paddingStart;
        mCenterY = h / 2f + paddingTop;
        mSize = Math.min(w, h) / 2f;

        if (mBorderSize <= 0) {
            mBorderSize = mSize / 4f;
        }
        if (mTickSize <= 0) {
            mTickSize = mSize / 5f;
        }
        mRadius = mSize - mBorderSize / 2f;

        // 计算勾的路径
        mTickPath.reset();
        mTickPath.moveTo(mCenterX - mRadius / 2f, mCenterY);
        mTickPath.rLineTo(mRadius / 3f, mRadius / 3f);
        mTickPath.rLineTo(mRadius * 2 / 3f, -mRadius * 2 / 3f);
        update(1f);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        mPaint.setStrokeWidth(0);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(mUnCheckedColor);
        Log.e("ZCheckBox", "onDraw mBgRadius=" + mBgRadius + " mUnCheckedColor=" + mUnCheckedColor);
        canvas.drawCircle(mCenterX, mCenterY, mBgRadius, mPaint);

        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(mCurrentBorderSize);
        mPaint.setColor(mCurrentBorderColor);
        Log.e("ZCheckBox", "onDraw mCurrentBorderSize=" + mCurrentBorderSize + " mRadius=" + mRadius + " mCurrentBorderColor=" + mCurrentBorderColor);
        canvas.drawCircle(mCenterX, mCenterY, mRadius, mPaint);

        mPaint.setColor(Color.BLUE);
        mPaint.setStrokeWidth(mTickSize);
        Log.e("ZCheckBox", "onDraw mTickSize=" + mTickSize + " mTickColor=" + mTickColor);
        canvas.drawPath(mCurrentTickPath, mPaint);
    }


    @Override
    public void setChecked(boolean checked) {
        setChecked(checked, true);
    }

    public void setCheckedWithoutAnim(boolean checked) {
        setChecked(checked, false);
    }

    public void setChecked(boolean checked, boolean animate) {
        if (mChecked == checked) {
            return;
        }
        mChecked = checked;
        if (mOnCheckedChangeListener != null) {
            mOnCheckedChangeListener.onCheckedChanged(this, mChecked);
        }
        if (mAnimator != null) {
            mAnimator.cancel();
            mAnimator = null;
        }
        if (animate) {
            mAnimator = ValueAnimator.ofFloat(0, 1f);
            mAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator valueAnimator) {
                    float progress = (float) valueAnimator.getAnimatedValue();
                    update(progress);
                    postInvalidate();
                }
            });
            mAnimator.setDuration(mDuration);
            mAnimator.start();
        } else {
            update(1f);
            postInvalidate();
        }
    }

    @Override
    public boolean isChecked() {
        return mChecked;
    }

    @Override
    public void toggle() {
        setChecked(!mChecked);
    }

    public void toggleWithoutAnim() {
        setCheckedWithoutAnim(!mChecked);
    }

    public void setDuration(int mDuration) {
        this.mDuration = mDuration;
    }

    public int getDuration() {
        return mDuration;
    }

    public void setBorderColor(int mBorderColor) {
        this.mBorderColor = mBorderColor;
        onValueChanged();
    }

    public int getBorderColor() {
        return mBorderColor;
    }

    public void setBorderSize(float mBorderSize) {
        this.mBorderSize = mBorderSize;
        onValueChanged();
    }

    public float getBorderSize() {
        return mBorderSize;
    }

    public void setTickColor(int mTickColor) {
        this.mTickColor = mTickColor;
        onValueChanged();
    }

    public int getTickColor() {
        return mTickColor;
    }

    public void setTickSize(float mTickSize) {
        this.mTickSize = mTickSize;
        onValueChanged();
    }

    public float getTickSize() {
        return mTickSize;
    }

    public void setCheckedColor(int mCheckedColor) {
        this.mCheckedColor = mCheckedColor;
        onValueChanged();
    }

    public int getCheckedColor() {
        return mCheckedColor;
    }

    public void setUnCheckedColor(int mUnCheckedColor) {
        this.mUnCheckedColor = mUnCheckedColor;
        onValueChanged();
    }

    public int getUnCheckedColor() {
        return mUnCheckedColor;
    }

    public void setOnCheckedChangeListener(OnCheckedChangeListener onCheckedChangeListener) {
        this.mOnCheckedChangeListener = onCheckedChangeListener;
    }

    public OnCheckedChangeListener getOnCheckedChangeListener() {
        return mOnCheckedChangeListener;
    }

    private int measureSize(int measureSpec, boolean isWidth) {
        int specMode = MeasureSpec.getMode(measureSpec);
        int specSize = MeasureSpec.getSize(measureSpec);
        switch (specMode) {
            case MeasureSpec.EXACTLY:
                return specSize;
            case MeasureSpec.AT_MOST:
            case MeasureSpec.UNSPECIFIED:

                int minSize = isWidth ? this.minSize + getPaddingStart() + getPaddingEnd()
                        : this.minSize + getPaddingTop() + getPaddingBottom();
                ViewGroup.LayoutParams params = getLayoutParams();
                if (params == null) {
                    return Math.min(minSize, specSize);
                } else {
                    int size = isWidth ? params.width : params.height;
                    if (size == ViewGroup.LayoutParams.WRAP_CONTENT) {
                        return Math.min(minSize, specSize);
                    } else {
                        return Math.max(minSize, specSize);
                    }
                }


        }
        return specSize;
    }

    private void update(float progress) {
        mCurrentTickPath.reset();
        if (mChecked) {
            if (progress <= 0.4f) {
                float percent = progress > 0.2f ? 1f : 5 * progress;
                float mTempSize = mSize * (0.8f + Math.abs(progress - 0.2f));
                mCurrentBorderSize = mBorderSize + (mTempSize - mBorderSize) * percent;
                mBgRadius = mTempSize - mCurrentBorderSize;
                mRadius = mBgRadius + mCurrentBorderSize / 2;
                mCurrentBorderColor = (int) mArgbEvaluator.evaluate(percent, mBorderColor, mCheckedColor);
            } else {
                mCurrentBorderSize = mSize;
                mBgRadius = 0;
                mRadius = mSize / 2;
                mCurrentBorderColor = mCheckedColor;
                mPathMeasure.nextContour();
                mPathMeasure.setPath(mTickPath, false);
                mPathMeasure.getSegment(0, ((progress - 0.4f) / 0.6f) * mPathMeasure.getLength(), mCurrentTickPath, true);
            }
        } else {
            float mTempSize = mSize * (0.8f + Math.abs(2 * progress - 1f) * 0.2f);
            mCurrentBorderSize = mBorderSize + (mTempSize - mBorderSize) * (1f - progress);
            mBgRadius = mTempSize - mCurrentBorderSize;
            mRadius = mBgRadius + mCurrentBorderSize / 2;
            mCurrentBorderColor = (int) mArgbEvaluator.evaluate(progress, mCheckedColor, mBorderColor);
        }
    }

    private void onValueChanged() {
        if (mAnimator == null || !mAnimator.isRunning()) {
            update(1f);
            postInvalidate();
        }
    }

    private int getPrimaryColor(Context context) {
        int[] attribute = new int[] { android.R.attr.colorPrimary};
        TypedArray array = context.getTheme().obtainStyledAttributes(attribute);
        int color = array.getColor(0, Color.BLUE);
        array.recycle();
        return color;
    }

}
