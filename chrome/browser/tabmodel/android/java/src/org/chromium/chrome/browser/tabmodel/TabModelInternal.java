// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;

/** Package private internal methods for {@link TabModel}. */
@NullMarked
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public interface TabModelInternal extends Destroyable, TabCloser, TabModel {
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
}
