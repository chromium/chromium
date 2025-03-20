// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;

/** Package private interface extension of {@link TabGroupModelFilter}. */
@NullMarked
interface TabGroupModelFilterInternal extends Destroyable, TabCloser, TabGroupModelFilter {
    /**
     * Mark TabState initialized, and TabGroupModelFilter ready to use. This should only be called
     * once, and should only be called by {@link TabGroupModelFilterProvider}.
     */
    /*package*/ void markTabStateInitialized();

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
