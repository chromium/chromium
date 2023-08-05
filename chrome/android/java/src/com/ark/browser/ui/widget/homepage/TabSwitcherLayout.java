package com.ark.browser.ui.widget.homepage;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.VelocityTracker;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.ui.fragment.search.SearchFragment;
import com.ark.browser.ui.widget.DrawableTintTextView;
import com.ark.browser.ui.widget.indicator.SwitcherIndicatorView;
import com.zpj.toast.ZToast;
import com.zpj.utils.ScreenUtils;

import net.lucode.hackware.magicindicator.MagicIndicator;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.CommonNavigator;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.CommonNavigatorAdapter;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.IPagerIndicator;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.abs.IPagerTitleView;
import net.lucode.hackware.magicindicator.buildins.commonnavigator.indicators.WrapPagerIndicator;

import org.chromium.chrome.R;

public class TabSwitcherLayout extends FrameLayout {

//    private final int mShadowColor = Color.parseColor("#60000000");

    private static final String TAG = "TabSwitcherLayout";

    private static final String[] TAB_TITLES = {"窗口", "无痕"};

    private final SwitcherRecyclerLayout mSwitcher;
    private final View mSwitcherTopBar;
    private final View mSwitcherBottomBar;
    private final View mEmptyLayout;
    private final View mLoadingLayout;
    private final ImageButton mBtnCloseAll;
    private final DrawableTintTextView mBtnBack;

    private final MagicIndicator mIndicator;

    private VelocityTracker mTracker;


    public TabSwitcherLayout(@NonNull Context context) {
        this(context, null);
    }

    public TabSwitcherLayout(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TabSwitcherLayout(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        LayoutInflater.from(context).inflate(R.layout.layout_tab_switcher, this, true);

        mSwitcher = findViewById(R.id.my_recycler);
        mSwitcher.addCallback(new SwitcherRecyclerLayout.Callback() {

            private float mAlpha;

            @Override
            public void onTabGroupChanged(ITabGroup tabGroup) {
                updateBackButtonState();
                if (mSwitcher.getCount() == 0) {
                    mEmptyLayout.setAlpha(1f);
                    mEmptyLayout.setVisibility(VISIBLE);
                } else {
                    mEmptyLayout.setAlpha(0f);
                    mEmptyLayout.setVisibility(GONE);
                }
            }

            @Override
            public boolean onSwipe(int position) {
                if (mSwitcher.getCount() == 0) {
                    mEmptyLayout.setAlpha(0f);
                    mEmptyLayout.setVisibility(VISIBLE);
                    mEmptyLayout.animate()
                            .alpha(1f)
                            .setDuration(200)
                            .start();
                }
                return false;
            }

            @Override
            public void onBeforeHide(int position) {
                mAlpha = mSwitcherTopBar.getAlpha();
            }

            @Override
            public void onHide(int position) {
                mAlpha = 0f;
//                GetLauncherEvent.post(new Callback<LauncherFragment>() {
//                    @Override
//                    public void onResult(LauncherFragment fragment) {
//                        fragment.getLauncherManager().initStatusBarColor();
//                    }
//                });
            }

            @Override
            public void onOpen(float percent) {
                setBarAlpha(percent);
                if (mSwitcher.getCount() == 0) {
                    mEmptyLayout.setAlpha(percent);
                    mEmptyLayout.setVisibility(VISIBLE);
                } else {
                    mEmptyLayout.setAlpha(0f);
                    mEmptyLayout.setVisibility(GONE);
                }
            }

            @Override
            public void onAnimExpand(float percent) {
                mEmptyLayout.setVisibility(GONE);
                setBarAlpha(1f - percent);
            }

            @Override
            public void onAnimIdle(float percent) {
                setBarAlpha(percent);
                if (mSwitcher.getCount() == 0) {
                    mEmptyLayout.setAlpha(percent);
                    mEmptyLayout.setVisibility(VISIBLE);
                } else {
                    mEmptyLayout.setAlpha(0f);
                    mEmptyLayout.setVisibility(GONE);
                }
            }

            @Override
            public void onClose(float percent) {
                if (mAlpha > 0) {
                    setBarAlpha(1f - percent);

                    if (mSwitcher.getCount() == 0) {
                        mEmptyLayout.setAlpha(1 - percent);
                        mEmptyLayout.setVisibility(VISIBLE);
                    } else {
                        mEmptyLayout.setAlpha(0f);
                        mEmptyLayout.setVisibility(GONE);
                    }

                }

            }
        });
        mSwitcherTopBar = findViewById(R.id.top_bar);
        MarginLayoutParams params = (MarginLayoutParams) mSwitcherTopBar.getLayoutParams();
        params.topMargin = ScreenUtils.getStatusBarHeight(context) + ScreenUtils.dp2pxInt(context, 8);
        mSwitcherTopBar.setLayoutParams(params);

        mSwitcherBottomBar = findViewById(R.id.bottom_bar);

        mEmptyLayout = findViewById(R.id.layout_empty);
        mEmptyLayout.setAlpha(0f);

        mLoadingLayout = findViewById(R.id.layout_loading);

        mSwitcherTopBar.setAlpha(0f);
        mSwitcherBottomBar.setAlpha(0f);
        mIndicator = findViewById(R.id.magic_indicator);

        mBtnBack = findViewById(R.id.btn_back);
        mBtnBack.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                ITabGroup tabGroup = mSwitcher.getTabGroup();
                if (tabGroup == null) {
                    return;
                }
                if (tabGroup.getParentGroup() != null) {
                    TabGroupManager.global().selectGroup(tabGroup.getParentGroup());
                }

                updateBackButtonState();
            }
        });

        mBtnCloseAll = findViewById(R.id.btn_close_all);
        mBtnCloseAll.setAlpha(0f);
        mBtnCloseAll.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                ZToast.warning("TODO closeAll");
            }
        });

        findViewById(R.id.btn_search).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                new SearchFragment().show(context);
            }
        });

        CommonNavigator navigator = new CommonNavigator(getContext());
        navigator.setAdjustMode(false);
        navigator.setAdapter(new CommonNavigatorAdapter() {
            @Override
            public int getCount() {
                return TAB_TITLES.length;
            }

            @Override
            public IPagerTitleView getTitleView(Context context, int index) {
                SwitcherIndicatorView titleView = new SwitcherIndicatorView(context);
                titleView.setOnClickListener(new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        Toast.makeText(context, "index=" + index, Toast.LENGTH_SHORT).show();
                        mIndicator.onPageSelected(index);
                    }
                });
                return titleView;
            }

            @Override
            public IPagerIndicator getIndicator(Context context) {
                WrapPagerIndicator indicator = new WrapPagerIndicator(context);
                indicator.setFillColor(Color.parseColor("#aa999999"));
                return indicator;
            }
        });
        mIndicator.setNavigator(navigator);


