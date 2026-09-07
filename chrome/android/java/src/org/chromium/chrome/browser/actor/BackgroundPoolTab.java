// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModel;

/**
 * Represents a tab managed by {@link BackgroundTabPool}, abstracting whether it is an active
 * in-memory background tab or a cold state loaded from disk.
 */
@NullMarked
public interface BackgroundPoolTab extends Destroyable {

    /** Returns the placeholder tab ID associated with this background tab. */
    @TabId
    int getPlaceholderTabId();

    /**
     * Attaches the tab to the specified {@link TabModel} directly at the target index. Asserts that
     * the placeholder tab is not attached when attaching the background tab.
     *
     * @param tabModel The window-scoped TabModel to attach the tab into.
     * @param index The target index within the TabModel.
     * @return The attached Tab instance.
     */
    default Tab attachTab(TabModel tabModel, int index) {
        assert tabModel.getTabById(getPlaceholderTabId()) == null
                : "Placeholder tab must not be attached when attaching background tab.";
        return attachTabImpl(tabModel, index);
    }

    /**
     * Implementation-specific method to attach the underlying tab to the {@link TabModel}.
     *
     * @param tabModel The window-scoped TabModel to attach the tab into.
     * @param index The target index within the TabModel.
     * @return The attached Tab instance.
     */
    Tab attachTabImpl(TabModel tabModel, int index);

    @Override
    default void destroy() {}
}
