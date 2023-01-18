package com.ark.browser.ui.widget.homepage;

import android.view.View;

import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabListManager;

public class TabSwitcherManager implements SwitcherRecyclerLayout.Callback {


    private final SwitcherRecyclerLayout mSwitcher;

    private final Adapter mAdapter;

    public TabSwitcherManager(SwitcherRecyclerLayout switcher) {
        mSwitcher = switcher;
        mSwitcher.addCallback(this);
        mAdapter = new ArkTabAdapter();
        mSwitcher.setAdapter(mAdapter);
    }


    @Override
    public boolean onSwipe(int position) {
        if (mAdapter.getCount() == 0) {
            // TODO show empty
        }
        return false;
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
        mSwitcher.setVisibility(View.VISIBLE);
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
        // TODO
        mSwitcher.setVisibility(View.INVISIBLE);
    }

    public void goToBrowser() {
        goToBrowser(true);
    }

    public void goToTabSwitcher() {
        // TODO
        mSwitcher.setVisibility(View.VISIBLE);
        PageSnapshotManager.getInstance().cacheCurrentPage();
        mSwitcher.post(mSwitcher::goToIdle);
    }

    public void goToLauncher(boolean animated) {
        // TODO
        mSwitcher.setVisibility(View.INVISIBLE);
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



}
