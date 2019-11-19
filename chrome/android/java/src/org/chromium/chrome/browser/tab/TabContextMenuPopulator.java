// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.util.Pair;
import android.view.ContextMenu;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.chrome.browser.contextmenu.ContextMenuHelper;
import org.chromium.chrome.browser.contextmenu.ContextMenuItem;
import org.chromium.chrome.browser.contextmenu.ContextMenuParams;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;

import java.util.List;

/**
 * A simple wrapper around a {@link ContextMenuPopulator} to handle observer notification.
 */
public class TabContextMenuPopulator implements ContextMenuPopulator {
    @Nullable
    private final ContextMenuPopulator mPopulator;
    private final Tab mTab;

    /**
     * Constructs an instance of a {@link ContextMenuPopulator} and delegate calls to
     * {@code populator}.
     * @param populator The {@link ContextMenuPopulator} to delegate calls to.
     * @param tab The {@link Tab} that is using this context menu.
     */
    public TabContextMenuPopulator(ContextMenuPopulator populator, Tab tab) {
        mPopulator = populator;
        mTab = tab;
    }

    @Override
    public void onDestroy() {
        // |mPopulator| can be null for activities that do not use context menu. Following
        // methods are not called, but |onDestroy| is.
        if (mPopulator != null) mPopulator.onDestroy();
    }

    @Override
    public List<Pair<Integer, List<ContextMenuItem>>> buildContextMenu(
            ContextMenu menu, Context context, ContextMenuParams params) {
        List<Pair<Integer, List<ContextMenuItem>>> itemGroups =
                mPopulator.buildContextMenu(menu, context, params);
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().onContextMenuShown(mTab, menu);
        }
        return itemGroups;
    }

    @Override
    public boolean onItemSelected(ContextMenuHelper helper, ContextMenuParams params, int itemId) {
        return mPopulator.onItemSelected(helper, params, itemId);
    }
}
