// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;

/**
 * Manages observers for the tab list (e.g. {@link TabGroupObserver}), coordinating their lifecycles
 * and registering them on {@link TabModel} and individual {@link
 * org.chromium.chrome.browser.tab.Tab}s.
 */
@NullMarked
class TabListObserverManager {
    private final TabListLayoutDelegate mLayoutDelegate;

    /**
     * @param layoutDelegate Delegate handling list structural layout updates and tab group events.
     */
    TabListObserverManager(TabListLayoutDelegate layoutDelegate) {
        mLayoutDelegate = layoutDelegate;
    }

    /**
     * Attaches tab group observation to the given {@link TabModel}.
     *
     * @param tabModel The {@link TabModel} to observe.
     */
    void addTabGroupObserver(TabModel tabModel) {
        tabModel.addTabGroupObserver(mLayoutDelegate);
    }

    /**
     * Detaches tab group observation from the given {@link TabModel}.
     *
     * @param tabModel The {@link TabModel} to unobserve.
     */
    void removeTabGroupObserver(@Nullable TabModel tabModel) {
        if (tabModel == null) return;
        tabModel.removeTabGroupObserver(mLayoutDelegate);
    }
}
