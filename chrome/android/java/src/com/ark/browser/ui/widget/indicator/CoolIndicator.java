package com.ark.browser.ui.widget.indicator;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.AnimationUtils;
import android.view.animation.Interpolator;
import android.view.animation.LinearInterpolator;
import android.widget.ProgressBar;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.chrome.R;

/**
 * @author cenxiaozhong
 * @date 2018/2/23
 * @since 1.0.0
 */
public class CoolIndicator extends ProgressBar {
    private final static int PROGRESS_DURATION = 6000;
    private final static int CLOSING_DELAY = 200;
    private final static int CLOSING_DURATION = 600;
    private final static int FINISHED_DURATION = 300;
    private ValueAnimator mPrimaryAnimator;
    private ValueAnimator mShrinkAnimator = ValueAnimator.ofFloat(0f, 1f);
    private ValueAnimator mAlphaAnimator = ValueAnimator.ofFloat(1f, 0.25f);
    private float mClipRegion = 0f;
    private int mExpectedProgress = 0;
    private Rect mTempRect;
    private boolean mIsRtl;
    private static final String TAG = CoolIndicator.class.getSimpleName();
    private boolean mIsRunning = false;
    private boolean mIsRunningCompleteAnimation = false;
    private AccelerateDecelerateInterpolator mAccelerateDecelerateInterpolator = new AccelerateDecelerateInterpolator();
    private LinearInterpolator mLinearInterpolator = new LinearInterpolator();
    private static final float LINEAR_MAX_RADIX_SEGMENT = 0.92f;
    private static final float ACCELERATE_DECELERATE_MAX_RADIX_SEGMENT = 1f;
    private boolean mWrap;
    private int mDuration;
    private int mResID;
    /**
     * 进度放大倍数
     */
    private static final int RADIX = 100;
    private AnimatorSet mClosingAnimatorSet;

