// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;

/** Package private interface extension of {@link TabGroupModelFilter} */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public interface TabGroupModelFilterInternal extends TabGroupModelFilter, Destroyable {
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
}
