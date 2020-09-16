// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

import android.view.ActionMode;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manages the interactions with the find toolbar.
 */
public class FindToolbarManager {
    private FindToolbar mFindToolbar;
    private final ViewStub mFindToolbarStub;
    private final TabModelSelector mTabModelSelector;
    private final WindowAndroid mWindowAndroid;
    private final ActionMode.Callback mCallback;
    private final ObserverList<FindToolbarObserver> mObservers;

    /**
     * Creates an instance of a {@link FindToolbarManager}.
     * @param findToolbarStub The {@link ViewStub} where for the find toolbar.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param windowAndroid The {@link WindowAndroid} for the containing activity.
     * @param callback The ActionMode.Callback that will be used when selection occurs on the
     *         {@link FindToolbar}.
     */
    public FindToolbarManager(ViewStub findToolbarStub, TabModelSelector tabModelSelector,
            WindowAndroid windowAndroid, ActionMode.Callback callback) {
        mFindToolbarStub = findToolbarStub;
        mTabModelSelector = tabModelSelector;
        mWindowAndroid = windowAndroid;
        mCallback = callback;
        mObservers = new ObserverList<FindToolbarObserver>();
    }

    /**
     * @return Whether the find toolbar is currently showing.
     */
    public boolean isShowing() {
        return mFindToolbar != null && mFindToolbar.getVisibility() == View.VISIBLE;
    }

    /**
     * Hides the toolbar and clears the selection on the screen.
     */
    public void hideToolbar() {
        hideToolbar(true);
    }

    /**
     * Hides the toolbar.
     * @param clearSelection Whether the selection on the page should be cleared.
     */
    public void hideToolbar(boolean clearSelection) {
        if (mFindToolbar == null) return;

        mFindToolbar.deactivate(clearSelection);
    }

    /**
     * Shows the toolbar if it's not already visible otherwise activates.
     *
     * TODO(crrev.com/959841): Return a boolean for whether the toolbar was actually shown.
     */
    public void showToolbar() {
        if (mFindToolbar == null) {
            mFindToolbar = (FindToolbar) mFindToolbarStub.inflate();
            mFindToolbar.setTabModelSelector(mTabModelSelector);
            mFindToolbar.setWindowAndroid(mWindowAndroid);
            mFindToolbar.setActionModeCallbackForTextEdit(mCallback);
            mFindToolbar.setObserver(new FindToolbarObserver() {
                @Override
                public void onFindToolbarShown() {
                    for (FindToolbarObserver observer : mObservers) {
                        observer.onFindToolbarShown();
                    }
                }

                @Override
                public void onFindToolbarHidden() {
                    for (FindToolbarObserver observer : mObservers) {
                        observer.onFindToolbarHidden();
                    }
                }
            });
        }

        mFindToolbar.activate();
    }

    /**
     * Sets the find query text string.
     */
    public void setFindQuery(String findText) {
        assert mFindToolbar != null;
        mFindToolbar.setFindQuery(findText);
    }

    /**
     * Add an observer for find in page changes.
     */
    public void addObserver(FindToolbarObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer for find in page changes.
     */
    public void removeObserver(FindToolbarObserver observer) {
        mObservers.removeObserver(observer);
    }
}
