// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

/** Singleton class intended to stub out Tab model before it has been created. */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class EmptyTabModel implements IncognitoTabModelInternal {
    private boolean mIsIncognito;

    /**
     * Used to mock TabModel. Application code should use getInstance() to construct an
     * EmptyTabModel.
     */
    @VisibleForTesting
    public EmptyTabModel() {}

    private EmptyTabModel(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    // "Initialization on demand holder idiom"
    private static class LazyHolder {
        private static final EmptyTabModel INSTANCE = new EmptyTabModel(false);
        private static final EmptyTabModel INCOGNITO_INSTANCE = new EmptyTabModel(true);
    }

    /**
     * Get the singleton instance of EmptyTabModel.
     *
     * @return the instance of EmptyTabModel
     */
    public static EmptyTabModel getInstance(boolean isIncognito) {
        return isIncognito ? LazyHolder.INCOGNITO_INSTANCE : LazyHolder.INSTANCE;
    }

    @Override
    public Profile getProfile() {
        return null;
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public boolean isOffTheRecord() {
        return mIsIncognito;
    }

    @Override
    public boolean isIncognitoBranded() {
        return mIsIncognito;
    }

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        return false;
    }

    @Override
    public Tab getNextTabIfClosed(int id, boolean uponExit) {
        return null;
    }

    @Override
    public int getCount() {
        // We must return 0 to be consistent with getTab(i)
        return 0;
    }

    @Override
    public Tab getTabAt(int position) {
        return null;
    }

    @Override
    public @Nullable Tab getTabById(int tabId) {
        return null;
    }

    @Override
    public int indexOf(Tab tab) {
        return INVALID_TAB_INDEX;
    }

    @Override
    public int index() {
        return INVALID_TAB_INDEX;
    }

    @Override
    public @NonNull ObservableSupplier<Tab> getCurrentTabSupplier() {
        assert false : "This should be unreachable in production, it may be mocked for testing.";
        return new ObservableSupplierImpl<>();
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type) {}

    @Override
    public boolean isActiveModel() {
        return false;
    }

    @Override
    public void moveTab(int id, int newIndex) {}

    @Override
    public void destroy() {}

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
    public void notifyAllTabsClosureUndone() {}

    @Override
    public boolean supportsPendingClosures() {
        return false;
    }

    @Override
    public @NonNull ObservableSupplier<Integer> getTabCountSupplier() {
        assert false : "This should be unreachable in production, it may be mocked for testing.";
        return new ObservableSupplierImpl<>();
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        assert false;
    }

    @Override
    public void addObserver(TabModelObserver observer) {}

    @Override
    public void removeObserver(TabModelObserver observer) {}

    @Override
    public int getTabCountNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        return 0;
    }

    @Override
    public void closeTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {}

    @Override
    public void removeTab(Tab tab) {}

    @Override
    public void openMostRecentlyClosedEntry() {}

    @Override
    public void addIncognitoObserver(IncognitoTabModelObserver observer) {}

    @Override
    public void removeIncognitoObserver(IncognitoTabModelObserver observer) {}

    @Override
    public void setActive(boolean active) {}
}
