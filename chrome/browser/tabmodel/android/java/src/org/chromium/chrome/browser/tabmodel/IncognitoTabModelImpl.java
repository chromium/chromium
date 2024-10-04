// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * A TabModel implementation that handles off the record tabs.
 *
 * <p>This is not thread safe and must only be operated on the UI thread.
 *
 * <p>The lifetime of this object is not tied to that of the native TabModel. This ensures the
 * native TabModel is present when at least one incognito Tab has been created and added. When no
 * Tabs remain, the native model will be destroyed and only rebuilt when a new incognito Tab is
 * created.
 */
class IncognitoTabModelImpl implements IncognitoTabModelInternal {
    /** Creates TabModels for use in IncognitoModel. */
    public interface IncognitoTabModelDelegate {
        /** Creates a fully working TabModel to delegate calls to. */
        TabModelInternal createTabModel();
    }

    private final IncognitoTabModelDelegate mDelegate;
    private final ObserverList<TabModelObserver> mObservers = new ObserverList<>();
    private final ObserverList<IncognitoTabModelObserver> mIncognitoObservers =
            new ObserverList<>();
    private final Callback<Tab> mDelegateModelCurrentTabSupplierObserver;
    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private final Callback<Integer> mDelegateModelTabCountSupplierObserver;
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>();

    private TabModelInternal mDelegateModel;
    private int mCountOfAddingOrClosingTabs;
    private boolean mActive;

    /** Constructor for IncognitoTabModel. */
    IncognitoTabModelImpl(IncognitoTabModelDelegate tabModelCreator) {
        mDelegate = tabModelCreator;
        mDelegateModel = EmptyTabModel.getInstance(true);
        mDelegateModelCurrentTabSupplierObserver = mCurrentTabSupplier::set;
        mDelegateModelTabCountSupplierObserver = mTabCountSupplier::set;
        mTabCountSupplier.set(0);
    }

    /** Ensures that the real TabModel has been created. */
    protected void ensureTabModelImpl() {
        ThreadUtils.assertOnUiThread();
        if (!(mDelegateModel instanceof EmptyTabModel)) return;

        mDelegateModel = mDelegate.createTabModel();
        mDelegateModel
                .getCurrentTabSupplier()
                .addObserver(mDelegateModelCurrentTabSupplierObserver);
        mDelegateModel.getTabCountSupplier().addObserver(mDelegateModelTabCountSupplierObserver);
        for (TabModelObserver observer : mObservers) {
            mDelegateModel.addObserver(observer);
        }
    }

    /**
     * Resets the delegate TabModel to be a stub EmptyTabModel and notifies
     * {@link IncognitoTabModelObserver}s.
     */
    protected void destroyIncognitoIfNecessary() {
        ThreadUtils.assertOnUiThread();
        if (!isEmpty()
                || mDelegateModel instanceof EmptyTabModel
                || mCountOfAddingOrClosingTabs != 0) {
            return;
        }

        for (IncognitoTabModelObserver observer : mIncognitoObservers) {
            observer.didBecomeEmpty();
        }

        mDelegateModel
                .getCurrentTabSupplier()
                .removeObserver(mDelegateModelCurrentTabSupplierObserver);
        mDelegateModel.getTabCountSupplier().removeObserver(mDelegateModelTabCountSupplierObserver);
        mDelegateModel.destroy();
        mCurrentTabSupplier.set(null);
        mTabCountSupplier.set(0);

        mDelegateModel = EmptyTabModel.getInstance(true);
    }

    private boolean isEmpty() {
        return getComprehensiveModel().getCount() == 0;
    }

    // Triggers IncognitoTabModelObserver.wasFirstTabCreated function. This function should only be
    // called just after the first tab is created.
    private void notifyIncognitoObserverFirstTabCreated(boolean shouldTrigger) {
        if (!shouldTrigger) return;
        assert getCount() == 1;

        for (IncognitoTabModelObserver observer : mIncognitoObservers) {
            observer.wasFirstTabCreated();
        }
    }

    @Override
    public Profile getProfile() {
        return mDelegateModel.getProfile();
    }

    @Override
    public boolean isIncognito() {
        return true;
    }

    @Override
    public boolean isOffTheRecord() {
        return mDelegateModel.isOffTheRecord();
    }

    @Override
    public boolean isIncognitoBranded() {
        return mDelegateModel.isIncognitoBranded();
    }

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        mCountOfAddingOrClosingTabs++;
        boolean retVal = mDelegateModel.closeTabs(tabClosureParams);
        mCountOfAddingOrClosingTabs--;
        destroyIncognitoIfNecessary();
        return retVal;
    }

    @Override
    public Tab getNextTabIfClosed(int id, boolean uponExit) {
        return mDelegateModel.getNextTabIfClosed(id, uponExit);
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
    public @Nullable Tab getTabById(int tabId) {
        return mDelegateModel.getTabById(tabId);
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
    public @NonNull ObservableSupplier<Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type) {
        mDelegateModel.setIndex(i, type);
    }

    @Override
    public boolean isActiveModel() {
        return mActive;
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
    public void notifyAllTabsClosureUndone() {
        mDelegateModel.notifyAllTabsClosureUndone();
    }

    @Override
    public @NonNull ObservableSupplier<Integer> getTabCountSupplier() {
        return mTabCountSupplier;
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        mCountOfAddingOrClosingTabs++;
        ensureTabModelImpl();
        boolean shouldTriggerFirstTabCreated = getCount() == 0;
        mDelegateModel.addTab(tab, index, type, creationState);
        notifyIncognitoObserverFirstTabCreated(shouldTriggerFirstTabCreated);
        mCountOfAddingOrClosingTabs--;
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
    public int getTabCountNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        assert false : "Not reached.";
        return 0;
    }

    @Override
    public void closeTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        assert false : "Not reached.";
    }

    @Override
    public void addIncognitoObserver(IncognitoTabModelObserver observer) {
        mIncognitoObservers.addObserver(observer);
    }

    @Override
    public void removeIncognitoObserver(IncognitoTabModelObserver observer) {
        mIncognitoObservers.removeObserver(observer);
    }

    @Override
    public void removeTab(Tab tab) {
        mCountOfAddingOrClosingTabs++;
        mDelegateModel.removeTab(tab);
        mCountOfAddingOrClosingTabs--;
        // Call destroyIncognitoIfNecessary() in case the last incognito tab in this model is
        // reparented to a different activity. See crbug.com/611806.
        destroyIncognitoIfNecessary();
    }

    @Override
    public void openMostRecentlyClosedEntry() {}

    @Override
    public void setActive(boolean active) {
        mActive = active;
        if (active) ensureTabModelImpl();
        mDelegateModel.setActive(active);
        if (!active) destroyIncognitoIfNecessary();
    }
}
