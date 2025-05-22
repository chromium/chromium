// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.util.Pair;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ChipDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/** A simple wrapper around a {@link ContextMenuPopulator} to handle observer notification. */
@NullMarked
public class TabContextMenuPopulator implements ContextMenuPopulator {
    private final ContextMenuPopulator mPopulator;
    private final ContextMenuParams mParams;
    private final TabImpl mTab;

    /**
     * Constructs an instance of a {@link ContextMenuPopulator} and delegate calls to {@code
     * populator}.
     *
     * @param populator The {@link ContextMenuPopulator} to delegate calls to.
     * @param params The {@link ContextMenuParams} to use.
     * @param tab The {@link Tab} that is using this context menu.
     */
    public TabContextMenuPopulator(
            ContextMenuPopulator populator, ContextMenuParams params, Tab tab) {
        mPopulator = populator;
        mParams = params;
        mTab = (TabImpl) tab;
    }

    @Override
    public List<Pair<Integer, ModelList>> buildContextMenu() {
        List<Pair<Integer, ModelList>> itemGroups = mPopulator.buildContextMenu();
        if (!mTab.isDestroyed()) {
            TabContextMenuData.getOrCreateForTab(mTab)
                    .setLastTriggeringTouchPositionDp(
                            mParams.getTriggeringTouchXDp(), mParams.getTriggeringTouchYDp());
        }
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
        if (!mTab.isDestroyed()) {
            TabContextMenuData.getOrCreateForTab(mTab)
                    .setLastTriggeringTouchPositionDp(/* point= */ null);
        }
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
