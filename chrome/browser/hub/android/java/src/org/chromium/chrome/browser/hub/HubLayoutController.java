// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Interface implemented by {@link HubLayout} to expose a minimal API to the Hub internals. This is
 * set via {@link HubController#setHubLayoutController} once during {@link HubLayout}
 * initialization.
 */
public interface HubLayoutController {
    /**
     * Sets a tab as active and hides the Hub. A tab must be selected if the browser is
     * transitioning to an active tab. Only use {@link Tab.INVALID_TAB_ID} if a tab has already been
     * selected and doing so would repeat work.
     *
     * @param tabId The ID of the tab to select or {@link Tab.INVALID_TAB_ID}.
     */
    public void selectTabAndHideHubLayout(int tabId);

    /** Returns a supplier of the {@link LayoutType} shown prior to entering the Hub. */
    public ObservableSupplier<Integer> getPreviousLayoutTypeSupplier();
}
