// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.incognito.IncognitoNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * A TabModel implementation that handles off the record tabs.
 *
 * <p>
 * This is not thread safe and must only be operated on the UI thread.
 *
 * <p>
 * The lifetime of this object is not tied to that of the native TabModel.  This ensures the
 * native TabModel is present when at least one incognito Tab has been created and added.  When
 * no Tabs remain, the native model will be destroyed and only rebuilt when a new incognito Tab
 * is created.
 */
public class IncognitoTabModel implements TabModel {
    /** Creates TabModels for use in IncognitoModel. */
    public interface IncognitoTabModelDelegate {
        /** Creates a fully working TabModel to delegate calls to. */
        TabModel createTabModel();

        /** @return Whether Incognito Tabs exist. */
        boolean doIncognitoTabsExist();

        /**
         * @param model {@link TabModel} to act on.
         * @return Whether the provided {@link TabModel} is currently selected in the corresponding
         * {@link IncognitoTabModelDelegate}.
         */
        boolean isCurrentModel(TabModel model);
    }

    private final IncognitoTabModelDelegate mDelegate;
    private final ObserverList<TabModelObserver> mObservers = new ObserverList<TabModelObserver>();
    private TabModel mDelegateModel;
    private boolean mIsAddingTab;

    /**
     * Constructor for IncognitoTabModel.
     */
    public IncognitoTabModel(IncognitoTabModelDelegate tabModelCreator) {
        mDelegate = tabModelCreator;
        mDelegateModel = EmptyTabModel.getInstance();
    }

    /**
     * Ensures that the real TabModel has been created.
     */
    protected void ensureTabModelImpl() {
        ThreadUtils.assertOnUiThread();
        if (!(mDelegateModel instanceof EmptyTabModel)) return;

        IncognitoNotificationManager.showIncognitoNotification();
        mDelegateModel = mDelegate.createTabModel();
        for (TabModelObserver observer : mObservers) {
            mDelegateModel.addObserver(observer);
        }
    }

    /**
     * @return The TabModel that this {@link IncognitoTabModel} is delegating calls to.
     */
    protected TabModel getDelegateModel() {
        return mDelegateModel;
    }

    /**
     * Destroys the Incognito profile when all Incognito tabs have been closed.  Also resets the
     * delegate TabModel to be a stub EmptyTabModel.
     */
    protected void destroyIncognitoIfNecessary() {
        ThreadUtils.assertOnUiThread();
        if (!isEmpty() || mDelegateModel instanceof EmptyTabModel || mIsAddingTab) {
            return;
        }

        Profile profile = getProfile();
        mDelegateModel.destroy();

        // Only delete the incognito profile if there are no incognito tabs open in any tab
        // model selector as the profile is shared between them.
        if (profile != null && !mDelegate.doIncognitoTabsExist()) {
            IncognitoNotificationManager.dismissIncognitoNotification();

            profile.destroyWhenAppropriate();
        }

        mDelegateModel = EmptyTabModel.getInstance();
    }

    private boolean isEmpty() {
        return getComprehensiveModel().getCount() == 0;
    }

    @Override
    public Profile getProfile() {
        if (mDelegateModel instanceof TabModelJniBridge) {
            TabModelJniBridge tabModel = (TabModelJniBridge) mDelegateModel;
            return tabModel.isNativeInitialized() ? tabModel.getProfile() : null;
        }
        return mDelegateModel.getProfile();
    }

    @Override
    public boolean isIncognito() {
        return true;
    }

    @Override
    public boolean closeTab(Tab tab) {
        boolean retVal = mDelegateModel.closeTab(tab);
        destroyIncognitoIfNecessary();
        return retVal;
    }

    @Override
    public boolean closeTab(Tab tab, boolean animate, boolean uponExit, boolean canUndo) {
        boolean retVal = mDelegateModel.closeTab(tab, animate, uponExit, canUndo);
        destroyIncognitoIfNecessary();
        return retVal;
    }

    @Override
    public boolean closeTab(
            Tab tab, Tab recommendedNextTab, boolean animate, boolean uponExit, boolean canUndo) {
        boolean retVal =
                mDelegateModel.closeTab(tab, recommendedNextTab, animate, uponExit, canUndo);
        destroyIncognitoIfNecessary();
        return retVal;
    }

    @Override
    public Tab getNextTabIfClosed(int id) {
        return mDelegateModel.getNextTabIfClosed(id);
    }

    @Override
    public void closeMultipleTabs(List<Tab> tabs, boolean canUndo) {
        mDelegateModel.closeMultipleTabs(tabs, canUndo);
        destroyIncognitoIfNecessary();
    }

    @Override
    public void closeAllTabs() {
        mDelegateModel.closeAllTabs();
        destroyIncognitoIfNecessary();
    }

    @Override
    public void closeAllTabs(boolean allowDelegation, boolean uponExit) {
        mDelegateModel.closeAllTabs(allowDelegation, uponExit);
        destroyIncognitoIfNecessary();
    }

    @Override
    public int getCount() {
        return mDelegateModel.getCount();
    }

    @Override
    public Tab getTabAt(int index) {
        return mDelegateModel.getTabAt(index);
    }

    @Override
    public int indexOf(Tab tab) {
        return mDelegateModel.indexOf(tab);
    }

    @Override
    public int index() {
        return mDelegateModel.index();
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type) {
        mDelegateModel.setIndex(i, type);
    }

    @Override
    public boolean isCurrentModel() {
        return mDelegate.isCurrentModel(this);
    }

    @Override
    public void moveTab(int id, int newIndex) {
        mDelegateModel.moveTab(id, newIndex);
    }

    @Override
    public void destroy() {
        mDelegateModel.destroy();
    }

    @Override
    public boolean isClosurePending(int tabId) {
        return mDelegateModel.isClosurePending(tabId);
    }

    @Override
    public boolean supportsPendingClosures() {
        return mDelegateModel.supportsPendingClosures();
    }

    @Override
    public TabList getComprehensiveModel() {
        return mDelegateModel.getComprehensiveModel();
    }

    @Override
    public void commitAllTabClosures() {
        // Return early if no tabs are open. In particular, we don't want to destroy the incognito
        // tab model, in case we are about to add a tab to it.
        if (isEmpty()) return;
        mDelegateModel.commitAllTabClosures();
        destroyIncognitoIfNecessary();
    }

    @Override
    public void commitTabClosure(int tabId) {
        mDelegateModel.commitTabClosure(tabId);
        destroyIncognitoIfNecessary();
    }

    @Override
    public void cancelTabClosure(int tabId) {
        mDelegateModel.cancelTabClosure(tabId);
    }

    @Override
    public void addTab(Tab tab, int index, @TabLaunchType int type) {
        mIsAddingTab = true;
        ensureTabModelImpl();
        mDelegateModel.addTab(tab, index, type);
        mIsAddingTab = false;
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mObservers.addObserver(observer);
        mDelegateModel.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mObservers.removeObserver(observer);
        mDelegateModel.removeObserver(observer);
    }

    @Override
    public void removeTab(Tab tab) {
        mDelegateModel.removeTab(tab);
        // Call destroyIncognitoIfNecessary() in case the last incognito tab in this model is
        // reparented to a different activity. See crbug.com/611806.
        destroyIncognitoIfNecessary();
    }

    @Override
    public void openMostRecentlyClosedTab() {
    }
}
