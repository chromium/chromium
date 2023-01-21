package com.ark.browser.ui.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.NestedScrollingParent2;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.viewpager.widget.ViewPager;

import com.ark.browser.ui.fragment.dialog.RecyclerPartShadowDialogFragment;
import com.ark.browser.ui.recycler.BaseHeaderMultiData;
import com.ark.browser.ui.recycler.BookmarkMultiData;
import com.ark.browser.ui.recycler.HistoryMultiData;
import com.ark.browser.ui.recycler.OfflinePageMultiData;
import com.ark.browser.ui.recycler.SavedPasswordMultiData;
import com.zpj.recyclerview.MultiData;
import com.zpj.recyclerview.MultiRecycler;
import com.zpj.recyclerview.manager.MultiLayoutManager;
import com.zpj.skin.SkinEngine;
import com.zpj.utils.KeyboardUtils;
import com.zpj.utils.ScreenUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.List;

public class CustomHeaderLayout extends ViewGroup implements NestedScrollingParent2 {

    private static final String TAG = "CustomHeaderLayout";

    private final String[] titles = {"书签", "历史", "网页", "密码"};
    private final List<MultiData<?>> multiDataList = new ArrayList<>();

    private final float mTouchSlop;

    private final int dp56;
    private final int dp36;
    private final int dp16;
    private final int dp14;
    private final int mSearchBarHeight;
    private final int mStatusBarHeight;
    private int mContainerHeight;
    private final int mMinHeaderHeight;
    private final int mMaxHeaderHeight;
    private final int mNormalColor;
    private final int mSelectedColor;

    private int index = 0;
    private boolean isExpand = true;
    private boolean isSearchMode;

    private float mProgress = 1f;

    private ImageView mBtnBack;
    private ImageView mBtnMenu;
    private TextView mTvTitle;
    private SolidArrowView mArrowView;
    private SearchBar mSearchBar;
    private EditText mEditText;

    private ViewPager mViewPager;

    private RecyclerView mRecyclerView;
    private MultiRecycler mRecycler;
    private FrameLayout mContainer;

    private ValueAnimator mAnimator;

    private Callback mCallback;

    public interface Callback {
        void onBackButtonClicked(View view);

        void onMenuButtonClicked(View view);

        void onTitleClicked(View view, String[] titles, int position, Runnable dismissRunnable);
    }

    public CustomHeaderLayout(@NonNull Context context) {
        this(context, null);
    }

