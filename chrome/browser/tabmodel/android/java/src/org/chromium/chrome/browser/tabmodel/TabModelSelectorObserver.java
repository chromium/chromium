// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;

/** Observes changes to the tab model selector. */
public interface TabModelSelectorObserver {
    /** Called whenever the {@link TabModel} has changed. */
    default void onChange() {}

    /**
     * Called when a new tab is created.
     * @param tab A new tab being created.
     * @param creationState How the tab was created.
     */
    default void onNewTabCreated(Tab tab, @TabCreationState int creationState) {}

    /**
     * Called when a tab is hidden to switch to another tab.
     * @param tab The tab that was hidden.
     */
    default void onTabHidden(Tab tab) {}

    /**
     * Called when a different tab model has been selected.
     *
     * @param newModel The newly selected tab model.
     * @param oldModel The previously selected tab model.
     * @deprecated prefer observing {@link TabModelSelector#getCurrentTabModelSupplier()}.
     *     Especially, if the only method on this observer that is required is to observe changes in
     *     {@link TabModel}.
     */
    @Deprecated
    default void onTabModelSelected(TabModel newModel, TabModel oldModel) {}

    /**
     * Called when the tab state has been initialized and the current tab count and tab model states
     * are reliable.
     */
    default void onTabStateInitialized() {}

    /** Called when the tab model selector is detroyed. */
    default void onDestroyed() {}
}
