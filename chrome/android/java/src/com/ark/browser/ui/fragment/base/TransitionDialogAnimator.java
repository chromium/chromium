//package com.qianxun.browser.ui.fragment.base;
//
//import android.animation.Animator;
//import android.animation.AnimatorListenerAdapter;
//import android.animation.ValueAnimator;
//import android.graphics.Rect;
//import android.support.v4.view.animation.FastOutSlowInInterpolator;
//import android.support.v7.widget.CardView;
//import android.view.View;
//import android.view.ViewGroup;
//import android.widget.ImageView;
//
//import com.zpj.fragmentation.dialog.animator.AbsDialogAnimator;
//
//import org.chromium.base.Log;
//
//public class TransitionDialogAnimator extends AbsDialogAnimator<Animator, Animator> {
//
//    private CardView cardView;
//    private ImageView mIvLogo;
//
//    public TransitionDialogAnimator(View target, CardView cardView, ImageView ivLogo) {
//        super(target);
//        this.cardView = cardView;
//        this.mIvLogo = ivLogo;
//    }
//
//    @Override
//    public Animator onCreateShowAnimator() {
//
//
//        int width = targetView.getWidth();
//        int height = targetView.getHeight();
//
//        ViewGroup.LayoutParams layoutParams = mIvLogo.getLayoutParams();
//        layoutParams.width = width;
//        layoutParams.height = width;
//        mIvLogo.setLayoutParams(layoutParams);
//
//        if (mTransitionRect == null || mDrawableRes <= 0) {
//            int centerX = width / 2;
//            int centerY = height / 2;
//            mTransitionRect = new Rect(centerX, height, centerX, height);
//        }
//
//        Rect startRect = mTransitionRect;
//        Rect endRect = new Rect(0, 0, width, height);
//
//        ValueAnimator animator = ValueAnimator.ofFloat(0, 1f);
//        animator.setInterpolator(new FastOutSlowInInterpolator()); // new DecelerateInterpolator(2)
////        animator.addUpdateListener(animation -> {
////            float percent = (float) animation.getAnimatedValue();
////            int left = (int) (startRect.left + (endRect.left - startRect.left) * percent);
////            int top = (int) (startRect.top + (endRect.top - startRect.top) * percent);
////            int right = (int) (startRect.right + (endRect.right - startRect.right) * percent);
////            int bottom = (int) (startRect.bottom + (endRect.bottom - startRect.bottom) * percent);
////            float scale = (startRect.width() + (endRect.width() - startRect.width()) * percent) / endRect.width();
//////                float scaleY = (startRect.height() + (endRect.height() - startRect.height()) * percent) / endRect.height();
////
////            float scaleY = (float) (bottom - top) / endRect.height();
////
////            getImplView().setPivotX(0);
////            getImplView().setPivotY(0);
////            getImplView().setScaleX(scale);
////            getImplView().setScaleY(scaleY);
////            getImplView().setTranslationX(left);
////            getImplView().setTranslationY(top);
////
////            cardView.setRadius((1f - percent) * width);
////
////            getImplView().setAlpha(percent);
////        });
//
//        float midRadio = 0.6f;
//        int centerX = (int) (startRect.centerX() + (endRect.centerX() - startRect.centerX()) * midRadio);
//        int centerY = (int) (startRect.centerY() + (endRect.centerY() - startRect.centerY()) * midRadio);
//        int childHeight = (int) (startRect.height() + (endRect.height() - startRect.height()) * midRadio);
//        int childWidth = (int) (startRect.width() + (endRect.width() - startRect.width()) * midRadio);
//
//        int r = Math.min(childHeight, childWidth) / 2;
//        Rect middleRect = new Rect(
//                centerX - r,
//                centerY - r,
//                centerX + r,
//                centerY + r
//        );
//        animator.addUpdateListener(animation -> {
//            float percent = (float) animation.getAnimatedValue();
//
//            cardView.setRadius((1f - percent) * width);
//
//            int left;
//            int top;
//            int right;
//            int bottom;
//
//            left = (int) (startRect.left + (endRect.left - startRect.left) * percent);
//            top = (int) (startRect.top + (endRect.top - startRect.top) * percent);
//            right = (int) (startRect.right + (endRect.right - startRect.right) * percent);
//            bottom = (int) (startRect.bottom + (endRect.bottom - startRect.bottom) * percent);
//
//            float scaleX = (float) (right - left) / mIvLogo.getWidth();
//            float scaleY = (float) (bottom - top) / endRect.height();
//            getImplView().setPivotX(0);
//            getImplView().setPivotY(0);
//            getImplView().setScaleX(scaleX);
//            getImplView().setScaleY(scaleY);
//            getImplView().setTranslationX(left);
//            getImplView().setTranslationY(top);
//
//            cardView.setRadius((1f - percent) * width);
//
//            if (percent <= midRadio) {
//                getImplView().setAlpha(0f);
//                percent = percent / midRadio;
//
//                left = (int) (startRect.left + (middleRect.left - startRect.left) * percent);
//                top = (int) (startRect.top + (middleRect.top - startRect.top) * percent);
//                right = (int) (startRect.right + (middleRect.right - startRect.right) * percent);
//                bottom = (int) (startRect.bottom + (middleRect.bottom - startRect.bottom) * percent);
//
//                mIvLogo.setAlpha(1f - percent * midRadio);
//            } else {
//
//                getImplView().setAlpha(percent);
//                percent = (percent - midRadio) / (1f - midRadio);
//
//
//                mIvLogo.setAlpha(0f);
//
//                left = (int) (middleRect.left + (endRect.left - middleRect.left) * percent);
//                top = (int) (middleRect.top + (endRect.top - middleRect.top) * percent);
//                right = (int) (middleRect.right + (endRect.right - middleRect.right) * percent);
//                bottom = (int) (middleRect.bottom + (endRect.bottom - middleRect.bottom) * percent);
//            }
//
//            mIvLogo.setPivotX(0);
//            mIvLogo.setPivotY(0);
//            mIvLogo.setTranslationX(left);
//            mIvLogo.setTranslationY(top);
//            float scale = (float) (right - left) / endRect.width();
//            mIvLogo.setScaleX(scale);
//            mIvLogo.setScaleY(scale);
//            Log.d("fffffffffff", "height=" + mIvLogo.getHeight() + " width=" + mIvLogo.getWidth() + " scale=" + scale);
//            Log.d("fffffffffff", "wwwww=" + (scale * mIvLogo.getWidth()) + " ww=" + (right - left));
//
////            mIvLogo.measure(View.MeasureSpec.makeMeasureSpec(right - left, View.MeasureSpec.EXACTLY),
////                    View.MeasureSpec.makeMeasureSpec(right - left, View.MeasureSpec.EXACTLY));
////            mIvLogo.layout(left, top, right, bottom);
////            getImplView().measure(View.MeasureSpec.makeMeasureSpec(right - left, View.MeasureSpec.EXACTLY),
////                    View.MeasureSpec.makeMeasureSpec(bottom - top, View.MeasureSpec.EXACTLY));
//
//
//
//
////            getImplView().measure(View.MeasureSpec.makeMeasureSpec(right - left, View.MeasureSpec.EXACTLY),
////                    View.MeasureSpec.makeMeasureSpec(bottom - top, View.MeasureSpec.EXACTLY));
//
////            getImplView().layout(left, top, right, bottom);
//
////            view.layout(left, top, right, bottom);
//        });
//        animator.addListener(new AnimatorListenerAdapter() {
//
//            @Override
//            public void onAnimationStart(Animator animation) {
//                onShowAnimationStart(getSavedInstanceState());
//            }
//
//            @Override
//            public void onAnimationEnd(Animator animation) {
//                onShowAnimationEnd(getSavedInstanceState());
//            }
//        });
//        animator.setDuration(getShowAnimDuration());
//        animator.start();
//
//
//
//        return null;
//    }
//
//    @Override
//    public Animator onCreateDismissAnimator() {
//        return null;
//    }
//}
