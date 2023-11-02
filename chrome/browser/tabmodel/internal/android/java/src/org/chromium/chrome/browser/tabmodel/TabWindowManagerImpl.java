// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.util.Pair;
import android.util.SparseArray;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Manages multiple {@link TabModelSelector} instances, each owned by different {@link Activity}s.
 *
 * Also manages tabs being reparented in AsyncTabParamsManager.
 */
public class TabWindowManagerImpl implements ActivityStateListener, TabWindowManager {
    private TabModelSelectorFactory mSelectorFactory;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final int mMaxSelectors;

    private List<TabModelSelector> mSelectors = new ArrayList<>();

    private Map<Activity, TabModelSelector> mAssignments = new HashMap<>();

    TabWindowManagerImpl(TabModelSelectorFactory selectorFactory,
            AsyncTabParamsManager asyncTabParamsManager, int maxSelectors) {
        mSelectorFactory = selectorFactory;
        mAsyncTabParamsManager = asyncTabParamsManager;
        ApplicationStatus.registerStateListenerForAllActivities(this);
        mMaxSelectors = maxSelectors;
        for (int i = 0; i < mMaxSelectors; i++) mSelectors.add(null);
    }

    @Override
    public int getMaxSimultaneousSelectors() {
        return mMaxSelectors;
    }

    @Override
    public Pair<Integer, TabModelSelector> requestSelector(Activity activity,
            TabCreatorManager tabCreatorManager, NextTabPolicySupplier nextTabPolicySupplier,
            int index) {
        if (index < 0 || index >= mSelectors.size()) return null;

        // Return the already existing selector if found.
        if (mAssignments.get(activity) != null) {
            TabModelSelector assignedSelector = mAssignments.get(activity);
            for (int i = 0; i < mSelectors.size(); i++) {
                if (mSelectors.get(i) == assignedSelector) {
                    return Pair.create(i, assignedSelector);
                }
            }
            throw new IllegalStateException(
                    "TabModelSelector is assigned to an Activity but has no index.");
        }

        if (mSelectors.get(index) != null) {
            for (int i = 0; i < mSelectors.size(); i++) {
                if (mSelectors.get(i) == null) {
                    index = i;
                    break;
                }
            }
        }

        // Too many activities going at once.
        if (mSelectors.get(index) != null) return null;

        TabModelSelector selector = mSelectorFactory.buildSelector(
                activity, tabCreatorManager, nextTabPolicySupplier, index);
        mSelectors.set(index, selector);
        mAssignments.put(activity, selector);

        return Pair.create(index, selector);
    }

    @Override
    public int getIndexForWindow(Activity activity) {
        if (activity == null) return TabWindowManager.INVALID_WINDOW_INDEX;
        TabModelSelector selector = mAssignments.get(activity);
        if (selector == null) return TabWindowManager.INVALID_WINDOW_INDEX;
        int index = mSelectors.indexOf(selector);
        return index == -1 ? TabWindowManager.INVALID_WINDOW_INDEX : index;
    }

    @Override
    public int getNumberOfAssignedTabModelSelectors() {
        return mAssignments.size();
    }

    @Override
    public int getIncognitoTabCount() {
        int count = 0;
        for (int i = 0; i < mSelectors.size(); i++) {
            if (mSelectors.get(i) != null) {
                count += mSelectors.get(i).getModel(true).getCount();
            }
        }

        // Count tabs that are moving between activities (e.g. a tab that was recently reparented
        // and hasn't been attached to its new activity yet).
        SparseArray<AsyncTabParams> asyncTabParams = mAsyncTabParamsManager.getAsyncTabParams();
        for (int i = 0; i < asyncTabParams.size(); i++) {
            Tab tab = asyncTabParams.valueAt(i).getTabToReparent();
            if (tab != null && tab.isIncognito()) count++;
        }
        return count;
    }

    @Override
    public TabModel getTabModelForTab(Tab tab) {
        if (tab == null) return null;

        for (int i = 0; i < mSelectors.size(); i++) {
            TabModelSelector selector = mSelectors.get(i);
            if (selector != null) {
                TabModel tabModel = selector.getModelForTabId(tab.getId());
                if (tabModel != null) return tabModel;
            }
        }

        return null;
    }

    @Override
    public Tab getTabById(int tabId) {
        for (int i = 0; i < mSelectors.size(); i++) {
            TabModelSelector selector = mSelectors.get(i);
            if (selector != null) {
                final Tab tab = selector.getTabById(tabId);
                if (tab != null) return tab;
            }
        }

        if (mAsyncTabParamsManager.hasParamsForTabId(tabId)) {
            return mAsyncTabParamsManager.getAsyncTabParams().get(tabId).getTabToReparent();
        }

        return null;
    }

    @Override
    public TabModelSelector getTabModelSelectorById(int index) {
        return mSelectors.get(index);
    }

    // ActivityStateListener
    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.DESTROYED && mAssignments.containsKey(activity)) {
            int index = mSelectors.indexOf(mAssignments.remove(activity));
            if (index >= 0) {
                mSelectors.set(index, null);
            }
            // TODO(dtrainor): Move TabModelSelector#destroy() calls here.
        }
    }
}
