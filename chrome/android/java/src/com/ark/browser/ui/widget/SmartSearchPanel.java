package com.ark.browser.ui.widget;

import android.content.Context;
import android.graphics.Rect;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.customview.widget.ViewDragHelper;

import com.ark.browser.core.ArkCompositorViewHolder;
import com.ark.browser.core.ArkNavigationHandler;
import com.ark.browser.core.ArkWindowAndroid;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.EmptyTabInfoObserver;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabInfoObserver;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.tab.core.TabGroupImpl;
import com.ark.browser.tab.core.TabImpl;
import com.ark.browser.utils.ArkLogger;
import com.google.android.material.tabs.TabLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public class SmartSearchPanel extends FrameLayout {

    private static final String TAG = "SmartSearchPanel";

    private static final int MIN_FLING_VELOCITY = 400; // dips per second

    private final ViewDragHelper mDragHelper;

    private final float mTouchSlop;

    private View mDragView;

    private final int mMarginTop;
    private final int mBottomBarHeight;
    private int mSlideRange;
    private int mOffset;

    private String keyword;

    private final ArkWindowAndroid mNativeWindow;
    private Window mWindow;

    public SmartSearchPanel(@NonNull Context context) {
        this(context, null);
    }

    public SmartSearchPanel(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public SmartSearchPanel(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        mNativeWindow = new ArkWindowAndroid(context) {

            @Override
            public TabDelegateFactory getTabDelegateFactory() {
                return getCompositorViewHolder().getTabDelegateFactory();
            }

            @Override
            public ArkCompositorViewHolder getCompositorViewHolder() {
                return mViewHolder;
            }

            @Override
            public ArkNavigationHandler getNavigationHandler() {
                return new ArkNavigationHandler() {
                    @Override
                    public boolean canGoForward() {
                        return mFloatTabList.canGoForward();
                    }

                    @Override
                    public boolean goForward() {
                        return mFloatTabList.goForward();
                    }

                    @Override
                    public boolean canGoBack() {
                        return mFloatTabList.canGoBack();
                    }

                    @Override
                    public boolean goBack() {
                        return mFloatTabList.goBack();
                    }
                };
            }

            @Override
            protected Window getWindow() {
                if (mWindow == null) {
                    return super.getWindow();
                }
                return mWindow;
            }
        };


        float density = context.getResources().getDisplayMetrics().density;
        mMarginTop = (int) (density * 28);
        mBottomBarHeight = (int) (density * 56);

        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        this.mDragHelper = ViewDragHelper.create(this, 0.5f, new ViewDragHelper.Callback() {
            @Override
            public boolean tryCaptureView(@NonNull View child, int pointerId) {
                return child == mDragView;
            }

            @Override
            public int clampViewPositionVertical(@NonNull View child, int top, int dy) {
                if (top < mMarginTop) {
                    top = mMarginTop;
                } else if (top > mSlideRange + mMarginTop) {
                    top = mSlideRange + mMarginTop;
                }
                return top;
            }

            @Override
            public int clampViewPositionHorizontal(@NonNull View child, int left, int dx) {
                return 0;
            }

            @Override
            public void onViewPositionChanged(@NonNull View changedView, int left, int top, int dx, int dy) {
                mOffset = mSlideRange - (top - mMarginTop);
//                ArkLogger.e(TAG, "onViewPositionChanged top=" + top + " dy=" + dy + " getTop=" + mDragView.getTop());

//                if (mOffset > 0) {
//                    String url = TemplateUrlServiceFactory.get().getUrlForSearchQuery(keyword);
//                    loadUrl(posArray[mCurrentPosition], url);
//                }
            }

            @Override
            public void onViewReleased(@NonNull View releasedChild, float xvel, float yvel) {
//                ArkLogger.e(TAG, "onViewReleased yvel=" + yvel);
                if (yvel > 0 || (yvel == 0 && mOffset < mSlideRange / 2)) {
                    mDragHelper.settleCapturedViewAt(mDragView.getLeft(), mSlideRange + mMarginTop);
                } else {
                    mDragHelper.settleCapturedViewAt(mDragView.getLeft(), mMarginTop);
                }
                invalidate();
            }

            @Override
            public void onViewDragStateChanged(int state) {
                ArkLogger.e(TAG, "onViewDragStateChanged state=" + state + " mOffset=" + mOffset);
                onDragStateChanged(state);
            }
        });
        mDragHelper.setMinVelocity(MIN_FLING_VELOCITY * density);
    }

    private OnPanelStateChangedListener mStateListener;

    public void setOnPanelStateChangedListener(OnPanelStateChangedListener listener) {
        this.mStateListener = listener;
    }

    private void onDragStateChanged(int state) {
        if (state == ViewDragHelper.STATE_DRAGGING) {
            select(mCurrentPosition);
        }
        if (mStateListener != null) {
            mStateListener.onStateChanged(this);
        }
    }

    public interface OnPanelStateChangedListener {
        void onStateChanged(SmartSearchPanel panel);
    }

    public boolean isExpand() {
        return mDragHelper.getViewDragState() == ViewDragHelper.STATE_IDLE && mOffset > 0;
    }

    public boolean isDragging() {
        return mDragHelper.getViewDragState() == ViewDragHelper.STATE_DRAGGING;
    }

    public boolean isSettling() {
        return mDragHelper.getViewDragState() == ViewDragHelper.STATE_SETTLING;
    }

    public boolean isClosed() {
        return mDragHelper.getViewDragState() == ViewDragHelper.STATE_IDLE && mOffset == 0;
    }

    public void attachWindow(Window window) {
        this.mWindow = window;
    }


    private final FormatSmartSearchItem[] mSearchItems = {
            new FormatSmartSearchItem("Bing", "https://cn.bing.com/search?q=%s"),
            new FormatSmartSearchItem("磁力搜索", "http://www.btmovi.in/so/%s.html"),
            new FormatSmartSearchItem("网盘搜索", "https://www.wuyasou.com/search?keyword=%s"),
            new FormatSmartSearchItem("Baidu", "https://www.baidu.com"),
            new FormatSmartSearchItem("自定义搜索", "http://xia.fobenshidao.cc/search.php?mod=forum&searchsubmit=yes&srchtxt=%s"),
    };


    private int[] posArray;
    private ArkCompositorViewHolder mViewHolder;
    private View mTopBar;
    private TabLayout tabLayout;
    private int mCurrentPosition = 0;

    public abstract static class SmartSearchItem {

        @DrawableRes
        private final int icon;
        private final String title;

        public SmartSearchItem(String title) {
            this(title, 0);
        }

        public SmartSearchItem(String title, @DrawableRes int icon) {
            this.title = title;
            this.icon = icon;
        }

        @DrawableRes
        public int getIcon() {
            return icon;
        }

        public String getTitle() {
            return title;
        }

        public abstract String getUrl(String keyword);

    }

    public static class FormatSmartSearchItem extends SmartSearchItem {

        private final String formatUrl;

        private String keyword;

        public FormatSmartSearchItem(String title, String formatUrl) {
            super(title);
            this.formatUrl = formatUrl;
        }

        @Override
        public String getUrl(String keyword) {
            this.keyword = keyword;
            keyword = new String(keyword.getBytes(), StandardCharsets.UTF_8);
            try {
                keyword = URLEncoder.encode(keyword, StandardCharsets.UTF_8.name());
            } catch (UnsupportedEncodingException e) {
                e.printStackTrace();
            }
            return String.format(formatUrl, keyword);
        }

        public String getKeyword() {
            return keyword;
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        posArray = new int[mSearchItems.length];
        Arrays.fill(posArray, -1);
        mDragView = getChildAt(getChildCount() - 1);
        mTopBar = mDragView.findViewById(R.id.top_bar);
        tabLayout = mDragView.findViewById(R.id.smart_search_indicator);

        for (SmartSearchItem item : mSearchItems) {
            TabLayout.Tab tab = tabLayout.newTab().setText(item.getTitle());
            tabLayout.addTab(tab);
        }
        tabLayout.setTabMode(TabLayout.MODE_SCROLLABLE);
        tabLayout.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                select(tab.getPosition());
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {

            }

            @Override
            public void onTabReselected(TabLayout.Tab tab) {

            }
        });
        mViewHolder = mDragView.findViewById(R.id.my_compositor_view_holder);
        mViewHolder.setRootView(this);
    }

    private void select(int index) {
        mCurrentPosition = index;
        int pos = posArray[index];

        FormatSmartSearchItem item = mSearchItems[index];
//        String url = item.getUrl(keyword);

        pos = loadUrl(pos, item);
        posArray[index] = pos;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        ArkLogger.e(TAG, "onMeasure width=" + getWidth() + " height=" + getHeight());
        if (getHeight() >= mMarginTop) {
            mDragView.measure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(getHeight() - mMarginTop, MeasureSpec.EXACTLY));
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        ArkLogger.e(TAG, "onLayout l=%s, t=%s, r=%s, b=%s",
                left, top, right, bottom);
        super.onLayout(changed, left, top, right, bottom);
        mSlideRange = getHeight() - mBottomBarHeight - mMarginTop;
        ViewCompat.offsetTopAndBottom(mDragView, getHeight() - mBottomBarHeight - mOffset);
    }


    private boolean mSlideMode = false;

    private int mMode = MODE_NONE;
    private static final int MODE_NONE = -1;
    private static final int MODE_SLIDE = 0;
    private static final int MODE_OTHER = 0;

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        ArkLogger.e(TAG, "onInterceptTouchEvent action=" + MotionEvent.actionToString(ev.getAction()));
        return false;
    }

    private float mDownX;
    private float mDownY;
    private boolean canSlide;

    private boolean isCanSlide(int x, int y) {
        View view = mDragView;
        if (view == null) {
            return false;
        } else {
            int bottom = view.getTop() + mTopBar.getHeight();
            return x >= view.getLeft() && x < view.getRight() && y >= view.getTop() && y < bottom;
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        ArkLogger.e(TAG, "dispatchTouchEvent event=" + MotionEvent.actionToString(ev.getAction()));

        float x = ev.getX();
        float y = ev.getY();

        int action = ev.getAction();

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                mDownX = x;
                mDownY = y;

                Rect rect = new Rect();
                mTopBar.getGlobalVisibleRect(rect);
                canSlide = (x >= rect.left && x < rect.right
                        && y >= rect.top && y < rect.bottom);
//                if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) {
//                    canSlide = true;
//                }

//                canSlide = mDragHelper.isViewUnder(mDragView, (int) x, (int) y);
                if (!canSlide) {
                    return super.dispatchTouchEvent(ev);
                }
                break;
            case MotionEvent.ACTION_MOVE:
                float dx = Math.abs(x - mDownX);
                float dy = Math.abs(y - mDownY);
                if (dx < mTouchSlop && dy < mTouchSlop) {
                    break;
                }
                if (dy > dx) {
                    if (canSlide) {
                        mDragHelper.processTouchEvent(ev);
                        return true;
                    } else {
                        ViewGroup contentView = mViewHolder.getContentView();
                        if (contentView != null) {
                            boolean isTop = !contentView.canScrollVertically(-1);
                            boolean isBottom = !contentView.canScrollVertically(1);
                            ArkLogger.e(TAG, "dispatchTouchEvent move isTop=" + isTop + " isBottom=" + isBottom);
                        }

                        ArkLogger.e(TAG, "dispatchTouchEvent move isExpand=" + isExpand());

//                        if (isExpand()) {
//
//                            if (y < mDownY) {
//                                mDragHelper.cancel();
//                                return super.dispatchTouchEvent(ev);
//                            }
//                            super.dispatchTouchEvent(ev);
//                        }
//                        mDragHelper.cancel();
                        return super.dispatchTouchEvent(ev);
                    }
                }
                break;
        }

        ArkLogger.e(TAG, "dispatchTouchEvent action=" + MotionEvent.actionToString(action) + " isSlideViewUnder=" + canSlide);
//        if (canSlide) {
//            mDragHelper.processTouchEvent(ev);
//            return true;
//        }
        if (canSlide) {
            mDragHelper.processTouchEvent(ev);
        }
        boolean result = super.dispatchTouchEvent(ev);
        ArkLogger.e(TAG, "dispatchTouchEvent result=" + result);
        return result;
    }


    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!canSlide) {
            return super.onTouchEvent(event);
        }
        ArkLogger.e(TAG, "onTouchEvent action=" + MotionEvent.actionToString(event.getAction()));