    public CustomHeaderLayout(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public CustomHeaderLayout(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        mNormalColor = SkinEngine.getColor(context, R.attr.textColorMinor);
        mSelectedColor = context.getResources().getColor(R.color.colorPrimary);

        mStatusBarHeight = ScreenUtils.getStatusBarHeight(context);
        dp56 = (int) (56 * context.getResources().getDisplayMetrics().density);
        dp36 = (int) (34 * context.getResources().getDisplayMetrics().density);
        dp16 = (int) (16 * context.getResources().getDisplayMetrics().density);
        dp14 = (int) (12 * context.getResources().getDisplayMetrics().density);
        mSearchBarHeight = (int) (36 * context.getResources().getDisplayMetrics().density);

        mMinHeaderHeight = dp56 + mStatusBarHeight;
        mMaxHeaderHeight = 2 * dp56 + mStatusBarHeight;

        mCurrentTop = mMaxHeaderHeight;

        OnClickListener onClickListener = v -> {
            if (mViewPager != null) {
                mViewPager.setCurrentItem((int) v.getTag(), true);
            }
        };

        for (int i = 0; i < titles.length; i++) {
            TextView textView = new TextView(getContext());
            textView.setText(titles[i]);
            textView.setGravity(Gravity.CENTER);
            textView.setTextColor(i == index ? mSelectedColor : mNormalColor);
            textView.setTextSize(16);
            textView.getPaint().setFakeBoldText(true);
            textView.setTag(i);
            textView.setOnClickListener(onClickListener);
            addView(textView);
        }

        int padding = (int) (16 * context.getResources().getDisplayMetrics().density);

        mBtnBack = new ImageView(context);
        mBtnBack.setBackground(null);
        mBtnBack.setImageResource(R.drawable.ic_arrow_back_white_24dp);
        SkinEngine.setTint(mBtnBack, R.attr.textColorMajor);
        mBtnBack.setPadding(padding, padding, padding, padding);
        mBtnBack.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mCallback != null) {
                    mCallback.onBackButtonClicked(v);
                }
            }
        });
        addView(mBtnBack);

        mBtnMenu = new ImageView(context);
        mBtnMenu.setBackground(null);
        mBtnMenu.setImageResource(R.drawable.ic_more);
        SkinEngine.setTint(mBtnMenu, R.attr.textColorMajor);
        mBtnMenu.setPadding(padding, padding, padding, padding);
        mBtnMenu.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mCallback != null) {
                    mCallback.onMenuButtonClicked(v);
                }
            }
        });
        addView(mBtnMenu);

        mTvTitle = new TextView(context);
        mTvTitle.setGravity(Gravity.CENTER_VERTICAL);
        mTvTitle.setText(titles[index]);
        mTvTitle.setTextSize(16);
        mTvTitle.setTextColor(mSelectedColor);
        mTvTitle.getPaint().setFakeBoldText(true);
        mTvTitle.setVisibility(INVISIBLE);
        mTvTitle.setOnClickListener(view -> {
            mArrowView.switchState();
            new RecyclerPartShadowDialogFragment()
                    .addItemIf("全部", isSearchMode)
                    .addItems(titles)
                    .setSelectedItem(index)
                    .setOnItemClickListener((v, title, position) -> {
                        if (isSearchMode) {
                            if (position > 0) {
                                mTvTitle.setText(titles[position - 1]);
                            } else {
                                mTvTitle.setText("全部");
                            }
                        } else if (mViewPager != null) {
                            mViewPager.setCurrentItem(position, true);
                        }
                    })
                    .setOnDismissListener(dialog -> mArrowView.switchState())
                    .show(view);
//            if (mCallback != null) {
//                mCallback.onTitleClicked(view, titles, index, () -> mArrowView.switchState());
//            }
        });
        addView(mTvTitle);

        mArrowView = new SolidArrowView(context);
        mArrowView.setAlpha(0f);
        mArrowView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mTvTitle.performClick();
            }
        });
        mArrowView.setVisibility(INVISIBLE);
        mArrowView.setColor(mSelectedColor);
        addView(mArrowView);

        mSearchBar = new SearchBar(context);
//        mSearchBar.setOnSearchListener(new SearchBar.OnSearchListener() {
//            @Override
//            public void onSearch(String keyword) {
//
//            }
//        });
        mSearchBar.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (!isSearchMode()) {
                    enterSearch();
                }
            }
        });
        mSearchBar.addTextWatcher(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (isSearchMode) {
                    String keyword = s.toString();
                    if (TextUtils.isEmpty(keyword)) {
                        if (mProgress == 0f) {
                            post(() -> {
                                multiDataList.clear();
                                mRecycler.notifyDataSetChanged();
                            });
                        }
                        return;
                    }
                    for (MultiData<?> multiData : multiDataList) {
                        if (multiData instanceof BaseHeaderMultiData) {
                            ((BaseHeaderMultiData<?>) multiData).onDestroy();
                        }
                    }
                    multiDataList.clear();

                    multiDataList.add(new BookmarkMultiData(keyword));
                    multiDataList.add(new HistoryMultiData(keyword));
                    multiDataList.add(new OfflinePageMultiData(keyword));
                    multiDataList.add(new SavedPasswordMultiData(keyword));
                    mRecycler.notifyDataSetChanged();
                }
            }

            @Override
            public void afterTextChanged(Editable s) {

            }
        });
        mEditText = mSearchBar.getEditor();
//        mEditText.setOnClickListener(new OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                mSearchBar.performClick();
//            }
//        });
//        mEditText.setBackgroundResource(R.drawable.bg_search);
        mEditText.setHint("搜索" + titles[index]);
        mEditText.setCursorVisible(false);
        mEditText.setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (MotionEvent.ACTION_DOWN == event.getAction()) {
                    mEditText.setCursorVisible(true);
                    mEditText.setFocusable(true);
                    mEditText.setFocusableInTouchMode(true);
                    mEditText.requestFocus();
                }
                return false;
            }
        });
        addView(mSearchBar);

        mContainer = new FrameLayout(context);

        mViewPager = new ViewPager(context);
        mViewPager.setId(View.generateViewId());
        mViewPager.addOnPageChangeListener(new ViewPager.OnPageChangeListener() {
            @Override
            public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {

            }

            @Override
            public void onPageSelected(int position) {
                TextView tempTv = (TextView) getChildAt(index);
                tempTv.setTextColor(mNormalColor);
                index = position;
                TextView tv = (TextView) getChildAt(position);
                tv.setTextColor(mSelectedColor);
                mEditText.setHint("搜索" + titles[index]);
                mTvTitle.setText(titles[position]);
            }

            @Override
            public void onPageScrollStateChanged(int state) {

            }
        });
        mContainer.addView(mViewPager, new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));


        mRecyclerView = new RecyclerView(context);
        mRecycler = MultiRecycler.with(mRecyclerView, multiDataList)
