package com.ark.browser.ui.widget.homepage;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.LinearInterpolator;

import com.zpj.skin.SkinEngine;
import com.zpj.utils.ContextUtils;
import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.R;


public final class NewTabTransformAnimation {

    private Context context;

    //DecorView
    private ViewGroup mRootView;
    private AnimationView animationView;

    private Rect mStartRect;
    //扩散的起点
    private float mStartX = 0, mStartY = 0;
    private Runnable endRunnable;
    private Runnable startRunnable;

    private boolean isStarted;

    private final Rect mEndRect;

    private NewTabTransformAnimation(Context context) {
        this.context = context;
        //获取activity的根视图,用来添加本View
        mRootView = (ViewGroup) ContextUtils.getActivity(context).getWindow().getDecorView();
        mEndRect = new Rect(0, 0, mRootView.getWidth(), mRootView.getHeight());
    }

    public static NewTabTransformAnimation with(Context context) {
        return new NewTabTransformAnimation(context);
    }

    public NewTabTransformAnimation setRect(Rect rect) {
        mStartRect = rect;
        return this;
    }

    public NewTabTransformAnimation setCenterPosition(float startX, float startY) {
        this.mStartX = startX;
        this.mStartY = startY;
        return this;
    }

    public NewTabTransformAnimation onAnimationEnd(Runnable dismissRunnable) {
        this.endRunnable = dismissRunnable;
        return this;
    }

    public NewTabTransformAnimation onAnimationStart(Runnable startRunnable) {
        this.startRunnable = startRunnable;
        return this;
    }

    public void start() {
        if (!isStarted) {
            isStarted = true;
            updateMaxRadius();
            animationView = new AnimationView(context);
            animationView.setPivotX(mStartX);
            animationView.setPivotY(mStartY);
            animationView.setScaleX(0f);
            animationView.setScaleY(0f);
            SkinEngine.setBackground(animationView, R.attr.backgroundColor);
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
        if (mStartX == 0 && mStartY == 0) {
            mStartX = ScreenUtils.getScreenWidth(context) / 2f;
            mStartY = ScreenUtils.getScreenHeight(context) / 2f;
        }
    }

    private void startAnimation() {
//        float radius = animationView.getRadius();
//        ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
//        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
//            @Override
//            public void onAnimationUpdate(ValueAnimator valueAnimator) {
//                float p = (float) valueAnimator.getAnimatedValue();
//
//                float centerX = mStartX + (mEndRect.centerX() - mStartX) * p;
//                float centerY = mStartY + (mEndRect.centerY() - mStartY) * p;
//
//                int w = (int) ((mEndRect.width() - mStartRect.width()) * p + mStartRect.width());
//                int h = (int) ((mEndRect.height() - mStartRect.height()) * p + mStartRect.height());
//
//                animationView.setRadius(radius * (1 - p));
//                animationView.measure(View.MeasureSpec.makeMeasureSpec(w, View.MeasureSpec.EXACTLY),
//                        View.MeasureSpec.makeMeasureSpec(h, View.MeasureSpec.EXACTLY));
//                animationView.layout((int) (centerX - w / 2), (int) (centerY - h / 2), (int) (centerX + w / 2), (int) (centerY + h / 2));
//
//            }
//        });
//        animator.addListener(new AnimatorListenerAdapter() {
//            @Override
//            public void onAnimationEnd(Animator animation) {
//                if (endRunnable != null) {
//                    endRunnable.run();
//                }
//                animationView.animate()
//                        .setListener(new AnimatorListenerAdapter() {
//                            @Override
//                            public void onAnimationEnd(Animator animation) {
//                                isStarted = false;
//                                //动画播放完毕, 移除View
//                                if (mRootView != null) {
//                                    mRootView.removeView(animationView);
//                                    mRootView = null;
//                                }
//                                context = null;
//                                animationView = null;
//                            }
//                        })
//                        .setInterpolator(new LinearInterpolator())
//                        .alpha(0f)
//                        .setDuration(200)
//                        .start();
//            }
//        });
//        animator.setInterpolator(new DecelerateInterpolator(1.5f));
//        animator.setDuration(420);
//        animator.start();


        animationView.animate()
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (endRunnable != null) {
                            endRunnable.run();
                        }
                        animationView.animate()
                                .setListener(new AnimatorListenerAdapter() {
                                    @Override
                                    public void onAnimationEnd(Animator animation) {
                                        isStarted = false;
                                        //动画播放完毕, 移除View
                                        if (mRootView != null) {
                                            mRootView.removeView(animationView);
                                            mRootView = null;
                                        }
                                        context = null;
                                        animationView = null;
                                    }
                                })
                                .setInterpolator(new LinearInterpolator())
                                .alpha(0f)
                                .setDuration(200)
                                .start();
                    }

                    @Override
                    public void onAnimationStart(Animator animation) {
                        if (startRunnable != null) {
                            startRunnable.run();
                        }
                    }
                })
                .setInterpolator(new DecelerateInterpolator())
                .scaleX(1f)
                .scaleY(1f)
                .setDuration(420)
                .start();
    }

    public static class AnimationView extends View
            implements View.OnClickListener, View.OnLongClickListener, View.OnTouchListener {

        public AnimationView(Context context) {
            super(context);
            setOnClickListener(this);
            setOnLongClickListener(this);
            setOnTouchListener(this);
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

