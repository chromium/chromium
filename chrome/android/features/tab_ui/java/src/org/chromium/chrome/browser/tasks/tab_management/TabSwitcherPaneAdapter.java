// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;

import java.util.List;

/**
 * Implementation of {@link TabSwitcher} for adapting the tab switcher pane. {@link
 * TabSwitcherPaneBase} does not directly implement this interface to avoid dependency issues and so
 * that the eventual deprecation/shrinking of the {@link TabSwitcher} interface is simpler.
 */
public class TabSwitcherPaneAdapter implements TabSwitcher {
    private final @NonNull TabSwitcherPaneBase mTabSwitcherPane;

    /**
     * @param tabSwitcherPane The {@link TabSwitcherPaneBase} to adapt.
     */
    public TabSwitcherPaneAdapter(@NonNull TabSwitcherPaneBase tabSwitcherPane) {
        mTabSwitcherPane = tabSwitcherPane;
    }

    @Override
    public void setOnTabSelectingListener(OnTabSelectingListener listener) {
        assert false : "Not reached.";
    }

    @Override
    public void initWithNative() {
        mTabSwitcherPane.initWithNative();
    }

    @Override
    public Controller getController() {
        // TODO(crbug/1505772): So far no elements of Controller are needed. Consider deprecation.
        assert false : "Not implemented.";
        return null;
    }

    @Override
    public TabListDelegate getTabListDelegate() {
        // TODO(crbug/1505772): So far no elements of TabListDelegate are needed. Consider
        // deprecation.
        assert false : "Not implemented.";
        return null;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        return mTabSwitcherPane.getTabGridDialogVisibilitySupplier();
    }

    @Override
    public @Nullable TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        return mTabSwitcherPane.getTabSwitcherCustomViewManager();
    }

    @Override
    public int getTabSwitcherTabListModelSize() {
        return mTabSwitcherPane.getTabSwitcherTabListModelSize();
    }

    @Override
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition position) {
        mTabSwitcherPane.setTabSwitcherRecyclerViewPosition(position);
    }

    @Override
    public void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        mTabSwitcherPane.showQuickDeleteAnimation(onAnimationEnd, tabs);
    }

    @Override
    public void refreshTabList() {
        mTabSwitcherPane.showAllTabs();
    }

    @Override
    public void openInvitationModal(String invitationId) {
        mTabSwitcherPane.openInvitationModal(invitationId);
    }
}
