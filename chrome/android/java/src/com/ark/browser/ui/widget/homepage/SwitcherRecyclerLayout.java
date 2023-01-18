package com.ark.browser.ui.widget.homepage;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.TimeInterpolator;
import android.animation.TypeEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.animation.AnimationUtils;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.OvershootInterpolator;
import android.widget.OverScroller;

import androidx.core.view.ViewCompat;
import androidx.interpolator.view.animation.FastOutSlowInInterpolator;

import com.ark.browser.tab.EmptyTabInfoObserver;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.fragment.dialog.TabActionDialog;
import com.zpj.utils.ColorUtils;
import com.zpj.utils.PrefsHelper;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.Queue;

public class SwitcherRecyclerLayout extends ViewGroup {

    private static final String TAG = "SwitcherRecyclerView";

    private static final int STATE_HIDE = -1;
    private static final int STATE_IDLE = 0;
    private static final int STATE_ANIMATION = 1;
    private static final int STATE_EXPAND = 2;

    /**
     * 背景阴影颜色
     */
    private final int mShadowColor = Color.parseColor("#20000000");

    private final int mMaxV = 3000;


    // 缓存不再使用的View
    private final Queue<View> mViewQueue = new ArrayDeque<>();

    // 缓存屏幕中存在的item
    private final List<View> viewList = new ArrayList<>();

    private final ViewFlinger mViewFlinger;

    // 最小触摸距离
    private final int touchSlop;
    // 最大惯性滚动速度
    private final int mMaxVelocity;

    // 速度追踪
    private VelocityTracker mVelocityTracker;

    // 适配器
    private Adapter adapter;

    // 当前状态
    private int mState = STATE_IDLE;

    // 屏幕中第一个可见item的position
    private int mFirstPosition;

    // 当前view的position
    private int mPosition;

    // 当前触摸的itemView
    private View mCurrentTouchView;


    // 宽高
    private int mWidth;
    private int mHeight;

    // itemView的宽高
    private int mChildWidth;
    private int mChildHeight;

    private int mTitleHeight;

    // itemView之间的间距
    private int mGapSize = 50;

    private int originX;
    private float mDownX;
    private float mDownY;
    private float mLastX;
    private float mLastY;

    private float mBackgroundAlpha = 0f;

    private boolean needLayout;
    private boolean isSwipeMode;
    private boolean isScrollMode;

    public SwitcherRecyclerLayout(Context context) {
        this(context, null);
    }