//        ImageView ivNavPageNormal = findViewById(R.id.iv_nav_page_normal);
//        TabSwitcherDrawable drawable = TabSwitcherDrawable.createTabSwitcherNormalDrawable(getResources(), true);
//        drawable.updateForTabCount(10, false);
//        ivNavPageNormal.setImageDrawable(drawable);


//        onTabManagerInitialized();
    }

    public void onRestore() {
        mLoadingLayout.setAlpha(0f);
        mSwitcher.notifyDataSetChanged();
    }

    private void updateBackButtonState() {
        ITabGroup tabGroup = mSwitcher.getTabGroup();
        if (tabGroup == null) {
            mBtnBack.setEnabled(false);
        } else {
            mBtnBack.setEnabled(tabGroup.getParentGroup() != null);
        }

        if (mBtnBack.isEnabled()) {
            mBtnBack.setTint(Color.WHITE);
        } else {
            mBtnBack.setTint(Color.GRAY);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mTracker != null) {
            mTracker.clear();
            mTracker.recycle();
            mTracker = null;
        }
    }

    private void setBarAlpha(float percent) {
        mSwitcherBottomBar.setAlpha(percent);
        mSwitcherBottomBar.setPivotX(mSwitcherBottomBar.getWidth() / 2f);
        mSwitcherBottomBar.setPivotY(mSwitcherBottomBar.getHeight());
        mSwitcherBottomBar.setScaleX(percent);
        mSwitcherBottomBar.setScaleY(percent);
        mSwitcherBottomBar.setTranslationY(mSwitcherBottomBar.getMeasuredHeight() * (1f - percent));

        mSwitcherTopBar.setAlpha(percent);
        mSwitcherTopBar.setPivotX(mSwitcherTopBar.getWidth() / 2f);
        mSwitcherTopBar.setPivotY(0);
        mSwitcherTopBar.setScaleX(percent);
        mSwitcherTopBar.setScaleY(percent);
        mSwitcherTopBar.setTranslationY(-mSwitcherTopBar.getMeasuredHeight() * (1f - percent));

        mBtnCloseAll.setAlpha(percent);
        mBtnCloseAll.setTranslationY(-mBtnCloseAll.getMeasuredHeight() * (1f - percent));
    }

    public void addCallback(SwitcherRecyclerLayout.Callback callback) {
        mSwitcher.addCallback(callback);
    }

    public SwitcherRecyclerLayout getSwitcher() {
        return mSwitcher;
    }

    public void showSwitcher() {
        setVisibility(View.VISIBLE);
        mSwitcher.setVisibility(View.VISIBLE);

        mSwitcherBottomBar.setVisibility(View.VISIBLE);
        mSwitcherBottomBar.setAlpha(0f);
        mSwitcherTopBar.setVisibility(VISIBLE);
        mSwitcherTopBar.setAlpha(0f);
        mBtnCloseAll.setVisibility(VISIBLE);
        mBtnCloseAll.setAlpha(0f);
        mEmptyLayout.setVisibility(VISIBLE);
        mEmptyLayout.setAlpha(0f);

        updateBackButtonState();
    }

    public void showBrowser() {
        setVisibility(View.VISIBLE);

        mSwitcher.setVisibility(View.INVISIBLE);
        mSwitcherBottomBar.setVisibility(View.INVISIBLE);
        mSwitcherBottomBar.setAlpha(0f);
        mSwitcherTopBar.setVisibility(INVISIBLE);
        mSwitcherTopBar.setAlpha(0f);
        mBtnCloseAll.setVisibility(INVISIBLE);
        mBtnCloseAll.setAlpha(0f);
        mEmptyLayout.setVisibility(GONE);
        mEmptyLayout.setAlpha(0f);
    }

    public void open() {
        post(mSwitcher::goToIdle);
    }

    public void setStatusBarColor(int color) {
//        statusBarView.setBackgroundColor(color);
    }

    public void showToolbar() {
//        mBottomControlContainer.showOmnibox();
//        mBottomControlContainer.getFindToolbarManager().showToolbar();
    }

    public void hideToolbar() {
//        mBottomControlContainer.getFindToolbarManager().hideToolbar();
    }

//    public FindToolbarManager getFindToolbarManager() {
//        return mBottomControlContainer.getFindToolbarManager();
//    }
//
//    public BottomControlContainer getBottomContainer() {
//        return mBottomControlContainer;
//    }

}

