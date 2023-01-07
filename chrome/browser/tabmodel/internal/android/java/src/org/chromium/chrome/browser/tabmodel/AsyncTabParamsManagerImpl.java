// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.SparseArray;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Data that will be used later when a tab is opened via an intent. Often only the necessary
 * subset of the data will be set. All data is removed once the tab finishes initializing.
 */
public class AsyncTabParamsManagerImpl implements AsyncTabParamsManager {
    /** A map of tab IDs to AsyncTabParams consumed by Activities started asynchronously. */
    private final SparseArray<AsyncTabParams> mAsyncTabParams = new SparseArray<>();

    private boolean mAddedToIncognitoTabHostRegistry;

    @Override
    public void add(int tabId, AsyncTabParams params) {
        mAsyncTabParams.put(tabId, params);

        if (!mAddedToIncognitoTabHostRegistry) {
            // Make sure async incognito tabs are taken into account when, for example,
            // checking if any incognito tabs exist.
            IncognitoTabHostRegistry.getInstance().register(new AsyncTabsIncognitoTabHost(this));
            mAddedToIncognitoTabHostRegistry = true;
        }
    }

    @Override
    public boolean hasParamsForTabId(int tabId) {
        return mAsyncTabParams.get(tabId) != null;
    }

    @Override
    public boolean hasParamsWithTabToReparent() {
        for (int i = 0; i < mAsyncTabParams.size(); i++) {
            if (mAsyncTabParams.get(mAsyncTabParams.keyAt(i)).getTabToReparent() == null) continue;
            return true;
        }
        return false;
    }

    @Override
    public SparseArray<AsyncTabParams> getAsyncTabParams() {
        return mAsyncTabParams;
    }

    @Override
    public AsyncTabParams remove(int tabId) {
        AsyncTabParams data = mAsyncTabParams.get(tabId);
        mAsyncTabParams.remove(tabId);
        return data;
    }

    AsyncTabParamsManagerImpl() {}

    private static class AsyncTabsIncognitoTabHost implements IncognitoTabHost {
        private final AsyncTabParamsManager mAsyncTabParamsManager;

        private AsyncTabsIncognitoTabHost(AsyncTabParamsManager asyncTabParamsManager) {
            mAsyncTabParamsManager = asyncTabParamsManager;
        }

        @Override
        public boolean hasIncognitoTabs() {
            SparseArray<AsyncTabParams> asyncTabParams = mAsyncTabParamsManager.getAsyncTabParams();
            for (int i = 0; i < asyncTabParams.size(); i++) {
                Tab tab = asyncTabParams.valueAt(i).getTabToReparent();
                if (tab != null && tab.isIncognito()) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public void closeAllIncognitoTabs() {
            SparseArray<AsyncTabParams> asyncTabParams = mAsyncTabParamsManager.getAsyncTabParams();
            for (int i = 0; i < asyncTabParams.size(); i++) {
                Tab tab = asyncTabParams.valueAt(i).getTabToReparent();
                if (tab != null && tab.isIncognito()) {
                    mAsyncTabParamsManager.remove(tab.getId());
                }
            }
        }

        @Override
        public boolean isActiveModel() {
            return false;
        }
    }
}
