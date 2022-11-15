package com.ark.browser.utils;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.RectF;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import com.zpj.utils.ContextUtils;
import com.zpj.utils.ScreenUtils;

/**
 * @author Z-P-J
 * 参考 https://github.com/wuyr/RippleAnimation
 */
public final class SkinChangeAnimation {

    private Context context;

    //DecorView
    private ViewGroup mRootView;
    private View animationView;

    //屏幕截图
    private Bitmap mBackground;

    //扩散的起点
    private float mStartX = 0, mStartY = 0;
    private int mMaxRadius, mStartRadius, mCurrentRadius;
    private long mDuration = 500;
    private Runnable dismissRunnable;
    private Runnable startRunnable;

    private Paint mPaint;

    private boolean isStarted;

    private SkinChangeAnimation(Context context) {
        this.context = context;
        //获取activity的根视图,用来添加本View
        mRootView = (ViewGroup) ContextUtils.getActivity(context).getWindow().getDecorView();

        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        //设置为擦除模式
        mPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
    }

    public static SkinChangeAnimation with(Context context) {
        return new SkinChangeAnimation(context);
    }

    public SkinChangeAnimation setStartPosition(float startX, float startY) {
        this.mStartX = startX;
        this.mStartY = startY;
        return this;
    }

    public SkinChangeAnimation setStartRadius(int mStartRadius) {
        this.mStartRadius = mStartRadius;
        return this;
    }

    public SkinChangeAnimation setDuration(long mDuration) {
        this.mDuration = mDuration;
        return this;
    }

    public SkinChangeAnimation setDismissRunnable(Runnable dismissRunnable) {
        this.dismissRunnable = dismissRunnable;
        return this;
    }

    public SkinChangeAnimation setStartRunnable(Runnable startRunnable) {
        this.startRunnable = startRunnable;
        return this;
    }

    public void start() {
        if (!isStarted) {
            isStarted = true;

            updateMaxRadius();

            updateBackground();

            animationView = new AnimationView(context);
            animationView.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            mRootView.addView(animationView);

            startAnimation();


        }
    }

    /**
     * 根据起始点将屏幕分成4个小矩形,mMaxRadius就是取它们中最大的矩形的对角线长度
     * 这样的话, 无论起始点在屏幕中的哪一个位置上, 我们绘制的圆形总是能覆盖屏幕
     */
    private void updateMaxRadius() {

        if (mStartRadius < 0) {
            mStartRadius = 0;
        }

        if (mStartX == 0 && mStartY == 0) {
            mStartX = ScreenUtils.getScreenWidth(context) / 2f;
            mStartY = ScreenUtils.getScreenHeight(context) / 2f;
        }

        //将屏幕分成4个小矩形
        RectF leftTop = new RectF(0, 0, mStartX + mStartRadius, mStartY + mStartRadius);
        RectF rightTop = new RectF(leftTop.right, 0, mRootView.getRight(), leftTop.bottom);
        RectF leftBottom = new RectF(0, leftTop.bottom, leftTop.right, mRootView.getBottom());
        RectF rightBottom = new RectF(leftBottom.right, leftTop.bottom, mRootView.getRight(), leftBottom.bottom);
        //分别获取对角线长度
        double leftTopHypotenuse = Math.sqrt(Math.pow(leftTop.width(), 2) + Math.pow(leftTop.height(), 2));
        double rightTopHypotenuse = Math.sqrt(Math.pow(rightTop.width(), 2) + Math.pow(rightTop.height(), 2));
        double leftBottomHypotenuse = Math.sqrt(Math.pow(leftBottom.width(), 2) + Math.pow(leftBottom.height(), 2));
        double rightBottomHypotenuse = Math.sqrt(Math.pow(rightBottom.width(), 2) + Math.pow(rightBottom.height(), 2));
        //取最大值
        mMaxRadius = (int) Math.max(
                Math.max(leftTopHypotenuse, rightTopHypotenuse),
                Math.max(leftBottomHypotenuse, rightBottomHypotenuse));
    }


    /**
     * 更新屏幕截图
     */
    private void updateBackground() {
        if (mBackground != null && !mBackground.isRecycled()) {
            mBackground.recycle();
        }
        mRootView.setDrawingCacheEnabled(true);
        mBackground = mRootView.getDrawingCache();
        mBackground = Bitmap.createBitmap(mBackground);
        mRootView.setDrawingCacheEnabled(false);
    }

    private void startAnimation() {
        //动画播放完毕, 移除View
        Animator.AnimatorListener mAnimatorListener = new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                isStarted = false;

                //动画播放完毕, 移除View
                if (mRootView != null) {
                    mRootView.removeView(animationView);
                    mRootView = null;
                }
                if (mBackground != null) {
                    if (!mBackground.isRecycled()) {
                        mBackground.recycle();
                    }
                    mBackground = null;
                }
                if (mPaint != null) {
                    mPaint = null;
                }
                context = null;
                animationView = null;
                if (dismissRunnable != null) {
                    dismissRunnable.run();
                }
            }

            @Override
            public void onAnimationStart(Animator animation) {
                if (startRunnable != null) {
                    startRunnable.run();
                }
            }
        };
        //更新圆的半径
        ValueAnimator.AnimatorUpdateListener mAnimatorUpdateListener = new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                //更新圆的半径
                mCurrentRadius = (int) (float) animation.getAnimatedValue() + mStartRadius;
                animationView.postInvalidate();
            }
        };

        ValueAnimator valueAnimator = ValueAnimator.ofFloat(0, mMaxRadius).setDuration(mDuration);
        valueAnimator.addUpdateListener(mAnimatorUpdateListener);
        valueAnimator.addListener(mAnimatorListener);
        valueAnimator.start();
    }

    public class AnimationView extends View
            implements View.OnClickListener, View.OnLongClickListener, View.OnTouchListener {

        public AnimationView(Context context) {
            super(context);
            setOnClickListener(this);
            setOnLongClickListener(this);
            setOnTouchListener(this);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            //在新的图层上面绘制
            int layer;
            layer = canvas.saveLayer(0, 0, getWidth(), getHeight(), null);
            canvas.drawBitmap(mBackground, 0, 0, null);
            canvas.drawCircle(mStartX, mStartY, mCurrentRadius, mPaint);
            canvas.restoreToCount(layer);
        }

        @Override
        public void onClick(View v) {

        }

        @Override
        public boolean onLongClick(View v) {
            return true;
        }

        @Override
        public boolean onTouch(View v, MotionEvent event) {
            return true;
        }
    }

}
