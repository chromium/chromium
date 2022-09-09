// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.List;

/**
 * Singleton class intended to stub out Tab model before it has been created.
 */
public class EmptyTabModel implements TabModel {
    /**
     * Used to mock TabModel. Application code should use getInstance() to construct an
     * EmptyTabModel.
     */
    @VisibleForTesting
    public EmptyTabModel() {}

    // "Initialization on demand holder idiom"
    private static class LazyHolder {
        private static final EmptyTabModel INSTANCE = new EmptyTabModel();
    }

    /**
     * Get the singleton instance of EmptyTabModel.
     * @return the instance of EmptyTabModel
     */
    public static EmptyTabModel getInstance() {
        return LazyHolder.INSTANCE;
    }

    @Override
    public Profile getProfile() {
        return null;
    }

    @Override
    public boolean isIncognito() {
        return false;
    }

    @Override
    public boolean closeTab(Tab tab) {
        return false;
    }

    @Override
    public Tab getNextTabIfClosed(int id, boolean uponExit) {
        return null;
    }

    @Override
    public void closeMultipleTabs(List<Tab> tabs, boolean canUndo) {}

    @Override
    public void closeAllTabs() {}

    @Override
    public void closeAllTabs(boolean uponExit) {}

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
    public int indexOf(Tab tab) {
        return INVALID_TAB_INDEX;
    }

    @Override
    public int index() {
        return INVALID_TAB_INDEX;
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type, boolean skipLoadingTab) {}

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
    public boolean closeTab(Tab tab, boolean animate, boolean uponExit, boolean canUndo) {
        return false;
    }

    @Override
    public boolean closeTab(
            Tab tab, Tab recommendedNextTab, boolean animate, boolean uponExit, boolean canUndo) {
        return closeTab(tab, animate, uponExit, canUndo);
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
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        assert false;
    }

    @Override
    public void addObserver(TabModelObserver observer) {}

    @Override
    public void removeObserver(TabModelObserver observer) {}

    @Override
    public void setActive(boolean active) {}

    @Override
    public void removeTab(Tab tab) {}

    @Override
    public void openMostRecentlyClosedEntry() {}
}