//                .addItemDecoration(new StickyHeaderItemDecoration())
                .setLayoutManager(new MultiLayoutManager())
                .build();
        mContainer.addView(mRecyclerView, new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        addView(mContainer);
    }

    public View getSearchBar() {
        return mSearchBar;
    }

    public ViewPager getViewPager() {
        return mViewPager;
    }

    public void setCallback(Callback mCallback) {
        this.mCallback = mCallback;
    }

    public void setProgress(float mProgress) {
        layoutChildren(mProgress);
    }

    public float getProgress() {
        return mProgress;
    }

    public float getProgressHeight() {
        return mProgress * dp56;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        Log.d(TAG, "onMeasure height=" + MeasureSpec.getSize(heightMeasureSpec) + " mode=" + MeasureSpec.getMode(heightMeasureSpec));
//        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(mHeight, MeasureSpec.EXACTLY)); // 2 * dp56
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);
//        mContainerHeight = height - mStatusBarHeight - dp56;
//        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)); // 2 * dp56

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        mContainerHeight = getMeasuredHeight() - mStatusBarHeight - dp56;
        mContainer.measure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(mContainerHeight, MeasureSpec.EXACTLY));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        Log.d(TAG, "onLayout");
        layoutChildren(mProgress, isSearchMode);
    }

    private void layoutChildren(float percent) {
        layoutChildren(percent, false);
    }

    private void layoutChildren(float percent, boolean isSearch) {
        Log.d(TAG, "layoutChildren isSearch=" + isSearch + " per=" + percent);
        mProgress = percent;
        int width = (getWidth() - 2 * dp56) / 4;
        int heightSpec = MeasureSpec.makeMeasureSpec(dp56, MeasureSpec.EXACTLY);
        for (int i = 0; i < titles.length; i++) {
            View child = getChildAt(i);
            int left = (int) (dp56 + i * width * percent);
            int right = (int) (left + dp36 + (width - dp36) * percent);
            child.measure(MeasureSpec.makeMeasureSpec(right - left, MeasureSpec.EXACTLY), heightSpec);

            child.layout(left, mStatusBarHeight, right, dp56 + mStatusBarHeight);
            if (i != index) {
                child.setAlpha(percent);
            }
        }

        mBtnBack.measure(heightSpec, heightSpec);
        mBtnBack.layout(0, mStatusBarHeight, dp56, dp56 + mStatusBarHeight);
        mBtnMenu.measure(heightSpec, heightSpec);
        mBtnMenu.layout(getWidth() - dp56, mStatusBarHeight, getWidth(), dp56 + mStatusBarHeight);
        if (isSearch) {
            if (percent == 0) {
                mBtnMenu.setVisibility(INVISIBLE);
            } else {
                mBtnMenu.setVisibility(VISIBLE);
            }
            mBtnMenu.setAlpha(percent);
        } else {
            mBtnMenu.setVisibility(VISIBLE);
            mBtnMenu.setAlpha(1f);
        }

        View current = getChildAt(index);
        if (percent == 0) {
            current.setAlpha(0f);
            mTvTitle.setVisibility(VISIBLE);
            mTvTitle.measure(MeasureSpec.makeMeasureSpec(dp36, MeasureSpec.EXACTLY), heightSpec);
            mTvTitle.layout(dp56, mStatusBarHeight, dp36 + dp56, dp56 + mStatusBarHeight);
        } else {
            current.setAlpha(1f);
            mTvTitle.setVisibility(INVISIBLE);
        }

        int arrowSizeSpec = MeasureSpec.makeMeasureSpec(dp14, MeasureSpec.EXACTLY);
        mArrowView.measure(arrowSizeSpec, arrowSizeSpec);
        int arrowTop = (dp56 - dp14) / 2 + mStatusBarHeight;

        int arrowLeft = (current.getRight() + current.getLeft() + dp36) / 2;

        mArrowView.layout(arrowLeft, arrowTop, arrowLeft + dp14, arrowTop + dp14);
        mArrowView.setVisibility(percent == 1f ? INVISIBLE : VISIBLE);
        mArrowView.setAlpha(1 - percent);
        mArrowView.setScaleX(1 - percent);
        mArrowView.setScaleY(1 - percent);

        int childWidth;
        int childRight;
        int childTop = (int) (dp56 * percent + (dp56 - mSearchBarHeight) / 2f + mStatusBarHeight);
        if (isSearch) {
            childWidth = (int) (getWidth() - dp56 * percent - (dp56 + dp36 + dp14 + 2 * dp16) * (1 - percent));
            childRight = (int) (getWidth() - dp16 * (1 - percent) - dp56 / 2 * percent);
        } else {
//            childWidth = (int) (getWidth() - dp56 * percent - 3 * dp56 * (1 - percent));
            childWidth = (int) (getWidth() - dp56 * percent - (2 * dp56 + dp36 + dp14 + dp16) * (1 - percent));
            childRight = (int) (getWidth() - dp56 * (1 - percent) - dp56 / 2 * percent);
        }
        mSearchBar.measure(MeasureSpec.makeMeasureSpec(childWidth, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(mSearchBarHeight, MeasureSpec.EXACTLY));
        mSearchBar.layout(childRight - childWidth, childTop, childRight, childTop + mSearchBarHeight);

        if (isSearch) {
            mRecyclerView.setVisibility(VISIBLE);
            mRecyclerView.setAlpha(1f - percent);
            mViewPager.setVisibility(VISIBLE);
            mViewPager.setAlpha(percent);
            mSearchBar.setEditable(percent == 0f);
            if (percent == 0f) {
                mViewPager.setVisibility(INVISIBLE);
            } else if (percent == 1f) {
                mViewPager.setVisibility(VISIBLE);
                mRecyclerView.setVisibility(INVISIBLE);
                post(new Runnable() {
                    @Override
                    public void run() {
                        multiDataList.clear();
                        mRecycler.notifyDataSetChanged();
                    }
                });
            } else if (percent > 0f) {
                mEditText.setText(null);
            }
        } else {
            mEditText.setText(null);
            mSearchBar.setEditable(false);
            mRecyclerView.setAlpha(0f);
            mViewPager.setAlpha(1f);
            mRecyclerView.setVisibility(INVISIBLE);
            mViewPager.setVisibility(VISIBLE);

//            mContainer.setOnClickListener(null);
//            mContainer.setClickable(false);
        }
        int containerTop = (int) (dp56 * percent + mMinHeaderHeight);
        mCurrentTop = containerTop;
        mContainer.layout(0, containerTop, getWidth(), containerTop + mContainerHeight);
    }



//    private float mDownX;
//    private float mDownY;
//
//    private int mTouchMode = 0;
//
//    private static final int MODE_NONE = 0;
//    private static final int MODE_CLICK = 1;
//    private static final int MODE_SWIPE = 2;
//    private static final int MODE_SCROLL = 3;

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
//        switch (ev.getAction()) {
//            case MotionEvent.ACTION_DOWN:
//                mDownX = ev.getRawX();
//                mDownY = ev.getRawY();
//                mTouchMode = MODE_NONE;
//                break;
//            case MotionEvent.ACTION_MOVE:
//                if (mTouchMode == MODE_NONE) {
//                    float deltaX = ev.getRawX() - mDownX;
//                    float deltaY = ev.getRawY() - mDownY;
//                    if (Math.abs(deltaX) < mTouchSlop && Math.abs(deltaY) < mTouchSlop) {
//                        mTouchMode = MODE_CLICK;
//                    } else if (Math.abs(deltaX / deltaY) > 1f) {
//                        mTouchMode = MODE_SWIPE;
//                    } else {
//                        mTouchMode = MODE_SCROLL;
//                    }
//                }
//                break;
//        }
        return super.dispatchTouchEvent(ev);
    }

    public interface OnProgressChangeListener {
        void onProgressChange(float progress, boolean isSearch);
    }

    private OnProgressChangeListener onProgressChangeListener;

    public void setOnProgressChangeListener(OnProgressChangeListener onProgressChangeListener) {
        this.onProgressChangeListener = onProgressChangeListener;
    }

    public void onResume() {
        for (MultiData<?> multiData : multiDataList) {
            if (multiData instanceof BaseHeaderMultiData) {
                ((BaseHeaderMultiData<?>) multiData).onResume();
            }
        }
    }

    public void enterSearch() {
        if (mAnimator != null) {
            mAnimator.cancel();
            mAnimator = null;
        }
        isExpand = false;
        isSearchMode = true;
        float delta = mProgress;
        ValueAnimator animator = ValueAnimator.ofFloat(1, 0);
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator valueAnimator) {
                float percent = (float) valueAnimator.getAnimatedValue();
                layoutChildren(percent * delta, true);
                if (onProgressChangeListener != null) {
                    onProgressChangeListener.onProgressChange(percent, true);
                }
            }
        });
        animator.setDuration(360);
        animator.start();
    }

    public void exitSearch() {
        if (mAnimator != null) {
            mAnimator.cancel();
            mAnimator = null;
        }
        isExpand = true;
        mTvTitle.setText(titles[index]);
        float tempProgress = mProgress;
        float delta = 1f - mProgress;
        KeyboardUtils.hideSoftInputKeyboard(mEditText);
        ValueAnimator animator = ValueAnimator.ofFloat(0, 1);
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator valueAnimator) {
                float percent = (float) valueAnimator.getAnimatedValue();
                layoutChildren(tempProgress + percent * delta, true);
                if (percent == 1f) {
                    isSearchMode = false;
                }
                if (onProgressChangeListener != null) {
                    onProgressChangeListener.onProgressChange(percent, true);
                }
            }
        });
        animator.setDuration(360);
        animator.start();
    }

    public boolean isSearchMode() {
        return isSearchMode;
    }

    public void collapse() {
//        isSearchMode = false;
//        isExpand = false;
//        ValueAnimator animator = ValueAnimator.ofFloat(mProgress, 0);
//        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
//            @Override
//            public void onAnimationUpdate(ValueAnimator valueAnimator) {
//                float percent = (float) valueAnimator.getAnimatedValue();
//                layoutChildren(percent);
//                if (onProgressChangeListener != null) {
//                    onProgressChangeListener.onProgressChange(percent, false);
//                }
//            }
//        });
//        animator.setDuration(360);
//        animator.start();

        collapse(0f);

    }

    public void expand() {
//        isSearchMode = false;
//        isExpand = true;
//        ValueAnimator animator = ValueAnimator.ofFloat(mProgress, 1);
//        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
//            @Override
//            public void onAnimationUpdate(ValueAnimator valueAnimator) {
//                float percent = (float) valueAnimator.getAnimatedValue();
//                layoutChildren(percent);
//                if (onProgressChangeListener != null) {
//                    onProgressChangeListener.onProgressChange(percent, false);
//                }
//            }
//        });
//        animator.setDuration(360);
//        animator.start();

        expand(0f);

    }

    public void collapse(float velocityY) {
        switchHeader(velocityY, false);
    }

    public void expand(float velocityY) {
        switchHeader(velocityY, true);
    }

    private void switchHeader(float velocityY, final boolean expand) {
        isSearchMode = false;
        if (mAnimator != null) {
            mAnimator.cancel();
            mAnimator = null;
        }
        if (expand) {
            mAnimator = ValueAnimator.ofFloat(mProgress, 1f);
        } else {
            mAnimator = ValueAnimator.ofFloat(mProgress, 0f);
        }
        mAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                float percent = (float) animation.getAnimatedValue();
                layoutChildren(percent);
                if (onProgressChangeListener != null) {
                    onProgressChangeListener.onProgressChange(percent, false);
                }
            }
        });
        mAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                isExpand = expand;
            }
        });
        mAnimator.setDuration(velocityY == 0f ? 200 : Math.min(200, (int) (1000f * (mCurrentTop - dp56) / Math.abs(velocityY))));
        mAnimator.start();
    }

    public boolean isExpand() {
        return isExpand;
    }









    private int mCurrentTop;
    private float mFlingVelocityY;
    private boolean isFling;
    private boolean isNestedChange;

    @Override
    public int getNestedScrollAxes() {
        return ViewCompat.SCROLL_AXIS_VERTICAL;
    }

    @Override
    public boolean onStartNestedScroll(@NonNull View child, @NonNull View target, int axes, int type) {
        isFling = false;
        boolean result = !isSearchMode && axes == ViewCompat.SCROLL_AXIS_VERTICAL;
        Log.d(TAG, "onStartNestedScroll result=" + result + " type=" + type + " axes=" + axes);
        return result;
    }

    @Override
    public void onNestedScrollAccepted(@NonNull View child, @NonNull View target, int axes, int type) {
        Log.d(TAG, "onNestedScrollAccepted type=" + type + " axes=" + axes);
        if (type == ViewCompat.TYPE_TOUCH) {
            mFlingVelocityY = 0f;
        }
        if (mAnimator != null) {
            mAnimator.cancel();
            mAnimator = null;
        }
    }

    @Override
    public void onStopNestedScroll(@NonNull View view, int type) {
        Log.d(TAG, "onStopNestedScroll isNestedChange=" + isNestedChange + " type=" + type);
        if (isNestedChange) {
            if (type == ViewCompat.TYPE_TOUCH && !isFling) {
                isNestedChange = false;
                if (!isSearchMode) {
                    if (mCurrentTop > dp56 / 2f + mMinHeaderHeight) {
                        expand();
                    } else {
                        collapse();
                    }
                }
            } else if (type == ViewCompat.TYPE_NON_TOUCH && isFling) {
                isFling = false;
                isNestedChange = false;
                if (!isSearchMode) {
                    if (mFlingVelocityY > 0) {
                        collapse(mFlingVelocityY);
                    } else {
                        expand(mFlingVelocityY);
                    }
                }
            }
        }
    }

    @Override
    public void onNestedScroll(@NonNull View view, int dxConsumed, int dyConsumed, int dxUnconsumed, int dyUnconsumed, int type) {
        Log.d(TAG, "onNestedScroll dxConsumed=" + dxConsumed + " dyConsumed=" + dyConsumed + " dxUnconsumed=" + dxUnconsumed + " dyUnconsumed=" + dyUnconsumed + " type=" + type);
        if (!isSearchMode && mCurrentTop < mMaxHeaderHeight && dyConsumed == 0 && dyUnconsumed != 0) {
            isNestedChange = true;
            int y = mCurrentTop - dyUnconsumed;
            float percent;
            if (y < mMaxHeaderHeight) {
                if (y < mMinHeaderHeight) {
                    y = mMinHeaderHeight;
                }
                isExpand = false;
                percent = (float) (y - mMinHeaderHeight) / dp56;
                mCurrentTop = y;
            } else {
                percent = 1f;
                isExpand = true;
                mCurrentTop = mMaxHeaderHeight;
            }
            layoutChildren(percent);
        }
    }

    @Override
    public void onNestedPreScroll(@NonNull View view, int dx, int dy, @NonNull int[] consumed, int type) {
        Log.d(TAG, "onNestedPreScroll dx=" + dx + " dy=" + dy + " type=" + type + " isSearchMode=" + isSearchMode);
        if (!isSearchMode && mCurrentTop > mMinHeaderHeight && dy > 0) {
            isNestedChange = true;
            int y = mCurrentTop - dy;
            float percent;
            if (y > mMinHeaderHeight) {
                isExpand = true;
                consumed[1] = dy;
                percent = (float) (y - mMinHeaderHeight) / dp56;
                mCurrentTop = y;
            } else {
                consumed[1] = mCurrentTop - mMinHeaderHeight;
                isExpand = false;
                mCurrentTop = mMinHeaderHeight;
                percent = 0f;
            }
            layoutChildren(percent);
        }
    }

    @Override
    public boolean onNestedPreFling(View target, float velocityX, float velocityY) {
        Log.d(TAG, "onNestedPreFling velocityY=" + velocityY);
        return super.onNestedPreFling(target, velocityX, velocityY);
    }

    @Override
    public boolean onNestedFling(View target, float velocityX, float velocityY, boolean consumed) {
        Log.d(TAG, "onNestedFling velocityX=" + velocityX + " velocityY=" + velocityY + " consumed=" + consumed);
        mFlingVelocityY = velocityY;
        isFling = true;
        return super.onNestedFling(target, velocityX, velocityY, consumed);
    }
}