    public SwitcherRecyclerLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        setBackgroundColor(mShadowColor);
        mViewFlinger = new ViewFlinger(context);
        ViewConfiguration configuration = ViewConfiguration.get(context);
        touchSlop = configuration.getScaledTouchSlop();
        mMaxVelocity = configuration.getScaledMaximumFlingVelocity();
        needLayout = true;
        mTitleHeight = (int) (context.getResources().getDisplayMetrics().density * 56);
        mPosition = getPosition();
    }

    public void setAdapter(Adapter adapter) {
        Log.d(TAG, "setAdapter: ");
        this.adapter = adapter;
        if (adapter != null) {
            viewList.clear();
            notifyDataSetChanged();
        }
        TabListManager.getInstance().getCurrentTabList().addObserver(new EmptyTabInfoObserver() {

            @Override
            public void didAddTab(ITab tab, @TabSelectionType int type) {
                Log.d(TAG, "didAddTab state=" + mState);
                mPosition = TabListManager.getInstance().getCurrentTabList().getIndex();
                mCurrentTouchView = null;
                initChildren();
            }

        });
    }

    public void notifyDataSetChanged() {
        mPosition = getPosition();
        needLayout = true;
        requestLayout();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        Log.d(TAG, "onMeasure: ");
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int widthSize = MeasureSpec.getSize(widthMeasureSpec);
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);
        mChildWidth = (int) (widthSize * 0.68f);
        mChildHeight = (int) (heightSize * 0.68f);
        Log.d(TAG, "onMeasure: width=" + widthSize + " height=" + heightSize);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        Log.d(TAG, "onLayout:");
        changed = needLayout || (changed && (r - l != mWidth || (b - t != mHeight)));
        Log.d(TAG, "onLayout changed=" + changed + " viewList.size=" + viewList.size() + " childCount=" + getChildCount());
        needLayout = false;
        if (changed) {
            mWidth = r - l;
            mHeight = b - t;

            viewList.clear();
            removeAllViews();
            if (adapter != null) {
                if (getCount() == 0) {
                    return;
                }
                mWidth = r - l;
                mHeight = b - t;

                int left = (mWidth - mChildWidth) / 2;
                int right;
                int top = (mHeight - mChildHeight) / 2;
                int bottom = top + mChildHeight;


                int end;
                if (mPosition < 0) {
                    mPosition = 0;
                } else if (mPosition > getCount() - 1) {
                    mPosition = getCount() - 1;
                }
                if (mPosition == getCount() - 1) {
                    if (mPosition == 0) {
                        mFirstPosition = 0;
                    } else {
                        mFirstPosition = mPosition - Math.min(2, getCount() - 1);
                        left -= (mPosition - mFirstPosition) * (mChildWidth + mGapSize);
                    }
                    end = mPosition;
                } else if (mPosition > 0) {
                    left -= (mChildWidth + mGapSize);
                    mFirstPosition = mPosition - 1;
                    end = mPosition + 1;
                } else {
                    mFirstPosition = 0;
//                    end = 2;
                    end = Math.min(2, Math.max(0, getCount() - 1));
                }
                for (int i = mFirstPosition; i <= end; i++) {
                    right = left + mChildWidth;
                    View view = makeAndStep(i, left, top, right, bottom, i - mFirstPosition);
                    viewList.add(view);
                    left = right + mGapSize;
                }

            }

        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        Log.d(TAG, "onInterceptTouchEvent event=" + MotionEvent.actionToString(ev.getAction()));
        boolean intercept = false;
        switch (ev.getAction()) {
            case MotionEvent.ACTION_DOWN:
                if (mVelocityTracker == null) {
                    mVelocityTracker = VelocityTracker.obtain();
                }
                mVelocityTracker.addMovement(ev);
                mViewFlinger.stop();
                if (mSwipeAnimator != null) {
                    mSwipeAnimator.cancel();
                    mSwipeAnimator = null;
                }
                mDownX = ev.getRawX();
                mDownY = ev.getRawY();
                originX = (int) ev.getRawX();
                mLastX = originX;
                mLastY = mDownY;
                isSwipeMode = false;
                isScrollMode = false;
                break;
            case MotionEvent.ACTION_MOVE:
                if (getCount() == 0 || mState != STATE_IDLE) {
                    return false;
                }
                if (Math.abs(originX - ev.getRawX()) < touchSlop && Math.abs(mDownY - ev.getRawY()) < touchSlop) {
                    return false;
                }
//                if (!(ChromeActivity.fromContext(getContext()).getTopFragment() instanceof LauncherFragment)) {
//                    return false;
//                }
                if (mCurrentTouchView != null) {
                    mCurrentTouchView.cancelLongPress();
                }
                if (isSwipeMode || isScrollMode) {
                    return true;
                }
                boolean isSwipeable = true;
                if (mCurrentTouchView != null && adapter != null) {
                    isSwipeable = !adapter.isLocked(mFirstPosition + indexOfChild(mCurrentTouchView));
                }
                if (isSwipeable && mCurrentTouchView != null && ev.getRawY() < mDownY
                        && Math.abs(ev.getRawY() - mDownY) > Math.abs(ev.getRawX() - mDownX)) {
                    isSwipeMode = true;
                    intercept = true;
                } else if (Math.abs(ev.getRawX() - mDownX) >= Math.abs(ev.getRawY() - mDownY)) {
                    isScrollMode = true;
                    intercept = true;
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (mState != STATE_IDLE) {
                    return false;
                }
                break;
        }
        return intercept;
    }


    private ValueAnimator mSwipeAnimator;

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        Log.d(TAG, "onTouch onTouchEvent event=" + MotionEvent.actionToString(event.getAction()));
        if (mState != STATE_IDLE) {
            return false;
        }
        if (mVelocityTracker == null) {
            mVelocityTracker = VelocityTracker.obtain();
        }
        mVelocityTracker.addMovement(event);

        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                super.onTouchEvent(event);
                break;
            case MotionEvent.ACTION_MOVE:
                if (getCount() == 0) {
                    super.onTouchEvent(event);
                    break;
                }

                if (Math.abs(originX - event.getRawX()) < touchSlop && Math.abs(mDownY - event.getRawY()) < touchSlop) {
                    return true;
                }

//                ISupportFragment topFragment = ChromeActivity.fromContext(getContext()).getTopFragment();
//                if (topFragment instanceof LauncherFragment) {
//                    if (((LauncherFragment) topFragment).getTopChildFragment() != null) {
//                        return true;
//                    }
//                } else {
//                    return true;
//                }
//                if (!(ChromeActivity.fromContext(getContext()).getTopFragment() instanceof LauncherFragment)) {
//                    return true;
//                }

                if (mCurrentTouchView != null) {
                    mCurrentTouchView.cancelLongPress();
                    event.setAction(MotionEvent.ACTION_CANCEL);
                    super.onTouchEvent(event);
                }

                boolean isSwipeable = true;
                if (mCurrentTouchView != null && adapter != null) {
                    isSwipeable = !adapter.isLocked(mFirstPosition + indexOfChild(mCurrentTouchView));
                }
                if (isSwipeable && !isSwipeMode && mCurrentTouchView != null && event.getRawY() < mDownY
                        && Math.abs(event.getRawY() - mDownY) > Math.abs(event.getRawX() - mDownX)) {
                    isSwipeMode = true;
                } else if (!isScrollMode && Math.abs(event.getRawX() - mDownX) >= Math.abs(event.getRawY() - mDownY)) {
                    isScrollMode = true;
                }


                if (isSwipeMode) {

                    float y = event.getRawY();
                    float dy = y - mLastY;
                    mLastY = y;

                    swipeBy(dy);
                    return true;
                } else if (isScrollMode) {
                    float x = event.getRawX();
                    int dx = (int) (x - mLastX);
                    mLastX = x;
                    scrollBy(dx, 0);
                } else {
                    super.onTouchEvent(event);
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (isSwipeMode) {
                    mVelocityTracker.computeCurrentVelocity(1000, mMaxVelocity);
                    Log.d(TAG, "swipe getYVelocity=" + mVelocityTracker.getYVelocity());
                    mVelocityTracker.clear();
                    mVelocityTracker.recycle();
                    mVelocityTracker = null;

                    mState = STATE_ANIMATION;
                    mSwipeAnimator = ValueAnimator.ofFloat(0, 1f);
                    mSwipeAnimator.setInterpolator(new FastOutSlowInInterpolator());

                    // TODO 采用Scroller改写
                    if (mCurrentTouchView.getAlpha() > 0.72f) { // 0.72f
                        mSwipeAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                            float last = 0;
                            final int dx = (mHeight + mChildHeight) / 2 - mCurrentTouchView.getBottom();

                            @Override
                            public void onAnimationUpdate(ValueAnimator valueAnimator) {
                                float percent = (float) (valueAnimator.getAnimatedValue());
                                float temp = percent * dx;
                                float delta = temp - last;
                                last = temp;
                                swipeBy(delta);
                            }
                        });
                        mSwipeAnimator.setInterpolator(new OvershootInterpolator(1f));
                        mSwipeAnimator.addListener(new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationEnd(Animator animation) {
                                onAnimationCancel(animation);
                                isSwipeMode = false;
                            }


                            @Override
                            public void onAnimationCancel(Animator animation) {
                                super.onAnimationCancel(animation);
                                mState = STATE_IDLE;
                            }
                        });
                    } else {
                        mSwipeAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                            float last = 0;
                            final int dx = -mCurrentTouchView.getBottom();

                            @Override
                            public void onAnimationUpdate(ValueAnimator valueAnimator) {
                                float percent = (float) (valueAnimator.getAnimatedValue());
                                float temp = percent * dx;
                                float delta = temp - last;
                                last = temp;
                                swipeBy(delta);
                            }
                        });
                        mSwipeAnimator.addListener(new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationEnd(Animator animation) {
                                super.onAnimationEnd(animation);
                                int pos = mFirstPosition + indexOfChild(mCurrentTouchView);
                                if (pos == getCount() - 1) {
                                    mPosition = Math.max(pos - 1, 0);
                                    savePosition();
                                }
                                if (adapter != null) {
                                    adapter.onSwipe(pos);
                                }
                                removeView(mCurrentTouchView);
                                viewList.remove(mCurrentTouchView);
                                mCurrentTouchView.setAlpha(1f);
                                mCurrentTouchView = null;
                                onAnimationCancel(animation);
                                isSwipeMode = false;
                                for (Callback callback : mCallbackList) {
                                    callback.onSwipe(pos);
                                }
                            }

                            @Override
                            public void onAnimationCancel(Animator animation) {
                                super.onAnimationCancel(animation);
                                mState = STATE_IDLE;
                            }
                        });
                    }

                    mSwipeAnimator.setDuration(300);
                    mSwipeAnimator.start();

                    return true;
                } else if (isScrollMode) {
                    isScrollMode = false;
                    mVelocityTracker.computeCurrentVelocity(1000, mMaxVelocity);
                    mViewFlinger.fling(mVelocityTracker.getXVelocity());
                    mVelocityTracker.clear();
                    mVelocityTracker.recycle();
                    mVelocityTracker = null;
                } else {
                    if (Math.abs(mDownX - event.getRawX()) < touchSlop
                            && Math.abs(mDownY - event.getRawY()) < touchSlop) {
                        close();
                        return true;
                    }
                    super.onTouchEvent(event);
                }

                break;
        }
        return true;
    }

    private int maxDx = 0;

    @Override
    public void scrollBy(int dx, int dy) {
        if (Math.abs(dx) > Math.abs(maxDx)) {
            maxDx = dx;
            Log.d(TAG, "maxDx=" + maxDx);
        }

        Log.d(TAG, "dx=" + dx + " dy=" + dy + " viewSize=" + viewList.size());

        View firstView = getChildAt(0);
        if (firstView == null) {
            return;
        }
        View visibleView = firstView;
        View lastView = getChildAt(getChildCount() - 1);
        int firstLeft = firstView.getLeft() + dx;
        int firstRight = firstView.getRight() + dx;
        int lastLeft = lastView.getLeft() + dx;
        int lastRight = lastView.getRight() + dx;
        Log.d(TAG, "firstLeft=" + firstLeft + " firstRight=" + firstRight + " lastLeft=" + lastLeft + " lastRight=" + lastRight);

        int gap = (mWidth - mChildWidth) / 2;
        // 向右滑动
        if (dx > 0) {
            Log.d(TAG, "yyyyyyyyy firstRight=" + firstRight + " lastRight=" + lastRight + " width=" + mWidth + " firstRow=" + mFirstPosition);
            if (firstLeft > 0) { // 0   -mChildWidth * 2
                if (mFirstPosition > 0) {
                    View view = obtainView(--mFirstPosition, 0);
                    viewList.add(0, view);
                    visibleView = getChildAt(1);
                    Log.d(TAG, "yyyyyyyyy add");
                }
            }

            if (lastLeft > mWidth) { // width   + mChildWidth * 2
                if (firstLeft < gap) {
                    removeView(viewList.remove(viewList.size() - 1));
                    Log.d(TAG, "yyyyyyyyy removed");
                }
            }
        } else { // 向左滑动
            Log.d(TAG, "xxxxxxxx firstRight=" + firstRight + " lastRight=" + lastRight + " width=" + mWidth + " firstRow=" + mFirstPosition);
            if (firstRight < 0) { // 0 -mGapSize
                if (lastRight > mWidth - gap) {
                    removeView(viewList.remove(0));
                    mFirstPosition++;
                    visibleView = getChildAt(0);
                    Log.d(TAG, "xxxxxxxx removed");
                }
            }

            if (lastRight < mWidth) { // width + mGapSize
                int nextItemIndex = viewList.size() + mFirstPosition;

                if (nextItemIndex < getCount()) {
                    Log.d(TAG, "xxxxxxxx add");
                    View view = obtainView(nextItemIndex, viewList.size());
                    viewList.add(viewList.size(), view);
                }
            }
            Log.d(TAG, "xxxxxxxx viewSize=" + viewList.size());
        }
        Log.d(TAG, "viewSize=" + viewList.size() + " dx=" + dx + " firstRow=" + mFirstPosition);


        int left;
        int right;
        int top = (mHeight - mChildHeight) / 2;
        int bottom = top + mChildHeight;

        int visibleLeft = visibleView.getLeft() + dx;
        int visibleRight = visibleView.getRight() + dx;

        int index = indexOfChild(visibleView);
        Log.d(TAG, "visibleLeft=" + visibleLeft + " visibleRight=" + visibleRight + " index=" + index);
        visibleView.layout(visibleLeft, top, visibleRight, bottom);
        for (int i = 0; i < viewList.size(); i++) {
            View view = viewList.get(i);
            if (view == visibleView) {
                continue;
            }
            int deltaIndex = i - index;
            int delta = deltaIndex * (mChildWidth + mGapSize);
            left = visibleLeft + delta;
            right = visibleRight + delta;
            view.layout(left, top, right, bottom);
            Log.d(TAG, "left=" + left + " right=" + right);
        }

    }

    @Override
    public void removeView(View view) {
        super.removeView(view);
        mViewQueue.add(view);
    }

    @Override
    public boolean isSelected() {
        return mState == STATE_EXPAND;
    }

    public View getSelectedView() {
        return mCurrentTouchView;
    }

    private void initChildren() {
        Log.d(TAG, "initChildren");
        int index;
        if (mCurrentTouchView != null) {
            index = indexOfChild(mCurrentTouchView);
            mPosition = mFirstPosition + index;
        } else {
            index = mPosition - mFirstPosition;
        }
        Log.d(TAG, "initChildren index=" + index + " mPosition=" + mPosition + " mFirstPosition=" + mFirstPosition);
        if (index == 0 && mPosition > 0) {
            View child = obtainView(mPosition - 1, 0);
            viewList.add(0, child);
            mFirstPosition--;
        } else if (index == getChildCount() - 1 && mPosition < getCount() - 1) {
            int size = viewList.size();
            View child = obtainView(mPosition + 1, size);
            viewList.add(size, child);
        }
        Log.d(TAG, "initChildren mPosition=" + mPosition + " mFirstPosition=" + mFirstPosition);
    }

    public void close() {
        if (getCount() == 0) {
            Rect rect = new Rect();
            new TransitionAnim(rect, rect, STATE_HIDE)
                    .setDuration(360)
                    .start();
            return;
        }
        selectCenterChildView();
        if (mCurrentTouchView == null) {
            return;
        }
        int index = indexOfChild(mCurrentTouchView);
        Rect endRect = new Rect();
        int delta = getChildCount() - index;
        endRect.left = -delta * (mChildWidth + mGapSize);
        endRect.right = endRect.left + mChildWidth;
        endRect.top = (mHeight - mChildHeight) / 2;
        endRect.bottom = endRect.top + mChildHeight;

        Rect startRect = new Rect(mCurrentTouchView.getLeft(), endRect.top, mCurrentTouchView.getRight(), endRect.bottom);
        new TransitionAnim(startRect, endRect, STATE_HIDE)
                .setDuration(360)
                .setTransitionCallback(new TransitionCallback() {

                    @Override
                    public void onTransition(Rect targetRect, View targetView, float percent) {
                        int index = indexOfChild(targetView);

                        int widthSpec = MeasureSpec.makeMeasureSpec(mChildWidth, MeasureSpec.EXACTLY);
                        int heightSpec = MeasureSpec.makeMeasureSpec(mChildHeight, MeasureSpec.EXACTLY);
                        int rRight = (int) (startRect.right + percent * (mWidth - startRect.right));
                        for (int i = 0; i < getChildCount(); i++) {
                            View child = getChildAt(i);
                            child.setAlpha(1 - percent);
                            if (child == targetView) {
                                continue;
                            }
                            int delta = i - index;
                            int left, right;
                            if (delta > 0) {
                                right = rRight + delta * (mGapSize + mChildWidth);
                                left = right - mChildWidth;
                            } else {
                                left = targetRect.left + delta * (mGapSize + mChildWidth);
                                right = left + mChildWidth;
                            }
                            child.measure(widthSpec, heightSpec);
                            child.layout(left, targetRect.top, right, targetRect.bottom);
                        }
                    }

                    @Override
                    public void onTransitionEnd() {
                        int index = indexOfChild(mCurrentTouchView);
                        mPosition = mFirstPosition + index;
                        if (index == 0 && mPosition > 0) {
                            View child = obtainView(mPosition - 1, 0);
                            viewList.add(0, child);
                            mFirstPosition--;
                        } else if (index == getChildCount() - 1 && mPosition < getCount() - 1) {
                            int size = viewList.size();
                            View child = obtainView(mPosition + 1, size);
                            viewList.add(size, child);
                        }
                    }
                })
                .start();
    }

    public void open() {
        if (getCount() == 0) {
            Rect rect = new Rect();
            new TransitionAnim(rect, rect, STATE_IDLE)
                    .setDuration(360)
                    .setInterpolator(new OvershootInterpolator(0.5f))
                    .setTransitionCallback(new TransitionCallback() {
                        @Override
                        public void onTransition(Rect targetRect, View targetView, float percent) {
                            for (Callback callback : mCallbackList) {
                                callback.onOpen(percent);
                            }
                        }
                    })
                    .start();
            return;
        }
        selectCenterChildView();
        int index = indexOfChild(mCurrentTouchView);
        Log.d(TAG, "open index=" + index + " count=" + getChildCount());
        Rect startRect = new Rect();
        int delta = getChildCount() - index;
        startRect.left = -delta * (mChildWidth + mGapSize);
        startRect.right = startRect.left + mChildWidth;
        startRect.top = (mHeight - mChildHeight) / 2;
        startRect.bottom = startRect.top + mChildHeight;
        int endLeft = (mWidth - mChildWidth) / 2;
        int endRight = endLeft + mChildWidth;

        int widthSpec = MeasureSpec.makeMeasureSpec(mChildWidth, MeasureSpec.EXACTLY);
        int heightSpec = MeasureSpec.makeMeasureSpec(mChildHeight, MeasureSpec.EXACTLY);

        Rect endRect = new Rect(endLeft, startRect.top, endRight, startRect.bottom);
        new TransitionAnim(startRect, endRect, STATE_IDLE)
                .setDuration(360)
                .setInterpolator(new OvershootInterpolator(0.5f))
                .setTransitionCallback(new TransitionCallback() {

                    private boolean isOver;

                    @Override
                    public void onTransition(Rect targetRect, View targetView, float percent) {
                        for (Callback callback : mCallbackList) {
                            callback.onOpen(percent);
                        }
                        int rRight;
                        if (isOver || percent >= 1f) {
                            isOver = true;
                            rRight = targetRect.right;
                        } else {
                            rRight = (int) (mWidth - percent * endLeft);
                        }
                        for (int i = 0; i < getChildCount(); i++) {
                            View child = getChildAt(i);
                            child.setAlpha(percent);
                            if (child == targetView) {
                                continue;
                            }
                            int delta = i - index;
                            int left, right;
                            if (delta > 0) {
                                right = rRight + delta * (mGapSize + mChildWidth);
                                left = right - mChildWidth;
                            } else {
                                left = targetRect.left + delta * (mGapSize + mChildWidth);
                                right = left + mChildWidth;
                            }
                            child.measure(widthSpec, heightSpec);
                            child.layout(left, targetRect.top, right, targetRect.bottom);
                        }
                    }

                })
                .start();
    }

    public void goToIdle() {
        mPosition = TabListManager.getInstance().getCurrentTabList().getIndex();
        selectChildView();
//        selectCenterChildView();
        Log.d(TAG, "goToIdle index=" + indexOfChild(mCurrentTouchView) + " count=" + getChildCount());
        goToIdle(true, new Rect(0, -mTitleHeight, mWidth, mHeight));
    }

    private void goToIdle(boolean updateChild, Rect startRect) {

        int startTop = (mHeight - mChildHeight) / 2;
        int startBottom = startTop + mChildHeight;

        int startLeft = (mWidth - mChildWidth) / 2;
        int startRight = startLeft + mChildWidth;

        Rect endRect = new Rect(startLeft, startTop, startRight, startBottom);
        startTransitionAnim(startRect, endRect, STATE_IDLE, updateChild);

    }

    public void goToSelect(View view, int position) {
        mCurrentTouchView = view;
        int childTop = (mHeight - mChildHeight) / 2;
        int childBottom = childTop + mChildHeight;
        int left = view.getLeft();
        Rect startRect = new Rect(left, childTop, left + mChildWidth, childBottom);
        Rect endRect = new Rect(0, -mTitleHeight, mWidth, mHeight);
        startTransitionAnim(startRect, endRect, STATE_EXPAND, true);
    }

    public void dragToSwitchTab(float dx, float dy) {
        mPosition = TabListManager.getInstance().getCurrentTabList().getIndex();
        selectChildView();

        int height = mHeight + mTitleHeight - (int) Math.abs(dy) * 3 / 2;
        height = Math.max((int) (mHeight * 0.42), height);
        int width = (int) ((float) height / (mHeight + mTitleHeight) * mWidth);
        int left = (int) ((mWidth - width) / 2f + dx);
        int right = left + width;

        int bottom = mHeight - (int) (Math.abs(dy));
        int top = bottom - height;
        if (height == (int) (mHeight * 0.42) && top < 0) {
            top = 0;
            bottom = top + height;
        }

        Log.d(TAG, "left=" + left + " top=" + top + " right=" + right + " bottom=" + bottom);
        final int widthSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);
        final int heightSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);
        mCurrentTouchView.measure(widthSpec, heightSpec);
        mCurrentTouchView.layout(left, top, right, bottom);

        int index = indexOfChild(mCurrentTouchView);
        int childWidth = right - left;
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            child.setAlpha(1f);
            if (child == mCurrentTouchView) {
                continue;
            }
            int delta = i - index;
            int childLeft;
            int childRight;
            if (delta > 0) {
                childRight = right + delta * (mGapSize + childWidth);
                childLeft = childRight - childWidth;
            } else {
                childLeft = left + delta * (mGapSize + childWidth);
                childRight = childLeft + childWidth;
            }
            child.measure(widthSpec, heightSpec);
            child.layout(childLeft, top, childRight, bottom);
        }


        mBackgroundAlpha = 1f - (float) width / (getWidth() - mChildWidth);
        setBackgroundColor(ColorUtils.alphaColor(mShadowColor, mBackgroundAlpha));
    }

    public void endDragToSwitchTab(float velocityX) {
        if (mCurrentTouchView == null) {
            selectChildView(mPosition);
        }

//        int position = TabListManager.getInstance().getCurrentTabList().getIndex();
        mState = STATE_ANIMATION;

        int delta = mCurrentTouchView.getRight() + mCurrentTouchView.getLeft() - mWidth;
        Log.d(TAG, "endDragToSwitchTab delta=" + delta + " velocityX=" + velocityX
                + " (width - childWidth)=" + (mWidth - mChildWidth));

        int index = indexOfChild(mCurrentTouchView);
        if (Math.abs(delta) > (mWidth - mChildWidth)) {
            if (delta > 0) {
                if (index > 0) {
                    mCurrentTouchView = getChildAt(index - 1);
                }
            } else {
                if (index < getChildCount() - 1) {
                    mCurrentTouchView = getChildAt(index + 1);
                }
            }
        } else if (Math.abs(velocityX) > 3000) {
            if (velocityX > 0) {
                if (index > 0) {
                    mCurrentTouchView = getChildAt(index - 1);
                }
            } else {
                if (index < getChildCount() - 1) {
                    mCurrentTouchView = getChildAt(index + 1);
                }
            }
        }

        Rect startRect = new Rect(mCurrentTouchView.getLeft(), mCurrentTouchView.getTop(),
                mCurrentTouchView.getRight(), mCurrentTouchView.getBottom());
        Rect endRect = new Rect(0, -mTitleHeight, mWidth, mHeight);
        int duration;
        if (velocityX == 0) {
            duration = 540;
        } else {
            duration = (int) (Math.min(540, Math.max(250, Math.abs((startRect.centerX() - endRect.centerX()) / velocityX * 500f))));
        }
        Log.d(TAG, "endDragToSwitchTab duration=" + duration);
        new TransitionAnim(startRect, endRect, STATE_EXPAND)
                .setDuration(duration)
                .setInterpolator(AnimationUtils.loadInterpolator(getContext(), R.anim.fast_out_extra_slow_in))
                .setTransitionCallback(new TransitionCallback() {

                    @Override
                    public void onTransition(Rect targetRect, View targetView, float percent) {
                        int index = indexOfChild(targetView);
                        int childWidth = targetRect.width();
                        int widthSpec = MeasureSpec.makeMeasureSpec(childWidth, MeasureSpec.EXACTLY);
                        int heightSpec = MeasureSpec.makeMeasureSpec(targetRect.height(), MeasureSpec.EXACTLY);
                        for (int i = 0; i < getChildCount(); i++) {
                            View child = getChildAt(i);
                            child.setAlpha(1f);
                            if (child == targetView) {
                                continue;
                            }
                            int delta = i - index;
                            int left, right;
                            if (delta > 0) {
                                right = targetRect.right + delta * (mGapSize + childWidth);
                                left = right - childWidth;
                            } else {
                                left = targetRect.left + delta * (mGapSize + childWidth);
                                right = left + childWidth;
                            }
                            child.measure(widthSpec, heightSpec);
                            child.layout(left, targetRect.top, right, targetRect.bottom);
                        }
                    }

                    @Override
                    public void onTransitionEnd() {
                        int index = indexOfChild(mCurrentTouchView);
                        mPosition = mFirstPosition + index;
                        if (index == 0 && mPosition > 0) {
                            View child = obtainView(mPosition - 1, 0);
                            viewList.add(0, child);
                            mFirstPosition--;
                        } else if (index == getChildCount() - 1 && mPosition < getCount() - 1) {
                            int size = viewList.size();
                            View child = obtainView(mPosition + 1, size);
                            viewList.add(size, child);
                        }
                    }
                })
                .start();


//        index = indexOfChild(mCurrentTouchView);
//        mPosition = mFirstPosition + index;
//        for (Callback callback : mCallbackList) {
//            callback.onBeforeExpand(mPosition);
//        }
//
//        new FlingAnim(getContext(), new OnFlingListener() {
//            @Override
//            public boolean onFling(int x) {
//
//                float percent = (float) x / (endRect.centerX() - startRect.centerX());
//                Log.d(TAG, "FlingAnim onFling x=" + x + " percent=" + percent);
//                percent = Math.min(percent, 1f);
//
//                int targetLeft = (int) (startRect.left - (startRect.left - endRect.left) * percent);
//                int targetTop = (int) (startRect.top - (startRect.top - endRect.top) * percent);
//                int targetRight = (int) (startRect.right - (startRect.right - endRect.right) * percent);
//                int targetBottom = (int) (startRect.bottom - (startRect.bottom - endRect.bottom) * percent);
//
//                int childWidth = targetRight - targetLeft;
//                int widthSpec = MeasureSpec.makeMeasureSpec(childWidth, MeasureSpec.EXACTLY);
//                int heightSpec = MeasureSpec.makeMeasureSpec(targetBottom - targetTop, MeasureSpec.EXACTLY);
//
//                mCurrentTouchView.measure(widthSpec, heightSpec);
//                mCurrentTouchView.layout(targetLeft, targetTop, targetRight, targetBottom);
//
//                int index = indexOfChild(mCurrentTouchView);
//                for (int i = 0; i < getChildCount(); i++) {
//                    View child = getChildAt(i);
//                    child.setAlpha(1f);
//                    if (child == mCurrentTouchView) {
//                        continue;
//                    }
//                    int delta = i - index;
//                    int left, right;
//                    if (delta > 0) {
//                        right = targetRight + delta * (mGapSize + childWidth);
//                        left = right - childWidth;
//                    } else {
//                        left = targetLeft + delta * (mGapSize + childWidth);
//                        right = left + childWidth;
//                    }
//                    child.measure(widthSpec, heightSpec);
//                    child.layout(left, targetTop, right, targetBottom);
//                }
//
//
//                return percent < 1f;
//            }
//
//            @Override
//            public void onStop() {
//                Log.d(TAG, "FlingAnim onStop");
//                mState = STATE_EXPAND;
//                int index = indexOfChild(mCurrentTouchView);
//                mPosition = mFirstPosition + index;
//                if (index == 0 && mPosition > 0) {
//                    View child = obtainView(mPosition - 1, 0);
//                    viewList.add(0, child);
//                    mFirstPosition--;
//                } else if (index == getChildCount() - 1 && mPosition < getCount() - 1) {
//                    int size = viewList.size();
//                    View child = obtainView(mPosition + 1, size);
//                    viewList.add(size, child);
//                }
//                for (Callback callback : mCallbackList) {
//                    callback.onExpand(mPosition);
//                }
//            }
//        }).fling(velocityX);

    }

    public void moveDrag(float dx, float dy) {
        mPosition = TabListManager.getInstance().getCurrentTabList().getIndex();

        if (getChildCount() == 0) {

        }

        selectChildView();

        int height = mHeight + mTitleHeight - (int) Math.abs(dy) * 3 / 2;
        height = Math.max((int) (mHeight * 0.42), height);
        int width = (int) ((float) height / (mHeight + mTitleHeight) * mWidth);
        int left = (int) ((mWidth - width) / 2f + dx);
        int right = left + width;

        int bottom = mHeight - (int) (Math.abs(dy));
        int top = bottom - height;
        if (height == (int) (mHeight * 0.42) && top < 0) {
            top = 0;
            bottom = top + height;
        }

        Log.d(TAG, "left=" + left + " top=" + top + " right=" + right + " bottom=" + bottom);
        final int widthSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);
        final int heightSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child != mCurrentTouchView) {
                child.setAlpha(0f);
            }
        }
        mCurrentTouchView.setAlpha(1f);
        mCurrentTouchView.measure(widthSpec, heightSpec);
        mCurrentTouchView.layout(left, top, right, bottom);

        mBackgroundAlpha = 1f - (float) width / (getWidth() - mChildWidth);
        setBackgroundColor(ColorUtils.alphaColor(mShadowColor, mBackgroundAlpha));
    }


    public void endDrag(float velocityX, float velocityY) {
        Log.d(TAG, "endDrag velocityX=" + velocityX + " velocityY=" + velocityY);
        mState = STATE_ANIMATION;
        float percent = (float) mCurrentTouchView.getWidth() / mWidth;

        int left;
        int top;
        int right;
        int bottom;
        int nextState;
        Rect endRect;
        if ((percent > 0.86f && velocityY > -1000) || (percent < 0.86f && velocityY > 2000)) { // percent > 0.86 || velocityY > mMaxV
            nextState = STATE_EXPAND;
            endRect = new Rect(0, -mTitleHeight, mWidth, mHeight);

            View view = mCurrentTouchView;
            Rect startRect = new Rect(view.getLeft(), view.getTop(), view.getRight(), view.getBottom());


            int duration = Math.min((int) (Math.abs((endRect.bottom - startRect.bottom) / velocityY * 1000)), 300);

            new TransitionAnim(startRect, endRect, nextState)
                    .setDuration(duration)
                    .setInterpolator(new DecelerateInterpolator())
                    .setTransitionCallback(new TransitionCallback() {
                        @Override
                        public void onTransition(Rect targetRect, View targetView, float percent) {
                            int childTop = (mHeight - mChildHeight) / 2;
                            int childBottom = childTop + mChildHeight;
                            int targetLeft = targetRect.left;
                            int targetTop = targetRect.top;
                            int targetRight = targetRect.right;
                            int targetBottom = targetRect.bottom;
                            mCurrentTouchView.measure(MeasureSpec.makeMeasureSpec(targetRight - targetLeft, MeasureSpec.EXACTLY),
                                    MeasureSpec.makeMeasureSpec(targetBottom - targetTop, MeasureSpec.EXACTLY));
                            mCurrentTouchView.layout(targetLeft, targetTop, targetRight, targetBottom);
                        }
                    })
                    .start();

//            final int index = indexOfChild(mCurrentTouchView);
//            final int position = mFirstPosition + index;
//            for (Callback callback : mCallbackList) {
//                callback.onBeforeExpand(position);
//            }
//            float startAlpha = mBackgroundAlpha;
//            DecelerateInterpolator interpolator = new DecelerateInterpolator();
//            new ReturnFlinger(getContext(), new FlingerListener() {
//                @Override
//                public void onFling(int deltaY) {
////                    int deltaY = y - startRect.centerY();
//
//                    float percent = interpolator.getInterpolation((float) deltaY / (endRect.centerY() - startRect.centerY()));
//                    mBackgroundAlpha = (1f - percent) * startAlpha;
//                    setBackgroundColor(ColorUtils.alphaColor(mShadowColor, mBackgroundAlpha));
//
//
//                    int targetLeft = (int) (startRect.left - (startRect.left - endRect.left) * percent);
//                    int targetTop = (int) (startRect.top - (startRect.top - endRect.top) * percent);
//                    int targetRight = (int) (startRect.right - (startRect.right - endRect.right) * percent);
//                    int targetBottom = (int) (startRect.bottom - (startRect.bottom - endRect.bottom) * percent);
//
//                    mCurrentTouchView.measure(MeasureSpec.makeMeasureSpec(targetRight - targetLeft, MeasureSpec.EXACTLY),
//                            MeasureSpec.makeMeasureSpec(targetBottom - targetTop, MeasureSpec.EXACTLY));
//                    mCurrentTouchView.layout(targetLeft, targetTop, targetRight, targetBottom);
//                }
//
//                @Override
//                public void onStop() {
//                    mState = STATE_EXPAND;
//                    if (index == 0 && position > 0) {
//                        View child = obtainView(position - 1, 0);
//                        viewList.add(0, child);
//                        mFirstPosition--;
//                    } else if (index == getChildCount() - 1 && position < getCount() - 1) {
//                        int size = viewList.size();
//                        View child = obtainView(position + 1, size);
//                        viewList.add(size, child);
//                    }
//                    for (Callback callback : mCallbackList) {
//                        callback.onExpand(position);
//                    }
//                }
//            }).fling(endRect.centerY() - startRect.centerY(), Math.min(velocityY, mMaxVelocity));


        } else if (velocityY < -5000) { // -mMaxV
            int currentX = mCurrentTouchView.getLeft() + mCurrentTouchView.getWidth() / 2;
            int currentY = mCurrentTouchView.getTop() + mTitleHeight + (mCurrentTouchView.getHeight() - mTitleHeight) / 2;
            int targetX = getWidth() / 2;
            int targetY = getHeight() / 2;

            int duration = 300;

            float velocityRatio = (Math.min(-velocityY, mMaxVelocity) - mMaxV) / (mMaxVelocity - mMaxV);
            float dx = (targetX - currentX) + velocityRatio * (velocityX > 0 ? targetX : -targetX);
            float dy = (targetY - currentY) - velocityRatio * targetY;

            int tempWidth = mCurrentTouchView.getWidth();
            int tempHeight = mCurrentTouchView.getHeight() - mTitleHeight;

            int targetWidth = 0;
            int targetHeight = 0;

            final int index = indexOfChild(mCurrentTouchView);
            final int position = mFirstPosition + index;
            for (Callback callback : mCallbackList) {
                callback.onBeforeHide(position);
            }

            float startAlpha = mBackgroundAlpha;

            ValueAnimator animator = ValueAnimator.ofObject(
                    new BezierEvaluator(new Point((int) (currentX + dx), (int) (currentY + dy))),
                    new Point(currentX, currentY),
                    new Point(targetX, targetY)
            );
            animator.setDuration(duration);
            animator.setInterpolator(new DecelerateInterpolator());
            animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    Point point = (Point) animation.getAnimatedValue();
                    float percent = 1f - animation.getAnimatedFraction();

                    for (Callback callback : mCallbackList) {
                        callback.onClose(1f - percent);
                    }
                    mBackgroundAlpha = percent * startAlpha;
                    setBackgroundColor(ColorUtils.alphaColor(mShadowColor, mBackgroundAlpha));

                    int width = (int) (targetWidth + percent * (tempWidth - targetWidth));
                    int height = Math.max((int) (targetHeight + percent * (tempHeight - mTitleHeight)), width) + mTitleHeight;
                    int left = point.x - width / 2;
                    int right = left + left + width;
                    int top = point.y - (height + mTitleHeight) / 2;
                    int bottom = top + height;
                    mCurrentTouchView.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
                    mCurrentTouchView.layout(left, top, right, bottom);
                }
            });
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    mState = STATE_HIDE;
                    for (Callback callback : mCallbackList) {
                        callback.onHide(position);
                    }
                }
            });
            animator.start();
        } else {
            goToIdle(false, new Rect(mCurrentTouchView.getLeft(), mCurrentTouchView.getTop(),
                    mCurrentTouchView.getRight(), mCurrentTouchView.getBottom()));
        }
    }

    private View makeAndStep(int pos, int left, int top, int right, int bottom, int index) {
        Log.d(TAG, "makeAndStep pos=" + pos);
        View view = obtainView(pos, index);
        view.layout(left, top, right, bottom);
        return view;
    }

    private View obtainView(int position, int index) {
        Log.d(TAG, "obtainView position=" + position + " index=" + index);
        View view = mViewQueue.poll();
        if (view == null) {
            view = adapter.onCreateViewHolder(this, position);
            if (view == null) {
                throw new RuntimeException("onCreateViewHolder  必须填充布局");
            }
        }
//        view.setTag(R.id.tag_switcher_tab_position, position);
        adapter.onBindViewHolder(view, position);
        view.setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent motionEvent) {
                if (mState != STATE_IDLE) {
                    return false;
                }
                if (motionEvent.getAction() == MotionEvent.ACTION_DOWN) {
                    mCurrentTouchView = view;
                }
                return false;
            }
        });
        view.setOnClickListener(view1 -> {
            if (mState == STATE_IDLE) {
                mState = STATE_ANIMATION;
                mPosition = mFirstPosition + indexOfChild(view1);
                mCurrentTouchView = view1;
                goToSelect(view1, mPosition);
            }
        });
        view.setOnLongClickListener(v -> {
            int pos = mFirstPosition + indexOfChild(v);
            mCurrentTouchView = v;
            ITab tabInfo = TabListManager.getInstance().getCurrentTabList().getTabAt(pos);
            TabActionDialog.newInstance(tabInfo, mDownX, mDownY).show(getContext());
            return true;
        });
        view.measure(MeasureSpec.makeMeasureSpec(mChildWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(mChildHeight, MeasureSpec.EXACTLY));
        addView(view, index);
        return view;
    }

    private boolean flag;

    private void selectChildView() {
        if (mPosition < 0) {
            mPosition = getPosition();
        }
        selectChildView(mPosition);
    }

    private void selectCenterPosition() {
        if (mPosition < 0) {
            mPosition = getPosition();
        }
        int maxPos = mFirstPosition + getChildCount() - 1;
        if (mPosition < mFirstPosition || mPosition > maxPos) {
            mPosition = (maxPos + mFirstPosition) / 2;
        }
        savePosition();
    }

    private int getPosition() {
        return Math.max(0, PrefsHelper.with().getInt("tab_index", 0));
    }

    private void savePosition() {
        PrefsHelper.with().getSharedPreferences().edit().putInt("tab_index", mPosition).apply();
    }

    private void selectCenterChildView() {
        selectCenterPosition();
        selectChildView(mPosition);
    }

    private void selectChildView(int position) {
        int childCount = getChildCount();
        Log.d(TAG, "selectChildView position=" + position + " mFirstPosition=" + mFirstPosition + " childCount=" + childCount);
        if (position == 0) {
            mCurrentTouchView = getChildAt(0);
        } else if (position == getCount() - 1) {
            mCurrentTouchView = getChildAt(childCount - 1);
        } else {
            int center = childCount / 2;
            if (position + getChildCount() - center >= getCount()) {
                center = getChildCount() - (getCount() - 1 - position) - 1;
            }
            if (center > position) {
                center = position;
            }
            mCurrentTouchView = getChildAt(center);

        }
        mFirstPosition = position - indexOfChild(mCurrentTouchView);
        Log.e(TAG, "selectChildView position=" + position + " childCount=" + childCount + " currentView=" + mCurrentTouchView + " firstPos=" + mFirstPosition);
//        if (flag) {
//            return;
//        }
//        flag = true;
        for (int i = 0; i < childCount; i++) {
            View child = getChildAt(i);
            Log.e(TAG, "selectChildView mFirstPosition=" + mFirstPosition + " i=" + i + " count=" + getCount());
            adapter.onBindViewHolder(child, mFirstPosition + i);
        }
    }

    private void startTransitionAnim(Rect startRect, Rect endRect, final int nextState, boolean updateChild) {
        new TransitionAnim(startRect, endRect, nextState)
                .setDuration(300)
                .setTransitionCallback(new TransitionCallback() {
                    @Override
                    public void onTransition(Rect targetRect, View targetView, float percent) {
                        int childTop = (mHeight - mChildHeight) / 2;
                        int childBottom = childTop + mChildHeight;
                        int targetLeft = targetRect.left;
                        int targetTop = targetRect.top;
                        int targetRight = targetRect.right;
                        int targetBottom = targetRect.bottom;
                        int index = indexOfChild(mCurrentTouchView);
                        if (updateChild) {
                            if (nextState == STATE_EXPAND) {
                                for (Callback callback : mCallbackList) {
                                    callback.onAnimExpand(percent);
                                }
                            }
                            for (int i = 0; i < getChildCount(); i++) {
                                View child = getChildAt(i);
                                child.setAlpha(1f);
                                int delta = i - index;
                                int left, top, right, bottom;
                                if (delta == 0) {
                                    left = targetLeft;
                                    top = targetTop;
                                    right = targetRight;
                                    bottom = targetBottom;
                                } else if (delta > 0) {
                                    top = childTop;
                                    bottom = childBottom;
                                    right = targetRight + delta * (mGapSize + mChildWidth);
                                    left = right - mChildWidth;
                                } else {
                                    top = childTop;
                                    bottom = childBottom;
                                    left = targetLeft + delta * (mGapSize + mChildWidth);
                                    right = left + mChildWidth;
                                }
                                child.measure(MeasureSpec.makeMeasureSpec(right - left, MeasureSpec.EXACTLY),
                                        MeasureSpec.makeMeasureSpec(bottom - top, MeasureSpec.EXACTLY));
                                child.layout(left, top, right, bottom);
                            }
                        } else {
                            mCurrentTouchView.measure(MeasureSpec.makeMeasureSpec(targetRight - targetLeft, MeasureSpec.EXACTLY),
                                    MeasureSpec.makeMeasureSpec(targetBottom - targetTop, MeasureSpec.EXACTLY));
                            mCurrentTouchView.layout(targetLeft, targetTop, targetRight, targetBottom);
                            if (nextState == STATE_IDLE) {
                                targetLeft = (int) (endRect.left * percent);
                                targetRight = (int) (mWidth - ((mWidth - endRect.right) * percent));
                                for (int i = 0; i < getChildCount(); i++) {
                                    View child = getChildAt(i);
                                    child.setAlpha(1f);
                                    int delta = i - index;
                                    int left, top, right, bottom;
                                    if (delta == 0) {
                                        continue;
                                    } else if (delta > 0) {
                                        top = childTop;
                                        bottom = childBottom;
                                        right = targetRight + delta * (mGapSize + mChildWidth);
                                        left = right - mChildWidth;
                                    } else {
                                        top = childTop;
                                        bottom = childBottom;
                                        left = targetLeft + delta * (mGapSize + mChildWidth);
                                        right = left + mChildWidth;
                                    }
                                    child.measure(MeasureSpec.makeMeasureSpec(right - left, MeasureSpec.EXACTLY),
                                            MeasureSpec.makeMeasureSpec(bottom - top, MeasureSpec.EXACTLY));
                                    child.layout(left, top, right, bottom);
                                }
                            }
                        }
                    }
                })
                .start();
    }

    private void swipeBy(float dy) {

        if (mCurrentTouchView == null) {
            return;
        }

        int currentLeft = mCurrentTouchView.getLeft();
        int currentRight = mCurrentTouchView.getRight();
        int currentTop = (int) (mCurrentTouchView.getTop() + dy);
        int currentBottom = currentTop + mChildHeight;
        mCurrentTouchView.layout(currentLeft, currentTop, currentRight, currentBottom);
        float b = (mHeight + mChildHeight) / 2f;
        float percent = Math.abs(currentBottom) / b;
        mCurrentTouchView.setAlpha(percent);

//        float percent = (mCurrentTouchView.getBottom() + dy) * 2f / (height + mChildHeight);
//        int dx = (int) (percent * (mChildWidth + mGapSize));
//        int dx = (int) (Math.abs(percent - lastP) * (mChildWidth + mGapSize));
        int dx = (int) ((1 - percent) * (mChildWidth + mGapSize));
        Log.d(TAG, "swipeBy dy=" + dy + " percent=" + percent + " dx=" + dx);

        int index = indexOfChild(mCurrentTouchView);
        int position = mFirstPosition + index;
        int top = (mHeight - mChildHeight) / 2;
        int bottom = top + top + mChildHeight;

        int scrollIndex;
        if (mCurrentTouchView.getRight() >= mWidth) {
            scrollIndex = index + 1;
            if (mFirstPosition + scrollIndex >= getCount()) {
                scrollIndex = index - 1;
            }
        } else {
            scrollIndex = index - 1;
            if (mFirstPosition + scrollIndex < 0) {
                scrollIndex = index + 1;
            }
        }


        if (scrollIndex < index) {
            for (int i = 0; i < index; i++) {
                View child = getChildAt(i);
                int left = mCurrentTouchView.getLeft() + (i - index) * (mChildWidth + mGapSize) + dx;
                int right = left + mChildWidth;
                child.layout(left, top, right, bottom);
            }
            View firstView = getChildAt(0);
            int left = firstView.getLeft();
            Log.d(TAG, "remove left mFirstPosition=" + mFirstPosition + " left=" + firstView.getLeft());
            if ((firstView == mCurrentTouchView || left > 0) && mFirstPosition > 0) {
                int right = left - mGapSize;
                viewList.add(0, makeAndStep(--mFirstPosition, right - mChildWidth, top, right, bottom, 0));
//                mFirstPosition--;
            }
        } else if (scrollIndex > index) {
            for (int i = scrollIndex; i < getChildCount(); i++) {
                View child = getChildAt(i);
                int right = mCurrentTouchView.getRight() + (i - index) * (mChildWidth + mGapSize) - dx;
                int left = right - mChildWidth;
                child.layout(left, top, right, bottom);
            }
            View lastView = getChildAt(getChildCount() - 1);

            int right = lastView.getRight();
            if (lastView == mCurrentTouchView) {
                right -= dx;
            }
            Log.d(TAG, "add right mFirstPosition=" + mFirstPosition + " right=" + right + " width=" + mWidth);
            Log.d(TAG, "add right lastPosition=" + (mFirstPosition + getChildCount() - 1) + " count=" + getCount());
            if (right < mWidth && mFirstPosition + getChildCount() - 1 < getCount() - 1) {
                int left = right + mGapSize;
                viewList.add(viewList.size(), makeAndStep(mFirstPosition + getChildCount(), left, top, left + mChildWidth, bottom, viewList.size()));
                Log.d(TAG, "remove right");
            }
        }
    }


    public static class TransitionCallback {
        public void onTransitionStart() {

        }

        public void onTransition(Rect targetRect, View targetView, float percent) {

        }

        public void onTransitionEnd() {

        }
    }

    private class TransitionAnim {

        private final Rect startRect;
        private final Rect endRect;
        final int nextState;
        private TransitionCallback mTransitionCallback;
        private int duration = 360;
        private TimeInterpolator interpolator;


        private TransitionAnim(Rect startRect, Rect endRect, final int nextState) {
            this.startRect = startRect;
            this.endRect = endRect;
            this.nextState = nextState;
        }

        public TransitionAnim setTransitionCallback(TransitionCallback mTransitionCallback) {
            this.mTransitionCallback = mTransitionCallback;
            return this;
        }

        public TransitionAnim setDuration(int duration) {
            this.duration = duration;
            return this;
        }

        public TransitionAnim setInterpolator(TimeInterpolator interpolator) {
            this.interpolator = interpolator;
            return this;
        }

        public void start() {
            int startLeft = startRect.left;
            int startTop = startRect.top;
            int startRight = startRect.right;
            int startBottom = startRect.bottom;

            final int index = indexOfChild(mCurrentTouchView);
            final int position = mFirstPosition + index;
            float startAlpha = mBackgroundAlpha;
            switch (nextState) {
                case STATE_HIDE:
                    for (Callback callback : mCallbackList) {
                        callback.onBeforeHide(position);
                    }
                    break;
                case STATE_IDLE:
                    for (Callback callback : mCallbackList) {
                        callback.onBeforeIdle(position);
                    }
                    break;
                case STATE_EXPAND:
                    for (Callback callback : mCallbackList) {
                        callback.onBeforeExpand(position);
                    }
                    break;
            }


            final Rect transitionRect = new Rect();
            ValueAnimator animator = ValueAnimator.ofFloat(0, 1f);
            animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    float percent = (float) animation.getAnimatedValue();

                    switch (nextState) {
                        case STATE_IDLE:
                            mBackgroundAlpha = startAlpha + percent * (0.4f - startAlpha);
                            for (Callback callback : mCallbackList) {
                                callback.onAnimIdle(percent);
                            }
                            break;
                        case STATE_HIDE:
                            for (Callback callback : mCallbackList) {
                                callback.onClose(percent);
                            }
                            mBackgroundAlpha = (1f - percent) * startAlpha;
                            break;
                        case STATE_EXPAND:
                            mBackgroundAlpha = (1f - percent) * startAlpha;
                            break;
                    }
                    setBackgroundColor(ColorUtils.alphaColor(mShadowColor, mBackgroundAlpha));

                    int targetLeft = (int) (startLeft - (startLeft - endRect.left) * percent);
                    int targetTop = (int) (startTop - (startTop - endRect.top) * percent);
                    int targetRight = (int) (startRight - (startRight - endRect.right) * percent);
                    int targetBottom = (int) (startBottom - (startBottom - endRect.bottom) * percent);
                    transitionRect.set(targetLeft, targetTop, targetRight, targetBottom);
                    if (mCurrentTouchView != null) {
                        mCurrentTouchView.setAlpha(1f);
                        mCurrentTouchView.measure(MeasureSpec.makeMeasureSpec(targetRight - targetLeft, MeasureSpec.EXACTLY),
                                MeasureSpec.makeMeasureSpec(targetBottom - targetTop, MeasureSpec.EXACTLY));
                        mCurrentTouchView.layout(targetLeft, targetTop, targetRight, targetBottom);
                    }

                    if (mTransitionCallback != null) {
                        mTransitionCallback.onTransition(transitionRect, mCurrentTouchView, percent);
                    }
                }
            });
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationStart(Animator animation) {
                    if (mTransitionCallback != null) {
                        mTransitionCallback.onTransitionStart();
                    }
                }

                @Override
                public void onAnimationEnd(Animator animation) {
                    flag = false;
                    mState = nextState;
                    switch (mState) {
                        case STATE_HIDE:
                            for (Callback callback : mCallbackList) {
                                callback.onHide(position);
                            }
                            break;
                        case STATE_IDLE:
                            for (Callback callback : mCallbackList) {
                                callback.onIdle(position);
                            }
                            break;
                        case STATE_EXPAND:
                            if (index == 0 && position > 0) {
                                View child = obtainView(position - 1, 0);
                                viewList.add(0, child);
                                mFirstPosition--;
                            } else if (index == getChildCount() - 1 && position < getCount() - 1) {
                                int size = viewList.size();
                                View child = obtainView(position + 1, size);
                                viewList.add(size, child);
                            }
                            for (Callback callback : mCallbackList) {
                                callback.onExpand(position);
                            }
                            break;
                    }
                    if (mTransitionCallback != null) {
                        mTransitionCallback.onTransitionEnd();
                    }
                }
            });
            if (interpolator == null) {
                if (nextState == STATE_IDLE) {
                    interpolator = new OvershootInterpolator(1f);
                }
            }
            if (interpolator != null) {
                animator.setInterpolator(interpolator);
            }
            animator.setDuration(duration);
            animator.start();
        }
    }

    public interface FlingerListener {
        void onFling(int y);

        void onStop();
    }

    public interface OnFlingListener {
        boolean onFling(int y);

        void onStop();
    }

    private class ReturnFlinger implements Runnable {

        private final OverScroller mScroller;
        private final FlingerListener mListener;


        public ReturnFlinger(Context context, FlingerListener listener) {
            this.mScroller = new OverScroller(context);
            this.mListener = listener;
        }

        @Override
        public void run() {
            if (mScroller.computeScrollOffset()) {
                final int x = mScroller.getCurrX();
                final int y = mScroller.getCurrY();
                if (mListener != null) {
                    mListener.onFling(y);
                }
                postOnAnimation();
            } else {
                if (mListener != null) {
                    mListener.onStop();
                }
            }
        }

        public void fling(int deltaY, float velocityY) {
//            mScroller.startScroll(start.x, start.y, end.x - start.x, end.y - start.y);
            mScroller.fling(0, 0, 0, (int) velocityY, 0, 0, 0, deltaY);
            postOnAnimation();
        }

        public void stop() {
            removeCallbacks(this);
            mScroller.forceFinished(true);
        }

        private void postOnAnimation() {
            removeCallbacks(this);
            ViewCompat.postOnAnimation(SwitcherRecyclerLayout.this, this);
        }

    }

    private class FlingAnim implements Runnable {

        private final OverScroller mScroller;
        private final OnFlingListener mListener;


        public FlingAnim(Context context, OnFlingListener listener) {
            this.mScroller = new OverScroller(context, t -> {
                t -= 1.0f;
                return t * t * t * t * t + 1.0f;
            });
            this.mListener = listener;
        }

        @Override
        public void run() {
            if (mScroller.computeScrollOffset()) {
                final int x = mScroller.getCurrX();
                final int y = mScroller.getCurrY();
                if (mListener != null) {
                    if (!mListener.onFling(x)) {
                        stop();
                        mListener.onStop();
                        return;
                    }
                }
                postOnAnimation();
            } else {
                if (mListener != null) {
                    mListener.onStop();
                }
            }
        }

        public void fling(float velocityX) {
//            mScroller.startScroll(start.x, start.y, end.x - start.x, end.y - start.y);
            mScroller.fling(0, 0, (int) velocityX, 0, Integer.MIN_VALUE, Integer.MAX_VALUE, 0, 0);
            Log.d(TAG, "FlingAnim velocityX=" + velocityX + " finalX=" + mScroller.getFinalX());
            postOnAnimation();
        }

        public void fling(Rect startRect, Rect endRect, float velocityX) {
//            mScroller.startScroll(start.x, start.y, end.x - start.x, end.y - start.y);
            mScroller.fling(0, 0, (int) velocityX, 0, Integer.MIN_VALUE, Integer.MAX_VALUE, 0, 0);
            Log.d(TAG, "FlingAnim velocityX=" + velocityX + " finalX=" + mScroller.getFinalX());
//            if (mScroller.getFinalX() < endRect.centerX() - startRect.centerX()) {
//                mScroller.forceFinished(true);
//                mScroller.startScroll(startRect.centerX(), startRect.centerY(),
//                        endRect.centerX() - startRect.centerX(),
//                        endRect.centerY() - startRect.centerY(), 200);
//            }
            postOnAnimation();
        }

        public void stop() {
            removeCallbacks(this);
            mScroller.forceFinished(true);
        }

        private void postOnAnimation() {
            removeCallbacks(this);
            ViewCompat.postOnAnimation(SwitcherRecyclerLayout.this, this);
        }

    }

    private class ViewFlinger implements Runnable {

        private final OverScroller mScroller;
        private int mLastFlingX = 0;

        private ValueAnimator animator;

        public ViewFlinger(Context context) {
            mScroller = new OverScroller(context, t -> {
                t -= 1.0f;
                return t * t * t * t * t + 1.0f;
            });
        }

        @Override
        public void run() {
            if (mScroller.computeScrollOffset()) {
                final int x = mScroller.getCurrX();
                int dx = x - mLastFlingX;
                mLastFlingX = x;

                View view = getChildAt(0);
                if (view == null) {
                    stop();
                    return;
                }
                int gap = (mWidth - mChildWidth) / 2;
                // 第一个view越界回弹
                if (mFirstPosition == 0 && view.getLeft() > gap) {
                    startSpringAnimation(gap - view.getLeft());
                    mPosition = 0;
                    savePosition();
                    return;
                } else {
                    int nextItemIndex = viewList.size() + mFirstPosition;
                    if (nextItemIndex >= getCount()) {
                        View lastChild = getChildAt(getChildCount() - 1);
                        // 最后一个view越界回弹
                        if (lastChild.getLeft() < gap) {
                            startSpringAnimation(gap - lastChild.getLeft());
                            mPosition = getCount() - 1;
                            savePosition();
                            return;
                        }
                    }
                }
                scrollBy(dx, 0);

                postOnAnimation();
            }
            selectCenterPosition();
        }

        private void startSpringAnimation(int dx) {
            stop();
            animator = ValueAnimator.ofFloat(0, 1f);
//            animator.setInterpolator(new FastOutSlowInInterpolator());
            animator.setInterpolator(new OvershootInterpolator(1f));
            animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                float last = 0;

                @Override
                public void onAnimationUpdate(ValueAnimator valueAnimator) {
                    float percent = (float) (valueAnimator.getAnimatedValue());
                    float temp = percent * dx;
                    float delta = temp - last;
                    last = temp;
                    scrollBy((int) delta, 0);
                }
            });
            animator.setDuration(300);
            animator.start();
        }

        public void stop() {
            if (animator != null) {
                animator.cancel();
                animator = null;
            }
//            TransitionManager.endTransitions(SwitcherRecyclerView.this);
            removeCallbacks(this);
            mScroller.forceFinished(true);
            selectCenterPosition();
        }

        public void fling(float velocityX) {
            mScroller.fling(mScroller.getCurrX(), 0, (int) velocityX, 0, Integer.MIN_VALUE, Integer.MAX_VALUE, 0, 0);
            postOnAnimation();
        }

        private void postOnAnimation() {
            removeCallbacks(this);
            ViewCompat.postOnAnimation(SwitcherRecyclerLayout.this, this);
        }

    }

    private int getCount() {
        return adapter.getCount();
    }


    private final List<Callback> mCallbackList = new ArrayList<>();

    public void addCallback(Callback mCallback) {
        this.mCallbackList.add(mCallback);
    }

    private static class BezierEvaluator implements TypeEvaluator<Point> {

        private final Point controlPoint;
        private final Point tempPoint = new Point();

        public BezierEvaluator(Point controlPoint) {
            this.controlPoint = controlPoint;
        }

        @Override
        public Point evaluate(float t, Point startValue, Point endValue) {
            tempPoint.x = (int) ((1 - t) * (1 - t) * startValue.x + 2 * t * (1 - t) * controlPoint.x + t * t * endValue.x);
            tempPoint.y = (int) ((1 - t) * (1 - t) * startValue.y + 2 * t * (1 - t) * controlPoint.y + t * t * endValue.y);
            return tempPoint;
        }
    }

    public interface Callback {

        boolean onSwipe(int position);

        void onBeforeExpand(int position);

        void onExpand(int position);

        void onBeforeIdle(int position);

        void onIdle(int position);

        void onBeforeHide(int position);

        void onHide(int position);

        void onOpen(float percent);

        void onAnimExpand(float percent);

        void onAnimIdle(float percent);

        void onClose(float percent);

    }


}
