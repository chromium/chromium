package com.ark.browser.ui.widget.homepage;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.graphics.Rect;
import android.view.View;
import android.view.animation.DecelerateInterpolator;

import com.ark.browser.tab.TabListManager;

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
    }

    public SwitcherRecyclerLayout getSwitcher() {
        return mSwitcher;
    }

    public TabSwitcherLayout getTabSwitcherLayout() {
        return mTabSwitcherLayout;
    }

    public void showSwitcher() {
        mBrowserLayout.setVisibility(View.INVISIBLE);
//        ArkCompositorViewHolder viewHolder = mBrowserLayout.findViewById(R.id.compositor_view_holder);
//        viewHolder.setTab(null);
        mTabSwitcherLayout.showSwitcher();
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

    }

    @Override
    public void onAnimExpand(float percent) {

    }

    @Override
    public void onAnimIdle(float percent) {

    }

    @Override
    public void onClose(float percent) {

    }

    public void goToBrowser(boolean animated) {
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
        // TODO make it
//        mSwitcher.setVisibility(View.INVISIBLE);
        goToBrowser();
    }

    public void goToLauncher() {
        goToLauncher(true);
    }

    public boolean onBackPressed() {
        if (mSwitcher.getVisibility() == View.VISIBLE) {
            mSwitcher.close();
            return true;
        }
//        else if (!isInLauncher()) {
////            if (isFromBrowser) {
////                isFromBrowser = false;
////                goToBrowser();
////            } else {
////                goToLauncher();
////            }
//            goToLauncher();
//            return true;
//        }

        return false;
//        return mSlideUp.onBackPressed();
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
                if (endRunnable != null) {
                    endRunnable.run();
                }
            }
        });
        animator.setDuration(360);
        animator.start();
    }



}
