// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Implement methods shared across the different model implementations.
 */
public abstract class TabModelSelectorBase implements TabModelSelector {
    private static final int MODEL_NOT_FOUND = -1;

    private static TabModelSelectorObserver sObserver;

    private List<TabModel> mTabModels = new ArrayList<>();

    /**
     * This is a dummy implementation intended to stub out TabModelFilterProvider before native is
     * ready.
     */
    private TabModelFilterProvider mTabModelFilterProvider = new TabModelFilterProvider();

    private int mActiveModelIndex;
    private final ObserverList<TabModelSelectorObserver> mObservers = new ObserverList<>();
    private boolean mTabStateInitialized;
    private boolean mStartIncognito;

    private final TabCreatorManager mTabCreatorManager;

    protected TabModelSelectorBase(TabCreatorManager tabCreatorManager, boolean startIncognito) {
        mTabCreatorManager = tabCreatorManager;
        mStartIncognito = startIncognito;
    }

    protected final void initialize(TabModel... models) {
        // Only normal and incognito supported for now.
        assert mTabModels.isEmpty();
        assert models.length > 0;

        Collections.addAll(mTabModels, models);
        mActiveModelIndex = getModelIndex(mStartIncognito);
        assert mActiveModelIndex != MODEL_NOT_FOUND;
        mTabModelFilterProvider = new TabModelFilterProvider(mTabModels);

        TabModelObserver tabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                notifyChanged();
                notifyNewTabCreated(tab);
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                notifyChanged();
            }

            @Override
            public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                notifyChanged();
            }
        };
        for (TabModel model : models) {
            model.addObserver(tabModelObserver);
        }

        if (sObserver != null) {
            addObserver(sObserver);
        }

        notifyChanged();
    }

    public static void setObserverForTests(TabModelSelectorObserver observer) {
        sObserver = observer;
    }

    @Override
    public void selectModel(boolean incognito) {
        if (mTabModels.size() == 0) {
            mStartIncognito = incognito;
            return;
        }
        int newIndex = getModelIndex(incognito);
        assert newIndex != MODEL_NOT_FOUND;
        if (newIndex == mActiveModelIndex) return;

        TabModel newModel = mTabModels.get(newIndex);
        TabModel previousModel = mTabModels.get(mActiveModelIndex);
        mActiveModelIndex = newIndex;
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onTabModelSelected(newModel, previousModel);
        }
    }

    @Override
    public Tab getCurrentTab() {
        return TabModelUtils.getCurrentTab(getCurrentModel());
    }

    @Override
    public int getCurrentTabId() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    @Override
    public TabModel getModelForTabId(int id) {
        for (int i = 0; i < mTabModels.size(); i++) {
            TabModel model = mTabModels.get(i);
            if (TabModelUtils.getTabById(model, id) != null || model.isClosurePending(id)) {
                return model;
            }
        }
        return null;
    }

    @Override
    public TabModel getCurrentModel() {
        if (mTabModels.size() == 0) return EmptyTabModel.getInstance();
        return mTabModels.get(mActiveModelIndex);
    }

    @Override
    public int getCurrentModelIndex() {
        return mActiveModelIndex;
    }

    @Override
    public TabModel getModel(boolean incognito) {
        int index = getModelIndex(incognito);
        if (index == MODEL_NOT_FOUND) return EmptyTabModel.getInstance();
        return mTabModels.get(index);
    }

    private int getModelIndex(boolean incognito) {
        for (int i = 0; i < mTabModels.size(); i++) {
            if (incognito == mTabModels.get(i).isIncognito()) return i;
        }
        return MODEL_NOT_FOUND;
    }

    @Override
    public TabModelFilterProvider getTabModelFilterProvider() {
        return mTabModelFilterProvider;
    }

    @Override
    public boolean isIncognitoSelected() {
        if (mTabModels.size() == 0) return mStartIncognito;
        return getCurrentModel().isIncognito();
    }

    @Override
    public List<TabModel> getModels() {
        return mTabModels;
    }

    @Override
    public Tab openNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, boolean incognito) {
        return mTabCreatorManager.getTabCreator(incognito).createNewTab(
                loadUrlParams, type, parent);
    }

    @Override
    public boolean closeTab(Tab tab) {
        for (int i = 0; i < getModels().size(); i++) {
            TabModel model = mTabModels.get(i);
            if (model.indexOf(tab) >= 0) {
                return model.closeTab(tab);
            }
        }
        assert false : "Tried to close a tab that is not in any model!";
        return false;
    }

    @Override
    public void commitAllTabClosures() {
        for (int i = 0; i < mTabModels.size(); i++) {
            mTabModels.get(i).commitAllTabClosures();
        }
    }

    @Override
    public Tab getTabById(int id) {
        for (int i = 0; i < getModels().size(); i++) {
            Tab tab = TabModelUtils.getTabById(mTabModels.get(i), id);
            if (tab != null) return tab;
        }
        return null;
    }

    @Override
    public void closeAllTabs() {
        closeAllTabs(false);
    }

    @Override
    public void closeAllTabs(boolean uponExit) {
        for (int i = 0; i < getModels().size(); i++) {
            mTabModels.get(i).closeAllTabs(!uponExit, uponExit);
        }
    }

    @Override
    public int getTotalTabCount() {
        int count = 0;
        for (int i = 0; i < getModels().size(); i++)  {
            count += mTabModels.get(i).getCount();
        }
        return count;
    }

    @Override
    public void addObserver(TabModelSelectorObserver observer) {
        if (!mObservers.hasObserver(observer)) mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelSelectorObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void setCloseAllTabsDelegate(CloseAllTabsDelegate delegate) { }

    /**
     * Marks the task state being initialized and notifies observers.
     */
    public void markTabStateInitialized() {
        if (mTabStateInitialized) return;
        mTabStateInitialized = true;
        for (TabModelSelectorObserver listener : mObservers) listener.onTabStateInitialized();
    }

    @Override
    public boolean isTabStateInitialized() {
        return mTabStateInitialized;
    }

    @Override
    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {}

    @Override
    public void mergeState() {}

    @Override
    public void destroy() {
        mTabModelFilterProvider.destroy();
        for (int i = 0; i < getModels().size(); i++) mTabModels.get(i).destroy();
        mTabModels.clear();
    }

    /**
     * Notifies all the listeners that the {@link TabModelSelector} or its {@link TabModel} has
     * changed.
     */
    protected void notifyChanged() {
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onChange();
        }
    }

    /**
     * Notifies all the listeners that a new tab has been created.
     * @param tab The tab that has been created.
     */
    private void notifyNewTabCreated(Tab tab) {
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onNewTabCreated(tab);
        }
    }

    protected TabCreatorManager getTabCreatorManager() {
        return mTabCreatorManager;
    }
}
