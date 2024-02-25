// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

/** An interface to control price welcome message in grid tab switcher. */
interface PriceWelcomeMessageController {
    /**
     * Remove the price welcome message item in the model list. Right now this is used when its
     * binding tab is closed in the grid tab switcher.
     */
    void removePriceWelcomeMessage();

    /**
     * Restore the price welcome message item that should show. Right now this is only used when the
     * closure of the binding tab in tab switcher is undone.
     */
    void restorePriceWelcomeMessage();

    /**
     * Show the price welcome message in tab switcher. This is used when any open tab in tab
     * switcher has a price drop.
     */
    void showPriceWelcomeMessage(PriceMessageService.PriceTabData priceTabData);
}
