package com.ark.browser.ui.widget.homepage;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.graphics.Rect;
import android.view.View;
import android.view.animation.DecelerateInterpolator;

import com.ark.browser.tab.TabListManager;
import com.ark.browser.ui.fragment.wallpaper.WallpaperSelectFragment;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.impl.AttachListDialogFragment;
import com.zpj.utils.ClickHelper;

import org.chromium.chrome.R;

public class TabSwitcherManager implements SwitcherRecyclerLayout.Callback {


    private final View mBrowserLayout;
    private final View mLauncherLayout;
    private final TabSwitcherLayout mTabSwitcherLayout;
    private final SwitcherRecyclerLayout mSwitcher;

    public TabSwitcherManager(View view) {
        mLauncherLayout = view.findViewById(R.id.launcher_layout);
        mBrowserLayout = view.findViewById(R.id.browser_layout);
        mTabSwitcherLayout = view.findViewById(R.id.tab_switcher_layout);
        mSwitcher = mTabSwitcherLayout.getSwitcher();
        mSwitcher.addCallback(this);
        mSwitcher.setAdapter(new ArkTabAdapter());

        // TODO
        ClickHelper.with(mLauncherLayout)
                .setOnClickListener((view1, x, y) -> {
                    if (isInLauncher()) {
                        mSwitcher.open();
                    }
                })
                .setOnLongClickListener(new ClickHelper.OnLongClickListener() {
                    @Override
                    public boolean onLongClick(View view, float x, float y) {
                        ZDialog.attach()
                                .addItem("壁纸")
                                .setOnSelectListener((fragment, i, s) -> {
                                    if (i == 0) {
                                        fragment.start(new WallpaperSelectFragment());
                                    }
                                    fragment.dismiss();
                                })
                                .setTouchPoint(x, y)
                                .show(view);
                        return true;
                    }
                });


    }

    public SwitcherRecyclerLayout getSwitcher() {
        return mSwitcher;
    }

    public TabSwitcherLayout getTabSwitcherLayout() {
        return mTabSwitcherLayout;
    }

    public void showSwitcher() {
        mLauncherLayout.setVisibility(View.VISIBLE);
        mBrowserLayout.setVisibility(View.INVISIBLE);
        mTabSwitcherLayout.showSwitcher();
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
        TabListManager.getInstance().selectTab(position, false);
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
        return isVisible(mTabSwitcherLayout) && !isVisible(mTabSwitcherLayout.getSwitcher());
    }

    public boolean isInLauncher() {
        return isVisible(mLauncherLayout) && !isVisible(mTabSwitcherLayout);
    }

    public boolean isInTabSwitcher() {
        return isVisible(mTabSwitcherLayout) && isVisible(mTabSwitcherLayout.getSwitcher());
    }

    public void goToBrowser(boolean animated) {
        mLauncherLayout.setVisibility(View.INVISIBLE);
        mBrowserLayout.setVisibility(View.VISIBLE);
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
        } else if (!isInLauncher()) {
            goToLauncher();
            return true;
        }
        return false;
    }

    public void transitionToBrowser(Rect startRect, Runnable endRunnable) {

        mTabSwitcherLayout.setVisibility(View.VISIBLE);
        mSwitcher.setVisibility(View.INVISIBLE);
//        mSwitcherBottomBar.setVisibility(INVISIBLE);
//        mSwitcherTopBar.setVisibility(INVISIBLE);

        mBrowserLayout.setVisibility(View.INVISIBLE);

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


                mBrowserLayout.setVisibility(View.VISIBLE);
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


}
