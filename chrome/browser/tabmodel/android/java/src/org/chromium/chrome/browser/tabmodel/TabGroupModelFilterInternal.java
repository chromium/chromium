// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.lifetime.Destroyable;

/** Package private interface extension of {@link TabGroupModelFilter} */
interface TabGroupModelFilterInternal extends TabGroupModelFilter, Destroyable {
    /**
     * Mark TabState initialized, and TabGroupModelFilter ready to use. This should only be called
     * once, and should only be called by {@link TabGroupModelFilterProvider}.
     */
    /*package*/ void markTabStateInitialized();

    /**
     * A wrapper around {@link TabModel#closeTabs} that sets hiding state for tab groups correctly.
     *
     * @param tabClosureParams The params to use when closing tabs.
     */
    /*package*/ boolean closeTabs(TabClosureParams tabClosureParams);

    /**
     * This method moves the Tab with {@code sourceTabId} out of the group it belongs to in the
     * specified direction.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param trailing True if the tab should be placed after the tab group when removed. False if
     *     it should be placed before.
     */
    /*package*/ void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing);
}
