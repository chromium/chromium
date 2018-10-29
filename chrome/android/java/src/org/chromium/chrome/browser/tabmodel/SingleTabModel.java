// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Simple TabModel that assumes that only one Tab exists.
 */
public class SingleTabModel implements TabModel {
    private final Activity mActivity;
    private final ObserverList<TabModelObserver> mObservers = new ObserverList<>();

    private Tab mTab;
    private boolean mIsIncognito;
    private boolean mBlockNewWindows;

    SingleTabModel(Activity activity, boolean incognito, boolean blockNewWindows) {
        mActivity = activity;
        mIsIncognito = incognito;
        mBlockNewWindows = blockNewWindows;
    }

    /**
     * Sets the Tab that is managed by the SingleTabModel.
     * Should only be called once.
     * @param tab Tab to manage.
     */
    void setTab(Tab tab) {
        mTab = tab;
        assert mTab.isIncognito() == mIsIncognito;
        if (mBlockNewWindows) nativePermanentlyBlockAllNewWindows(mTab);

        for (TabModelObserver observer : mObservers) {
            observer.didAddTab(tab, TabLaunchType.FROM_LINK);
            observer.didSelectTab(tab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        }

        int state = ApplicationStatus.getStateForActivity(mActivity);
        if (state == ActivityState.CREATED || state == ActivityState.STARTED
                || state == ActivityState.RESUMED) {
            mTab.show(TabSelectionType.FROM_USER);
        }
    }

    @Override
    public Profile getProfile() {
        return mTab == null ? null : mTab.getProfile();
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public int getCount() {
        return mTab == null ? 0 : 1;
    }

    @Override
    public int indexOf(Tab tab) {
        return mTab != null && mTab.getId() == tab.getId() ? 0 : INVALID_TAB_INDEX;
    }

    @Override
    public int index() {
        return mTab != null ? 0 : INVALID_TAB_INDEX;
    }

    @Override
    public boolean closeTab(Tab tab) {
        return closeTab(tab, false, false, false);
    }

    @Override
    public boolean closeTab(Tab tab, boolean animate, boolean uponExit, boolean canUndo) {
        if (mTab != null && mTab.getId() == tab.getId()) {
            completeActivity();
            return true;
        }
        return false;
    }

    /**
     * In webapps, calls finish on the activity, but keeps it in recents. In Document mode,
     * finishes and removes from recents. We use mBlockNewWindows flag to distinguish the user
     * of this model.
     */
    private void completeActivity() {
        if (mBlockNewWindows) {
            mActivity.finish();
        } else {
            ApiCompatibilityUtils.finishAndRemoveTask(mActivity);
        }
    }

    @Override
    public void closeAllTabs() {
        closeAllTabs(true, false);
    }

    @Override
    public void closeAllTabs(boolean allowDelegation, boolean uponExit) {
        completeActivity();
    }

    // Tab retrieval functions.
    @Override
    public Tab getTabAt(int position) {
        return position == 0 ? mTab : null;
    }

    @Override
    public void setIndex(int i, final @TabSelectionType int type) {
        assert i == 0;
    }

    @Override
    public boolean isCurrentModel() {
        return true;
    }

    @Override
    public void moveTab(int id, int newIndex) {
        assert false;
    }

    @Override
    public void destroy() {
        if (mTab != null) mTab.destroy();
        mTab = null;
    }

    @Override
    public Tab getNextTabIfClosed(int id) {
        return null;
    }

    @Override
    public boolean isClosurePending(int tabId) {
        return false;
    }

    @Override
    public TabList getComprehensiveModel() {
        return this;
    }

    @Override
    public void commitAllTabClosures() {}

    @Override
    public void commitTabClosure(int tabId) {}

    @Override
    public void cancelTabClosure(int tabId) {}

    @Override
    public boolean supportsPendingClosures() {
        return false;
    }

    @Override
    public void addTab(Tab tab, int index, @TabLaunchType int type) {}

    @Override
    public void removeTab(Tab tab) {
        mTab = null;
        for (TabModelObserver obs : mObservers) obs.tabRemoved(tab);
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    private static native void nativePermanentlyBlockAllNewWindows(Tab nativeTabAndroid);

    @Override
    public void openMostRecentlyClosedTab() {}
}
