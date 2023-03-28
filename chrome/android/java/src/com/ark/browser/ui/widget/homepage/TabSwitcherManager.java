package com.ark.browser.ui.widget.homepage;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.animation.DecelerateInterpolator;

import com.android.launcher3.LauncherLayout;
import com.ark.browser.core.ArkCompositorViewHolder;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.ui.fragment.wallpaper.WallpaperSelectFragment;
import com.ark.browser.ui.widget.BottomControlBar;
import com.ark.browser.ui.widget.BottomController;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;
import com.zpj.utils.StatusBarUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.util.ColorUtils;

public class TabSwitcherManager implements SwitcherRecyclerLayout.Callback {


    private final ArkCompositorViewHolder mViewHolder;
    private final View mBrowserLayout;
    private final ArkLauncherLayout mLauncherLayout;
    private final TabSwitcherLayout mTabSwitcherLayout;
    private final SwitcherRecyclerLayout mSwitcher;
    private final BottomController mBottomController;

    public TabSwitcherManager(View view, Bundle savedInstanceState) {
        mViewHolder = view.findViewById(R.id.compositor_view_holder);
        mViewHolder.setRootView(view);
        mLauncherLayout = view.findViewById(R.id.launcher_layout);
        mBrowserLayout = view.findViewById(R.id.browser_layout);
        mTabSwitcherLayout = view.findViewById(R.id.tab_switcher_layout);
        mSwitcher = mTabSwitcherLayout.getSwitcher();
        mSwitcher.addCallback(this);
        mSwitcher.setAdapter(new ArkTabAdapter());

        BottomControlBar bottomControlBar = view.findViewById(R.id.bottom_control_bar);
        bottomControlBar.setSwitcherManager(this);
        mBottomController = new BottomController(view);

//        // TODO
//        ClickHelper.with(mLauncherLayout)
//                .setOnClickListener((view1, x, y) -> {
//                    if (isInLauncher()) {
//                        mSwitcher.open();
//                    }
//                })
//                .setOnLongClickListener(new ClickHelper.OnLongClickListener() {
//                    @Override
//                    public boolean onLongClick(View view, float x, float y) {
//                        ZDialog.attach()
//                                .addItem("壁纸")
//                                .setOnSelectListener((fragment, i, s) -> {
//                                    if (i == 0) {
//                                        fragment.start(new WallpaperSelectFragment());
//                                    }
//                                    fragment.dismiss();
//                                })
//                                .setTouchPoint(x, y)
//                                .show(view);
//                        return true;
//                    }
//                });

        mLauncherLayout.init(savedInstanceState);
        mLauncherLayout.setSlideListener(new LauncherLayout.SlideListener() {
            @Override
            public void onSlideStart(int i) {
//                if (!isInTabSwitcher()) {
//                    goToTabSwitcher();
//                }
                if (isInLauncher()) {
                    mSwitcher.open();
                }
            }

            @Override
            public void onSlideVertical(float v, int i) {

            }

            @Override
            public void onSlideEnd() {

            }

            @Override
            public boolean canHandleLongPress() {
                return isInLauncher();
            }

            @Override
            public boolean canStartDrag() {
                return isInLauncher();
            }
        });

    }

    public BottomController getBottomController() {
        return mBottomController;
    }

    public SwitcherRecyclerLayout getSwitcher() {
        return mSwitcher;
    }

    public ArkCompositorViewHolder getCompositorViewHolder() {
        return mViewHolder;
    }

    public View getBrowserLayout() {
        return mBrowserLayout;
    }

    public TabSwitcherLayout getTabSwitcherLayout() {
        return mTabSwitcherLayout;
    }

    public void showSwitcher() {
        mLauncherLayout.setVisibility(View.VISIBLE);
        hideBrowser();
        mTabSwitcherLayout.showSwitcher();
    }

    public void showBrowser() {
        mBrowserLayout.setVisibility(View.VISIBLE);
        Tab tab = mViewHolder.getTab();
        if (tab != null) {
            int color = tab.getThemeColor();
            setThemeColor(color);
        } else {
            setThemeColor(AppConfig.isNightMode() ? Color.BLACK : Color.WHITE);
        }
    }

    public void hideBrowser() {
        mBrowserLayout.setVisibility(View.INVISIBLE);
        StatusBarUtils.setLightMode(ContextUtils.getActivity(mBrowserLayout.getContext()));
    }

    public void setThemeColor(int color) {
        if (mBrowserLayout.getVisibility() != View.VISIBLE) {
            return;
        }
        mBottomController.updatePrimaryColor(color);
        mBrowserLayout.setBackgroundColor(color);

        boolean useLight = ColorUtils.shouldUseLightForegroundOnBackground(color);
        mTabSwitcherLayout.setStatusBarColor(color);
        Log.d("LauncherManager", "setStatusBarColor color=" + color);
        if (!useLight && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            StatusBarUtils.setDarkMode(ContextUtils.getActivity(mBrowserLayout.getContext()));
        } else {
            StatusBarUtils.setLightMode(ContextUtils.getActivity(mBrowserLayout.getContext()));
        }
    }

