// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/** Observes changes to the tab model selector. */
@NullMarked
public interface TabModelSelectorObserver {
    /**
     * Called when the tab state has been initialized and the current tab count and tab model states
     * are reliable.
     */
    default void onTabStateInitialized() {}

    /** Called when the tab model selector is detroyed. */
    default void onDestroyed() {}
}
