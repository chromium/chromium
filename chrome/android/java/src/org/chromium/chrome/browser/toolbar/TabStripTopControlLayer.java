// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripHeightObserver;

/**
 * Top control layer representing tab strip. It can have different state than the current height
 * store in the StripLayoutHelperManager, as this represents the target height it is going for
 * during tab strip height transition.
 */
@NullMarked
class TabStripTopControlLayer extends ObservableSupplierImpl<Integer>
        implements TopControlLayer, TabStripHeightObserver {

    public TabStripTopControlLayer(int tabStripHeight) {
        super(tabStripHeight);
    }

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.TABSTRIP;
    }

    @Override
    public int getTopControlHeight() {
        return get();
    }

    @Override
    public int getTopControlVisibility() {
        // The tab strip adds to the total height of the top controls regardless of whether or
        // not it is "visible" to the user, i.e. we take its inherent height into account even
        // when scrolled offscreen or obscured, except when hidden by height transition.
        //
        // TODO(crbug.com/417238089): Possibly add way to notify stacker of visibility changes.
        boolean isTabStripVisibleAsLayer = get() > 0;
        return isTabStripVisibleAsLayer
                ? TopControlVisibility.VISIBLE
                : TopControlVisibility.HIDDEN;
    }

    @Override
    public void onTransitionRequested(int newHeight) {
        // TODO(crbug.com/41481630): Supplier can have an inconsistent value
        //  with mToolbar.getTabStripHeight().
        set(newHeight);
    }
}
