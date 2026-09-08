// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Represents a tab managed by {@link BackgroundTabPool}, abstracting whether it is an active
 * in-memory background tab or a cold state loaded from disk.
 */
@NullMarked
public interface BackgroundPoolTab extends Destroyable {

    /** Returns the unique original tab ID of this background tab. */
    @TabId
    int getOriginalTabId();

    /** Returns the placeholder tab ID associated with this background tab. */
    @TabId
    int getPlaceholderTabId();

    /** Prepares the background tab for foreground display, if applicable. */
    default void prepareForForeground(TabModelSelector selector) {}

    /**
     * Attaches the tab to the specified {@link TabModel} directly at the target index.
     *
     * @param tabModel The window-scoped TabModel to attach the tab into.
     * @param index The target index within the TabModel.
     * @return The attached Tab instance.
     */
    default Tab attachTab(TabModel tabModel, int index) {
        return attachTab(tabModel, index, /* placeholderTabState= */ null);
    }

    /**
     * Attaches the tab to the specified {@link TabModel} directly at the target index, optionally
     * providing the placeholder {@link TabState} to splice or destroy.
     *
     * @param tabModel The window-scoped TabModel to attach the tab into.
     * @param index The target index within the TabModel.
     * @param placeholderTabState Optional placeholder TabState to splice or clean up.
     * @return The attached Tab instance.
     */
    Tab attachTab(TabModel tabModel, int index, @Nullable TabState placeholderTabState);

    @Override
    default void destroy() {}
}
