// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * Interface for getting a filtered view of the tabs in the {@link TabModel}. The filtering logic is
 * delegated to the implementation. If no filter is active, this represents the same {@link TabList}
 * as {@link TabModel} does.
 */
public interface TabModelFilter extends TabList, Destroyable {
    /**
     * Adds a {@link TabModelObserver} to be notified on {@link TabModelFilter} changes.
     *
     * @param observer The {@link TabModelObserver} to add.
     */
    void addObserver(TabModelObserver observer);

    /**
     * Removes a {@link TabModelObserver}.
     *
     * @param observer The {@link TabModelObserver} to remove.
     */
    void removeObserver(TabModelObserver observer);

    /** Whether this is filter for the currently active {@link TabModel}. */
    boolean isCurrentlySelectedFilter();

    /** Returns the {@link TabModel} that the filter is acting on. */
    @NonNull
    TabModel getTabModel();

    /** Returns the total tab count in the underlying {@link TabModel}. */
    int getTotalTabCount();

    /**
     * Any of the concrete class can override and define a relationship that links a {@link Tab} to
     * a list of related {@link Tab}s. By default, this returns an unmodifiable list that only
     * contains the {@link Tab} with the given id. Note that the meaning of related can vary
     * depending on the filter being applied.
     *
     * @param tabId Id of the {@link Tab} try to relate with.
     * @return An unmodifiable list of {@link Tab} that relate with the given tab id.
     */
    @NonNull
    List<Tab> getRelatedTabList(int tabId);

    /**
     * Any of the concrete class can override and define a relationship that links a {@link Tab} to
     * a list of related {@link Tab}s. By default, this returns an unmodifiable list that only
     * contains the given id. Note that the meaning of related can vary depending on the filter
     * being applied.
     *
     * @param tabId Id of the {@link Tab} try to relate with.
     * @return An unmodifiable list of id that relate with the given tab id.
     */
    @NonNull
    List<Integer> getRelatedTabIds(int tabId);

    // TODO(crbug.com/41496693): This method sort of breaks the encapsulation of TabGroups being a
    // concept of TabGroupModelFilter and TabModelFilter being generic. We could call it something
    // like hasRelationship, but at this point there is only one valid implementation of
    // TabModelFilter and we should fold TabGroupModelFilter into TabModel eventually so breaking
    // encapsulation to be more clear when adding that groups of size one seems like a reasonable
    // tradeoff.
    /**
     * @param tab A {@link Tab} to check group membership of.
     * @return Whether the given {@link Tab} is part of a tab group.
     */
    boolean isTabInTabGroup(Tab tab);

    /**
     * Returns a valid position to add or move a tab to this model in the context of any related
     * tabs.
     *
     * @param tab The tab to be added/moved.
     * @param proposedPosition The current or proposed position of the tab in the model.
     * @return a valid position close to proposedPosition that respects related tab ordering rules.
     */
    int getValidPosition(Tab tab, int proposedPosition);

    /** Returns whether the tab model is fully restored. */
    boolean isTabModelRestored();
}