    private ValueAnimator.AnimatorUpdateListener mListener = new ValueAnimator.AnimatorUpdateListener() {
        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            setProgressImmediately((int) mPrimaryAnimator.getAnimatedValue());
        }
    };

    public static CoolIndicator create(Activity activity) {
        return new CoolIndicator(activity, null, android.R.style.Widget_Material_ProgressBar_Horizontal);
    }

    public CoolIndicator(@NonNull Context context) {
        super(context, null);
        init(context, null);
    }

    public CoolIndicator(@NonNull Context context,
                         @Nullable AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs);
    }

    public CoolIndicator(@NonNull Context context,
                         @Nullable AttributeSet attrs,
                         int defStyleAttr) {

        super(context, attrs, defStyleAttr);
        init(context, attrs);
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public CoolIndicator(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init(context, attrs);
    }

    private void init(@NonNull Context context, @Nullable AttributeSet attrs) {
        mTempRect = new Rect();

        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.CoolIndicator);
        mDuration = a.getInteger(R.styleable.CoolIndicator_shiftDuration, 1000);
        mResID = a.getResourceId(R.styleable.CoolIndicator_shiftInterpolator, 0);
        mWrap = a.getBoolean(R.styleable.CoolIndicator_wrapShiftDrawable, true);

        mPrimaryAnimator = ValueAnimator.ofInt(getProgress(), getMax());
        mPrimaryAnimator.setInterpolator(new LinearInterpolator());
        mPrimaryAnimator.setDuration((long) (PROGRESS_DURATION * 0.92));
        mPrimaryAnimator.addUpdateListener(mListener);
        mPrimaryAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                super.onAnimationEnd(animation);
                if (getProgress() == getMax()) {
                    Log.i(TAG, "progress:" + getProgress() + "  max:" + getMax());
                    animateClosing();
                }
            }
        });

        mClosingAnimatorSet = new AnimatorSet();
        mAlphaAnimator.setDuration(CLOSING_DURATION);
        mAlphaAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                setAlpha((Float) animation.getAnimatedValue());
            }
        });
        mAlphaAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationCancel(Animator animation) {
                super.onAnimationCancel(animation);
                setAlpha(1f);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                super.onAnimationEnd(animation);
                setAlpha(1f);
            }
        });
        mShrinkAnimator.setDuration(CLOSING_DURATION);
        mShrinkAnimator.setInterpolator(new AccelerateDecelerateInterpolator());
        mShrinkAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator valueAnimator) {
                final float region = (float) valueAnimator.getAnimatedValue();
                if (mClipRegion != region) {
                    mClipRegion = region;
                    invalidate();
                }
            }
        });
        mShrinkAnimator.addListener(new Animator.AnimatorListener() {
            @Override
            public void onAnimationStart(Animator animator) {
                mClipRegion = 0f;
            }

            @Override
            public void onAnimationEnd(Animator animator) {
                setVisibilityImmediately(GONE);
                mIsRunning = false;
                mIsRunningCompleteAnimation = false;
            }

            @Override
            public void onAnimationCancel(Animator animator) {
                mClipRegion = 0f;
                setVisibilityImmediately(GONE);
                mIsRunning = false;
                mIsRunningCompleteAnimation = false;
            }

            @Override
            public void onAnimationRepeat(Animator animator) {
            }
        });

        mClosingAnimatorSet.playTogether(mShrinkAnimator, mAlphaAnimator);
        if (getProgressDrawable() != null) {
            setProgressDrawableImmediately(buildWrapDrawable(getProgressDrawable(), mWrap, mDuration, mResID));
        }

        a.recycle();
        setMax(100);
    }


    @Deprecated
    @Override
    public void setProgress(int nextProgress) {
    }


    private void setProgressInternal(int nextProgress) {
        nextProgress = Math.min(nextProgress, getMax());
        nextProgress = Math.max(0, nextProgress);
        mExpectedProgress = nextProgress;

        // a dirty-hack for reloading page.
        if (mExpectedProgress < getProgress() && getProgress() == getMax()) {
            setProgressImmediately(0);
        }

        if (mPrimaryAnimator != null) {
            if (nextProgress == getMax()) {
                Log.i(TAG, "finished duration:" + (FINISHED_DURATION * (1 - ((float) getProgress() / getMax()))));
                mPrimaryAnimator.setDuration((long) (FINISHED_DURATION * (1 - (((float) getProgress() / getMax())))));
                mPrimaryAnimator.setInterpolator(mAccelerateDecelerateInterpolator);
            } else {
                mPrimaryAnimator.setDuration((long) (PROGRESS_DURATION * (1 - (((float) getProgress() / (getMax() * 0.92))))));
                mPrimaryAnimator.setInterpolator(mLinearInterpolator);
            }
            mPrimaryAnimator.cancel();
            mPrimaryAnimator.setIntValues(getProgress(), nextProgress);
            mPrimaryAnimator.start();
        } else {
            setProgressImmediately(nextProgress);
        }

        if (mShrinkAnimator != null) {
            if (nextProgress != getMax()) {
                // stop closing animation
                mShrinkAnimator.cancel();
                mClipRegion = 0f;
            }
        }
    }

    private void setProgressDrawableImmediately(Drawable drawable) {
        super.setProgressDrawable(drawable);
    }

    @Override
    public void setProgressDrawable(Drawable d) {
        super.setProgressDrawable(buildWrapDrawable(d, mWrap, mDuration, mResID));
    }

    public void start() {
        if (mIsRunning) {
            return;
        }
        mIsRunning = true;
        this.setVisibility(View.VISIBLE);
        setProgressImmediately(0);
        setProgressInternal((int) (getMax() * LINEAR_MAX_RADIX_SEGMENT));
    }

    public void complete() {
        if (mIsRunningCompleteAnimation) {
            return;
        }
        if (mIsRunning) {
            mIsRunningCompleteAnimation = true;
            setProgressInternal((int) (getMax() * ACCELERATE_DECELERATE_MAX_RADIX_SEGMENT));
        }
    }

    public boolean isRunning() {
        return mIsRunning;
    }


    @Override
    public synchronized void setMax(int max) {
        super.setMax(max * RADIX);
    }

    @Override
    public void onDraw(Canvas canvas) {
        if (mClipRegion == 0) {
            super.onDraw(canvas);
        } else {
            canvas.getClipBounds(mTempRect);
            final float clipWidth = mTempRect.width() * mClipRegion;
            final int saveCount = canvas.save();


            if (mIsRtl) {
                canvas.clipRect(mTempRect.left, mTempRect.top, mTempRect.right - clipWidth, mTempRect.bottom);
            } else {
                canvas.clipRect(mTempRect.left + clipWidth, mTempRect.top, mTempRect.right, mTempRect.bottom);
            }
            super.onDraw(canvas);
            canvas.restoreToCount(saveCount);
        }
    }

    @Override
    public void setVisibility(int value) {
        if (value == GONE) {
            if (mExpectedProgress == getMax()) {
//				animateClosing();
            } else {
                setVisibilityImmediately(value);
            }
        } else {
            setVisibilityImmediately(value);
        }
    }

    private void setVisibilityImmediately(int value) {
        super.setVisibility(value);
    }

    private void animateClosing() {
        mIsRtl = (ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_RTL);

        mClosingAnimatorSet.cancel();

        final Handler handler = getHandler();
        if (handler != null) {
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    mClosingAnimatorSet.start();
                }
            }, CLOSING_DELAY);
        }
    }

    private void setProgressImmediately(int progress) {
        super.setProgress(progress);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        if (mPrimaryAnimator != null) {
            mPrimaryAnimator.cancel();
        }
        if (mClosingAnimatorSet != null) {
            mClosingAnimatorSet.cancel();
        }
        if (mShrinkAnimator != null) {
            mShrinkAnimator.cancel();
        }
        if (mAlphaAnimator != null) {
            mAlphaAnimator.cancel();
        }

    }

    private Drawable buildWrapDrawable(Drawable original, boolean isWrap, int duration, int resID) {
        if (isWrap) {
            final Interpolator interpolator = (resID > 0)
                    ? AnimationUtils.loadInterpolator(getContext(), resID)
                    : null;
            final ShiftDrawable wrappedDrawable = new ShiftDrawable(original, duration, interpolator);
            return wrappedDrawable;
        } else {
            return original;
        }
    }
}