    public void onRestore() {
        mTabSwitcherLayout.onRestore();
    }

    @Override
    public boolean onSwipe(int position) {
        return true;
    }

    @Override
    public void onBeforeExpand(int position) {
        ITabGroup tabGroup = TabGroupManager.global().getCurrentTabGroup();
        tabGroup.selectTabAt(position);
    }

    @Override
    public void onExpand(int position) {
        goToBrowser(false);
    }

    @Override
    public void onBeforeIdle(int position) {
        showSwitcher();
        mLauncherLayout.setVisibility(View.INVISIBLE);
    }

    @Override
    public void onIdle(int position) {

    }

    @Override
    public void onBeforeHide(int position) {

    }

    @Override
    public void onHide(int position) {
        goToLauncher();
    }

    @Override
    public void onOpen(float percent) {
        mLauncherLayout.setVisibility(View.VISIBLE);
        mLauncherLayout.setAlpha(1 - percent);
    }

    @Override
    public void onAnimExpand(float percent) {

    }

    @Override
    public void onAnimIdle(float percent) {

    }

    @Override
    public void onClose(float percent) {
        mLauncherLayout.setVisibility(View.VISIBLE);
        mLauncherLayout.setAlpha(percent);
    }

    private boolean isVisible(View view) {
        return view.getVisibility() == View.VISIBLE;
    }

    public boolean isInBrowser() {
        return isVisible(mBrowserLayout);
    }

    public boolean isInLauncher() {
        return isVisible(mLauncherLayout) && !isVisible(mTabSwitcherLayout);
    }

    public boolean isInTabSwitcher() {
        return isVisible(mTabSwitcherLayout) && isVisible(mTabSwitcherLayout.getSwitcher());
    }

    public void goToBrowser(boolean animated) {
        mLauncherLayout.setVisibility(View.INVISIBLE);
        showBrowser();
        mTabSwitcherLayout.showBrowser();
    }

    public void goToBrowser() {
        goToBrowser(true);
    }

    public void goToTabSwitcher() {
        mTabSwitcherLayout.showSwitcher();
        mTabSwitcherLayout.open();
    }

    public void goToLauncher(boolean animated) {
        mLauncherLayout.setVisibility(View.VISIBLE);
        mTabSwitcherLayout.setVisibility(View.INVISIBLE);
    }

    public void goToLauncher() {
        goToLauncher(true);
    }

    public boolean onBackPressed() {
        if (isInTabSwitcher()) {
            mSwitcher.close();
            return true;
        } else if (isInBrowser()) {
            if (mViewHolder != null && mViewHolder.onBackPressed()) {
                return true;
            }
            goToTabSwitcher();
            return true;
        }
//        else if (!isInLauncher()) {
//            goToLauncher();
//            return true;
//        }
        return false;
    }

    public void transitionToBrowser(Rect startRect, Runnable endRunnable) {

        mTabSwitcherLayout.setVisibility(View.VISIBLE);
        mSwitcher.setVisibility(View.INVISIBLE);
//        mSwitcherBottomBar.setVisibility(INVISIBLE);
//        mSwitcherTopBar.setVisibility(INVISIBLE);

        hideBrowser();

        Rect endRect = new Rect(0, 0, mTabSwitcherLayout.getWidth(), mTabSwitcherLayout.getHeight());
        ValueAnimator animator = ValueAnimator.ofFloat(0, 1f);

//        layout(startRect.left, startRect.top, startRect.right, startRect.bottom);

        animator.setInterpolator(new DecelerateInterpolator(2));
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                float percent = (float) animation.getAnimatedValue();

                int left = (int) (startRect.left + (endRect.left - startRect.left) * percent);
                int top = (int) (startRect.top + (endRect.top - startRect.top) * percent);
                int right = (int) (startRect.right + (endRect.right - startRect.right) * percent);
                int bottom = (int) (startRect.bottom + (endRect.bottom - startRect.bottom) * percent);

                float scale = (startRect.width() + (endRect.width() - startRect.width()) * percent) / endRect.width();

                mBrowserLayout.setPivotX(0);
                mBrowserLayout.setPivotY(0);
                mBrowserLayout.setScaleX(scale);
                mBrowserLayout.setScaleY(scale);

                mBrowserLayout.setTranslationX(left);
                mBrowserLayout.setTranslationY(top);
//                mBrowserLayout.layout(left, top, right, bottom);


                showBrowser();
            }
        });
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mLauncherLayout.setVisibility(View.INVISIBLE);
                if (endRunnable != null) {
                    endRunnable.run();
                }
            }
        });
        animator.setDuration(360);
        animator.start();
    }

    public void onStart() {
        if (mViewHolder != null) {
            mViewHolder.onStart();
        }
    }

    public void onStop() {
        if (mViewHolder != null) {
            mViewHolder.onStop();
        }
    }

    public void onPause() {
        mLauncherLayout.onPause();
    }

    public void onDestroy() {
        mLauncherLayout.onDestroy();
    }

    public void onSaveInstanceState(Bundle outState) {
        mLauncherLayout.onSaveInstanceState(outState);
    }


}
