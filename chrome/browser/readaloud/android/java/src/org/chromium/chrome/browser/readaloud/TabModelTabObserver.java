// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.List;

/** Observer of tab changes for tabs selected within and owned by a {@link TabModel}. */
@NullMarked
public class TabModelTabObserver extends EmptyTabObserver {
    private final TabModel mTabModel;
    private final TabModelObserver mTabModelObserver;

    /**
     * Constructs an observer that should be notified of tab changes for the tabs
     * that were selected within {@link TabModel}. Any Tabs created and selected after this call
     * will be observed as well, and Tabs removed will no longer have their information
     * broadcast.
     *
     * <p>
     * {@link #destroy()} must be called to unregister this observer.
     *
     * @param model The model that owns the Tabs that should notify this observer.
     */
    public TabModelTabObserver(TabModel model) {
        mTabModel = model;
        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        onTabSelected(tab);
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        tab.removeObserver(TabModelTabObserver.this);
                    }

                    @Override
                    public void restoreCompleted() {
                        maybeCallOnRestoreCompleted();
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        TabModelTabObserver.this.willCloseTab(tab);
                    }

                    @Override
                    public void willCloseTabs(
                            List<Tab> tabs, boolean isAllTabs, boolean allowUndo) {
                        TabModelTabObserver.this.willCloseTabs(tabs, isAllTabs, allowUndo);
                    }
                };

        mTabModel.addObserver(mTabModelObserver);
        maybeCallOnTabSelected();
    }

    /** Destroys the observer and removes itself as a listener for Tab updates. */
    public void destroy() {
        mTabModel.removeObserver(mTabModelObserver);
        int tabCount = mTabModel.getCount();
        for (int i = 0; i < tabCount; i++) {
            mTabModel.getTabAtChecked(i).removeObserver(this);
        }
    }

    /**
     * Called when a tab starts closing.
     *
     * @param tab The tab to close.
     * @deprecated Use {@link #willCloseTabs(List, boolean, boolean)} instead. During the migration
     *     phase, continue implementing this method but also implement {@link #willCloseTabs(List,
     *     boolean, boolean)}.
     */
    @Deprecated
    public void willCloseTab(Tab tab) {}

    /**
     * Called before one or more tabs are removed from the {@link TabModel} for closure.
     *
     * <p>TODO(crbug.com/381471263): Method in development. This replaces {@link #willCloseTab}. It
     * is called once per closure operation while the tabs are still present in the tab model.
     *
     * @param tabs The list of {@link Tab}s about to be closed.
     * @param isAllTabs Whether this closure operation will leave 0 tabs in the model.
     * @param allowUndo Whether undo is allowed for this tab closure operation.
     */
    public void willCloseTabs(List<Tab> tabs, boolean isAllTabs, boolean allowUndo) {}

    /* Called when tabs are restored. */
    public void onRestoreCompleted(Tab tab) {}

    /* Called when a tab in a model is selected or restored. */
    protected void onTabSelected(Tab tab) {
        tab.addObserver(TabModelTabObserver.this);
    }

    private void maybeCallOnTabSelected() {
        Tab tab = mTabModel.getTabAt(mTabModel.index());
        if (tab != null) {
            onTabSelected(tab);
        }
    }

    private void maybeCallOnRestoreCompleted() {
        Tab tab = mTabModel.getTabAt(mTabModel.index());
        if (tab != null) {
            onRestoreCompleted(tab);
        }
    }
}
