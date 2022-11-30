// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

/**
 * An interface that handles displaying custom {@link View}s on top of {@link Tab}'s Content
 * view.
 */
public interface TabViewManager {
    /**
     * @return Whether the given {@link TabViewProvider} is currently being displayed.
     */
    boolean isShowing(TabViewProvider tabViewProvider);

    /**
     * Adds a {@link TabViewProvider} to be shown in the {@link Tab} associated with this {@link
     * TabViewManager}.
     */
    void addTabViewProvider(TabViewProvider tabViewProvider);

    /**
     * Remove the given {@link TabViewProvider} from the {@link Tab} associated with this {@link
     * TabViewManager}.
     */
    void removeTabViewProvider(TabViewProvider tabViewProvider);
}
