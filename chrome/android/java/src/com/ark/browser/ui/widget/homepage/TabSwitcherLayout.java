//package com.ark.browser.ui.widget.homepage;
//
//import android.animation.Animator;
//import android.animation.AnimatorListenerAdapter;
//import android.animation.ValueAnimator;
//import android.content.Context;
//import android.graphics.Color;
//import android.graphics.Rect;
//import android.support.annotation.NonNull;
//import android.support.annotation.Nullable;
//import android.support.v7.widget.CardView;
//import android.util.AttributeSet;
//import android.view.LayoutInflater;
//import android.view.VelocityTracker;
//import android.view.View;
//import android.view.ViewGroup;
//import android.view.animation.DecelerateInterpolator;
//import android.widget.FrameLayout;
//import android.widget.ImageButton;
//import android.widget.ImageView;
//import android.widget.TextView;
//import android.widget.Toast;
//
//import com.ark.browser.tab.TabListManager;
//import com.zpj.utils.ScreenUtils;
//
//import net.lucode.hackware.magicindicator.MagicIndicator;
//import net.lucode.hackware.magicindicator.buildins.commonnavigator.CommonNavigator;
//import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.CommonNavigatorAdapter;
//import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.IPagerIndicator;
//import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.IPagerTitleView;
//import net.lucode.hackware.magicindicator.buildins.commonnavigator.indicators.WrapPagerIndicator;
//
//import org.chromium.base.Callback;
//import org.chromium.chrome.browser.compositor.CompositorViewHolder;
//import org.chromium.chrome.browser.tab.Tab;
//
//import java.util.List;
//
//public class TabSwitcherLayout extends FrameLayout {
//
////    private final int mShadowColor = Color.parseColor("#60000000");
//
//    private static final String TAG = "TabSwitcherLayout";
//
//    private static final String[] TAB_TITLES = {"窗口", "无痕"};
//
//    private final SwitcherRecyclerLayout mSwitcher;
//    private final View mBrowserLayout;
//    private final View statusBarView;
//    private final BottomControlContainer mBottomControlContainer;
//    private final BottomContainer mBottomContainer;
//    private final View mSwitcherTopBar;
//    private final View mSwitcherBottomBar;
//    private final View mEmptyLayout;
//    private final View mLoadingLayout;
//
//    private final MagicIndicator mIndicator;
//
//    private VelocityTracker mTracker;
//
//
//    public TabSwitcherLayout(@NonNull Context context) {
//        this(context, null);
//    }
//
//    public TabSwitcherLayout(@NonNull Context context, @Nullable AttributeSet attrs) {
//        this(context, attrs, 0);
//    }
//
//    public TabSwitcherLayout(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
//        super(context, attrs, defStyleAttr);
//
//        LayoutInflater.from(context).inflate(R.layout.layout_tab_switcher, this, true);
//
//        mSwitcher = findViewById(R.id.my_recycler);
//        mSwitcher.addCallback(new SwitcherRecyclerLayout.Callback() {
//
//            private float mAlpha;
//
//            @Override
//            public boolean onSwipe(int position) {
//                if (TabListManager.getInstance().getCurrentTabList().getCount() == 0) {
//                    mEmptyLayout.setAlpha(0f);
//                    mEmptyLayout.setVisibility(VISIBLE);
//                    mEmptyLayout.animate()
//                            .alpha(1f)
//                            .setDuration(200)
//                            .start();
//                }
//                return false;
//            }
//
//            @Override
//            public void onBeforeExpand(int position) {
//
//            }
//
//            @Override
//            public void onExpand(int position) {
//
//            }
//
//            @Override
//            public void onBeforeIdle(int position) {
//
//            }
//
//            @Override
//            public void onIdle(int position) {
//
//            }
//
//            @Override
//            public void onBeforeHide(int position) {
//                mAlpha = mSwitcherTopBar.getAlpha();
//            }
//
//            @Override
//            public void onHide(int position) {
//                mAlpha = 0f;
//                GetLauncherEvent.post(new Callback<LauncherFragment>() {
//                    @Override
//                    public void onResult(LauncherFragment fragment) {
//                        fragment.getLauncherManager().initStatusBarColor();
//                    }
//                });
//            }
//
//            @Override
//            public void onOpen(float percent) {
//                setBarAlpha(percent);
//                if (TabListManager.getInstance().getCurrentTabList().getCount() == 0) {
//                    mEmptyLayout.setAlpha(percent);
//                    mEmptyLayout.setVisibility(VISIBLE);
//                } else {
//                    mEmptyLayout.setAlpha(0f);
//                    mEmptyLayout.setVisibility(GONE);
//                }
//            }
//
//            @Override
//            public void onAnimExpand(float percent) {
//                mEmptyLayout.setVisibility(GONE);
//                setBarAlpha(1f - percent);
//            }
//
//            @Override
//            public void onAnimIdle(float percent) {
//                setBarAlpha(percent);
//                if (TabListManager.getInstance().getCurrentTabList().getCount() == 0) {
//                    mEmptyLayout.setAlpha(percent);
//                    mEmptyLayout.setVisibility(VISIBLE);
//                } else {
//                    mEmptyLayout.setAlpha(0f);
//                    mEmptyLayout.setVisibility(GONE);
//                }
//            }
//
//            @Override
//            public void onClose(float percent) {
//                if (mAlpha > 0) {
//                    setBarAlpha(1f - percent);
//
//                    if (TabListManager.getInstance().getCurrentTabList().getCount() == 0) {
//                        mEmptyLayout.setAlpha(1 - percent);
//                        mEmptyLayout.setVisibility(VISIBLE);
//                    } else {
//                        mEmptyLayout.setAlpha(0f);
//                        mEmptyLayout.setVisibility(GONE);
//                    }
//
//                }
//
//            }
//        });
//        mBrowserLayout = findViewById(R.id.browser_layout);
//        statusBarView = findViewById(R.id.status_bar_view);
//        mBottomControlContainer = findViewById(R.id.bottom_control_container);
//        mBottomContainer = findViewById(R.id.bottom_container);
//        mSwitcherTopBar = findViewById(R.id.top_bar);
//        MarginLayoutParams params = (MarginLayoutParams) mSwitcherTopBar.getLayoutParams();
//        params.topMargin = ScreenUtils.getStatusBarHeight(context) + ScreenUtils.dp2pxInt(context, 8);
//        mSwitcherTopBar.setLayoutParams(params);
//
//        mSwitcherBottomBar = findViewById(R.id.bottom_bar);
//
//        mEmptyLayout = findViewById(R.id.layout_empty);
//        mEmptyLayout.setAlpha(0f);
//
//        mLoadingLayout = findViewById(R.id.layout_loading);
//
//        mSwitcherTopBar.setAlpha(0f);
//        mSwitcherBottomBar.setAlpha(0f);
//        mIndicator = findViewById(R.id.magic_indicator);
//
//        findViewById(R.id.btn_search).setOnClickListener(new OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                new SearchFragment2().show(context);
//            }
//        });
//
//        CommonNavigator navigator = new CommonNavigator(getContext());
//        navigator.setAdjustMode(false);
//        navigator.setAdapter(new CommonNavigatorAdapter() {
//            @Override
//            public int getCount() {
//                return TAB_TITLES.length;
//            }
//
//            @Override
//            public IPagerTitleView getTitleView(Context context, int index) {
//                SwitcherIndicatorView titleView = new SwitcherIndicatorView(context);
//                titleView.setOnClickListener(new OnClickListener() {
//                    @Override
//                    public void onClick(View v) {
//                        Toast.makeText(context, "index=" + index, Toast.LENGTH_SHORT).show();
//                        mIndicator.onPageSelected(index);
//                    }
//                });
//                return titleView;
//            }
//
//            @Override
//            public IPagerIndicator getIndicator(Context context) {
//                WrapPagerIndicator indicator = new WrapPagerIndicator(context);
//                indicator.setFillColor(Color.parseColor("#aa999999"));
//                return indicator;
//            }
//        });
//        mIndicator.setNavigator(navigator);
//
//
////        ImageView ivNavPageNormal = findViewById(R.id.iv_nav_page_normal);
////        TabSwitcherDrawable drawable = TabSwitcherDrawable.createTabSwitcherNormalDrawable(getResources(), true);
////        drawable.updateForTabCount(10, false);
////        ivNavPageNormal.setImageDrawable(drawable);
//
//
////        onTabManagerInitialized();
//    }
//
//    private void updateItem(View view, ITab iTab, Runnable runnable) {
//        TextView tvTitle = view.findViewById(R.id.tv_title);
//        ImageView ivIcon = view.findViewById(R.id.iv_icon);
//        ImageButton btnLock = view.findViewById(R.id.btn_lock);
//        ImageButton btnMore = view.findViewById(R.id.btn_more);
//        btnLock.setImageResource(iTab.getTabInfo().isLocked() ? R.drawable.ic_lock_24 : R.drawable.ic_lock_open_24);
//        btnLock.setOnClickListener(new OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                iTab.getTabInfo().setLocked(!iTab.getTabInfo().isLocked());
//                btnLock.setImageResource(iTab.getTabInfo().isLocked() ? R.drawable.ic_lock_24 : R.drawable.ic_lock_open_24);
//            }
//        });
//        btnMore.setOnClickListener(new OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                TabActionDialog.newInstance(iTab.getId()).show(v);
//            }
//        });
//        CardView cardView = view.findViewById(R.id.card_view);
//
////        ImageView ivThumbnail = view.findViewById(R.id.iv_thumbnail);
//        FitWidthImageView ivThumbnail = view.findViewById(R.id.iv_thumbnail);
//        PageInfo pageInfo = iTab.getCurrentPageInfo();
//        if (pageInfo != null) {
//            cardView.setCardBackgroundColor(pageInfo.getThemeColor());
//            tvTitle.setText(pageInfo.getTitle());
//            Tab tab = PageCacheManager.getInstance().findPage(pageInfo);
//            if (tab == null || tab.getFavicon() == null) {
//                ivIcon.setImageResource(R.drawable.default_favicon_white);
//            } else {
//                ivIcon.setImageBitmap(tab.getFavicon());
//            }
//            TabSnapshotManager.getInstance().loadTabSnapshot(ivThumbnail, pageInfo);
//        } else {
//            cardView.setCardBackgroundColor(Color.WHITE);
//            ivThumbnail.setImageBitmap(null);
//        }
//
//    }
//
//    public void onRestore() {
//        mLoadingLayout.setAlpha(0f);
//
//        mSwitcher.notifyDataSetChanged();
//    }
//
//    public void onTabManagerInitialized() {
//        final List<ITab> tabList = TabListManager.getInstance().getCurrentTabList().getTabInfoList();
//        mSwitcher.setAdapter(new Adapter() {
//            @Override
//            public View onCreateViewHolder(ViewGroup parent, int position) {
//                View itemView = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_tab, parent, false);
//                return itemView;
//            }
//
//            @Override
//            public void onBindViewHolder(View itemView, int position) {
//                if (TabListManager.getInstance().isLoaded()) {
//                    ITab tab = tabList.get(position);
//                    updateItem(itemView, tab, null);
//                }
//            }
//
//            @Override
//            public int getCount() {
//                if (TabListManager.getInstance().isLoaded()) {
//                    return tabList.size();
//                }
//                return 0;
//            }
//
//            @Override
//            public int getPosition() {
//                if (TabListManager.getInstance().isLoaded()) {
//                    return TabListManager.getInstance().getCurrentTabList().getIndex();
//                }
//                return 0;
//            }
//
//            @Override
//            public boolean onSwipe(int position) {
//                if (TabListManager.getInstance().isLoaded()) {
//                    TabListManager.getInstance().getCurrentTabList().closeTab(tabList.get(position));
//                }
//                return false;
//            }
//
//            @Override
//            public boolean isLocked(int position) {
////                        return true;
//                if (position < 0 || position > getCount() - 1) {
//                    return false;
//                }
//                return tabList.get(position).getTabInfo().isLocked();
//            }
//        });
//
//    }
//
//    @Override
//    protected void onDetachedFromWindow() {
//        super.onDetachedFromWindow();
//        if (mTracker != null) {
//            mTracker.clear();
//            mTracker.recycle();
//            mTracker = null;
//        }
//    }
//
//    private void setBarAlpha(float percent) {
//        mSwitcherBottomBar.setAlpha(percent);
//        mSwitcherBottomBar.setPivotX(mSwitcherBottomBar.getWidth() / 2f);
//        mSwitcherBottomBar.setPivotY(mSwitcherBottomBar.getHeight());
//        mSwitcherBottomBar.setScaleX(percent);
//        mSwitcherBottomBar.setScaleY(percent);
//        mSwitcherBottomBar.setTranslationY(mSwitcherBottomBar.getMeasuredHeight() * (1f - percent));
//
//        mSwitcherTopBar.setAlpha(percent);
//        mSwitcherTopBar.setPivotX(mSwitcherTopBar.getWidth() / 2f);
//        mSwitcherTopBar.setPivotY(0);
//        mSwitcherTopBar.setScaleX(percent);
//        mSwitcherTopBar.setScaleY(percent);
//        mSwitcherTopBar.setTranslationY(-mSwitcherTopBar.getMeasuredHeight() * (1f - percent));
//    }
//
//    public void addCallback(SwitcherRecyclerLayout.Callback callback) {
//        mSwitcher.addCallback(callback);
//    }
//
//    public SwitcherRecyclerLayout getSwitcher() {
//        return mSwitcher;
//    }
//
//    public void showSwitcher() {
//        setVisibility(View.VISIBLE);
//        mSwitcher.setVisibility(View.VISIBLE);
//        mBrowserLayout.setVisibility(INVISIBLE);
//        CompositorViewHolder viewHolder = mBrowserLayout.findViewById(R.id.compositor_view_holder);
//        viewHolder.setPage(null);
//
//        mSwitcherBottomBar.setVisibility(View.VISIBLE);
//        mSwitcherBottomBar.setAlpha(0f);
//        mSwitcherTopBar.setVisibility(VISIBLE);
//        mSwitcherTopBar.setAlpha(0f);
//        mEmptyLayout.setVisibility(VISIBLE);
//        mEmptyLayout.setAlpha(0f);
//        mBottomContainer.setVisibility(INVISIBLE);
//        mBottomControlContainer.setVisibility(View.INVISIBLE);
//    }
//
//    public void showBrowser() {
//        setVisibility(View.VISIBLE);
//        mBrowserLayout.setVisibility(VISIBLE);
//
//        mSwitcher.setVisibility(View.INVISIBLE);
//        mSwitcherBottomBar.setVisibility(View.INVISIBLE);
//        mSwitcherBottomBar.setAlpha(0f);
//        mSwitcherTopBar.setVisibility(INVISIBLE);
//        mSwitcherTopBar.setAlpha(0f);
//        mEmptyLayout.setVisibility(GONE);
//        mEmptyLayout.setAlpha(0f);
//        mBottomContainer.setVisibility(VISIBLE);
////        mBottomControlContainer.refreshSelectedTab();
//        mBottomControlContainer.setVisibility(View.VISIBLE);
//    }
//
//    public void transitionToBrowser(Rect rect, Runnable endRunnable) {
//
//        setVisibility(View.VISIBLE);
//        mSwitcher.setVisibility(View.INVISIBLE);
////        mSwitcherBottomBar.setVisibility(INVISIBLE);
////        mSwitcherTopBar.setVisibility(INVISIBLE);
//
//        mBottomContainer.setAlpha(0f);
//        mBottomControlContainer.setAlpha(0f);
//        mBottomContainer.setVisibility(VISIBLE);
//        mBottomControlContainer.setVisibility(View.VISIBLE);
//        mBrowserLayout.setVisibility(INVISIBLE);
//
//        Rect startRect = rect;
//        Rect endRect = new Rect(0, 0, getWidth(), getHeight());
//        ValueAnimator animator = ValueAnimator.ofFloat(0, 1f);
//
////        layout(startRect.left, startRect.top, startRect.right, startRect.bottom);
//
//        animator.setInterpolator(new DecelerateInterpolator(2));
//        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
//            @Override
//            public void onAnimationUpdate(ValueAnimator animation) {
//                float percent = (float) animation.getAnimatedValue();
//
//                int left = (int) (startRect.left + (endRect.left - startRect.left) * percent);
//                int top = (int) (startRect.top + (endRect.top - startRect.top) * percent);
//                int right = (int) (startRect.right + (endRect.right - startRect.right) * percent);
//                int bottom = (int) (startRect.bottom + (endRect.bottom - startRect.bottom) * percent);
//
//                float scale = (startRect.width() + (endRect.width() - startRect.width()) * percent) / endRect.width();
//
//                mBrowserLayout.setPivotX(0);
//                mBrowserLayout.setPivotY(0);
//                mBrowserLayout.setScaleX(scale);
//                mBrowserLayout.setScaleY(scale);
//
//                mBrowserLayout.setTranslationX(left);
//                mBrowserLayout.setTranslationY(top);
////                mBrowserLayout.layout(left, top, right, bottom);
//
//
//                mBrowserLayout.setVisibility(VISIBLE);
//
//                mBottomContainer.setAlpha(percent);
//                mBottomControlContainer.setAlpha(percent);
//            }
//        });
//        animator.addListener(new AnimatorListenerAdapter() {
//            @Override
//            public void onAnimationEnd(Animator animation) {
//                if (endRunnable != null) {
//                    endRunnable.run();
//                }
//            }
//        });
//        animator.setDuration(360);
//        animator.start();
//    }
//
//    public void open() {
//        TabSnapshotManager.getInstance().cacheCurrentTab();
//        post(mSwitcher::goToIdle);
//    }
//
//    public void setStatusBarColor(int color) {
//        statusBarView.setBackgroundColor(color);
//    }
//
//    public void showToolbar() {
//        mBottomControlContainer.showOmnibox();
//        mBottomControlContainer.getFindToolbarManager().showToolbar();
//    }
//
//    public void hideToolbar() {
//        mBottomControlContainer.getFindToolbarManager().hideToolbar();
//    }
//
//    public FindToolbarManager getFindToolbarManager() {
//        return mBottomControlContainer.getFindToolbarManager();
//    }
//
//    public BottomControlContainer getBottomContainer() {
//        return mBottomControlContainer;
//    }
//
//}
//