//        if (mSlideMode || event.getAction() == MotionEvent.ACTION_DOWN) {
//            mDragHelper.processTouchEvent(event);
//        }


        int action = event.getAction();

        switch (action) {
            case MotionEvent.ACTION_MOVE:
                if (mMode != MODE_NONE) {
//                    interceptTap = mMode == MODE_SLIDE;
                } else if (canSlide) {
                    float x = event.getX();
                    float y = event.getY();
                    float dx = Math.abs(x - mDownX);
                    float dy = Math.abs(y - mDownY);
                    if (dx < mTouchSlop && dy < mTouchSlop) {
                        break;
                    }
                    if (dy > dx) {
                        ArkLogger.e(TAG, "onTouchEvent move");
                        mSlideMode = true;
                        mMode = MODE_SLIDE;
                    } else {
                        mMode = MODE_OTHER;
                    }
                    break;
                }
        }
        ArkLogger.e(TAG, "onTouchEvent mSlideMode=" + mSlideMode);
        if (mSlideMode || event.getAction() == MotionEvent.ACTION_DOWN) {
            mDragHelper.processTouchEvent(event);
            return true;
        } else {
            mDragHelper.cancel();
            return super.onTouchEvent(event);
        }
    }

    @Override
    public void computeScroll() {
        if (mDragHelper.continueSettling(true)) {
            ViewCompat.postInvalidateOnAnimation(this);
        }
    }

    public void updateKeyword(String keyword) {
        ArkLogger.e(TAG, "updateKeyword keyword=" + keyword);
        this.keyword = keyword;
    }

    public boolean onBackPressed() {
        if (mViewHolder == null) {
            return false;
        }
        if (isClosed()) {
            return false;
        }
        return mViewHolder.onBackPressed();
    }

    public int getOffset() {
        return mOffset;
    }

    public void show() {
        ArkLogger.e(TAG, "show height=" + getHeight());
        mOffset = 0;
        ViewCompat.offsetTopAndBottom(mDragView, getHeight() - mBottomBarHeight);
        setVisibility(VISIBLE);
        if (mViewHolder.getWindowAndroid() == null) {
            mViewHolder.setFocusable(false);
            mViewHolder.initCompositor(mNativeWindow, new ArkCompositorViewHolder.Callback() {
//                @Override
//                public boolean openNewPage(@NonNull Tab current, @TabLaunchType int type, String url) {
//                    if (mFloatTabList == null) {
//                        return false;
//                    }
//                    return mFloatTabList.openNewPage(current, type, url);
//                }

                @Override
                public ITabGroup getTabList(@NonNull Tab current) {
                    return getFloatTabList();
                }

                @Override
                public void onPageAttached(@NonNull Tab page) {

                }

                @Override
                public void onPageDetached(@NonNull Tab page) {

                }

                @Override
                public void onShutDown() {
                    if (mFloatTabList != null) {
                        mFloatTabList.destroy();
                    }
                }
            });
        }
        mViewHolder.onStart();


        mViewHolder.setTab(null);

        Tab tab = getActivityTab();
        if (tab != null) {
            if (tab.isHidden()) {
                tab.show(TabSelectionType.FROM_USER);
            } else {
                // The visible Tab's renderer process may have died after the activity was
                // paused. Ensure that it's restored appropriately.
                tab.loadIfNeeded();
            }
        }

    }


    public void hide() {
        ArkLogger.e(TAG, "hide");
        if (mOffset != 0) {
            mOffset = 0;
            ViewCompat.offsetTopAndBottom(mDragView, getHeight() - mBottomBarHeight);
        }
        Arrays.fill(posArray, -1);
        mCurrentPosition = 0;
        if (mFloatTabList != null) {
            if (mFloatingTabInfoObserver != null) {
                mFloatingTabInfoObserver.onDestroy();
                mFloatingTabInfoObserver = null;
            }
            mFloatTabList.destroy();
            mFloatTabList = null;
        }

        if (mViewHolder != null) {
            mViewHolder.onStop();
        }

        setVisibility(INVISIBLE);

        Tab tab = getActivityTab();
        if (tab != null) {
            tab.hide(TabHidingType.ACTIVITY_HIDDEN);
        }
    }

    public Tab getActivityTab() {
        if (mFloatTabList == null) {
            return null;
        }
        ITab tab = mFloatTabList.getCurrentTab();
        if (tab != null) {
            return TabCacheManager.getInstance().findTab(tab.getId());
        }
        return null;
    }

    private ITabGroup mFloatTabList;

    private ITabGroup getFloatTabList() {
        if (mFloatTabList == null) {

            mFloatTabList = new TabGroupImpl("group_smart_search", false) {

                @Override
                public void onIndexChanged(int index) {
                    this.index = index;
                }

            };
            mFloatingTabInfoObserver = new FloatingTabInfoObserver(mFloatTabList);

            ArkLogger.e(TAG, "loadUrl mFloatTabList=" + mFloatTabList);

        }
        return mFloatTabList;
    }

    private int loadUrl(int index, FormatSmartSearchItem item) {
        mFloatTabList = getFloatTabList();
        ArkLogger.e(TAG, "loadUrl mFloatTabList=" + mFloatTabList);
        if (mFloatTabList == null) {
            return index;
        }

        ITab iTab = mFloatTabList.getTabAt(index);
        ArkLogger.e(TAG, "loadUrl tab=" + iTab);
        if (iTab == null) {
            String url = item.getUrl(keyword);
            LoadUrlParams loadUrlParams = new LoadUrlParams(UrlFormatter.fixupUrl(url));
            loadUrlParams.setTransitionType(TabLaunchType.FROM_CHROME_UI);
            index = openNewTab(loadUrlParams);
        } else {
            ArkTabImpl tab = (ArkTabImpl) TabCacheManager.getInstance().findTab(iTab);
            ArkLogger.e(TAG, "loadUrl tab=" + tab);
            if (tab != null) {
                ArkLogger.e(TAG, "loadUrl oldKey=" + item.getKeyword() + " newKey=" + keyword);
                if (!TextUtils.equals(keyword, item.getKeyword())) {
                    String url = item.getUrl(keyword);
                    LoadUrlParams loadUrlParams = new LoadUrlParams(UrlFormatter.fixupUrl(url));
                    loadUrlParams.setTransitionType(TabLaunchType.FROM_CHROME_UI);
                    tab.loadInNewPage(loadUrlParams);
                    return index;
                }
            }
        }

        mFloatTabList.selectTabAt(index);
        return index;
    }

    private int openNewTab(LoadUrlParams loadUrlParams) {
        ArkLogger.e(TAG, "openNewTab url=" + loadUrlParams.getUrl());
        TabInfo newTabInfo = TabInfo.create();
        ITab newTab = new TabImpl(newTabInfo) {

            @Override
            public void saveTabInfo() {

            }

            @Override
            public void deleteTabInfo() {

            }
        };

        mFloatTabList.getTabList().add(newTab);

        newTabInfo.setLaunchType(TabLaunchType.FROM_CHROME_UI);
        ArkTabImpl tab = ArkTabImpl.create(newTab, null);

        tab.loadInNewPage(loadUrlParams);

        for (TabInfoObserver obs : mFloatTabList.getObservers()) {
            obs.didAddTab(newTab, TabSelectionType.FROM_USER);
        }

        return mFloatTabList.getCount() - 1;
    }

    private FloatingTabInfoObserver mFloatingTabInfoObserver;

    private class FloatingTabInfoObserver extends EmptyTabInfoObserver {

        private ITabGroup mTabGroup;

        public FloatingTabInfoObserver(ITabGroup tabGroup) {
            this.mTabGroup = tabGroup;
            if (this.mTabGroup != null) {
                this.mTabGroup.addObserver(this);
            }
        }

        @Override
        public void didAddTab(ITab page, int type) {
            Tab tab = TabCacheManager.getInstance().findTab(page.getId());
            mViewHolder.setTab(tab);
        }

        @Override
        public void didSelectTab(ITab tab, int type, int lastId) {
            mViewHolder.setTab(TabCacheManager.getInstance().findTab(tab.getId()));
        }

        public void onDestroy() {
            if (this.mTabGroup != null) {
                this.mTabGroup.removeObserver(this);
                this.mTabGroup = null;
            }
        }

    }


}

