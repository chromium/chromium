// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.components.embedder_support.contextmenu.ChipDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/** A simple wrapper around a {@link ContextMenuPopulator} to handle observer notification. */
public class TabContextMenuPopulator implements ContextMenuPopulator {
    @Nullable private final ContextMenuPopulator mPopulator;
    private final TabImpl mTab;

    /**
     * Constructs an instance of a {@link ContextMenuPopulator} and delegate calls to
     * {@code populator}.
     * @param populator The {@link ContextMenuPopulator} to delegate calls to.
     * @param tab The {@link Tab} that is using this context menu.
     */
    public TabContextMenuPopulator(ContextMenuPopulator populator, Tab tab) {
        mPopulator = populator;
        mTab = (TabImpl) tab;
    }

    @Override
    public List<Pair<Integer, ModelList>> buildContextMenu() {
        List<Pair<Integer, ModelList>> itemGroups = mPopulator.buildContextMenu();
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().onContextMenuShown(mTab);
        }
        return itemGroups;
    }

    @Override
    public boolean onItemSelected(int itemId) {
        return mPopulator.onItemSelected(itemId);
    }

    @Override
    public void onMenuClosed() {
        mPopulator.onMenuClosed();
    }

    @Override
    public boolean isIncognito() {
        return mPopulator.isIncognito();
    }

    @Override
    public String getPageTitle() {
        return mPopulator.getPageTitle();
    }

    @Override
    public @Nullable ChipDelegate getChipDelegate() {
        return mPopulator.getChipDelegate();
    }
}
