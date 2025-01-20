// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;

/** Package private internal methods for {@link TabModel}. */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public interface TabModelInternal extends TabModel {
    /**
     * Closes tabs based on the provided parameters. Refer to {@link TabClosureParams} for different
     * ways to close tabs. The public API for this is on {@link TabRemover}.
     *
     * @param tabClosureParams The parameters to follow when closing tabs.
     * @return Whether the tab closure succeeded (only possibly false for single tab closure).
     */
    boolean closeTabs(TabClosureParams tabClosureParams);

    /**
     * Removes the given tab from the model without destroying it. The tab should be inserted into
     * another model to avoid leaking as after this the link to the old Activity will be broken. The
     * public API for this is on {@link TabRemover}.
     *
     * @param tab The tab to remove.
     */
    void removeTab(Tab tab);

    /**
     * Set when tab model become active and inactive.
     *
     * @param active Whether the tab model is active.
     */
    /* package */ void setActive(boolean active);

    /**
     * To be called when this model should be destroyed. The model should no longer be used after
     * this.
     *
     * <p>As a result of this call, all {@link Tab}s owned by this model should be destroyed.
     */
    /* package */ void destroy();
}
