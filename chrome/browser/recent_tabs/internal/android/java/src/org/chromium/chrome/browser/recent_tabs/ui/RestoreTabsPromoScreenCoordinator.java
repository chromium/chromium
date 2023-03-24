// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

/**
 * Coordinator for the home screen of the Restore Tabs on FRE promo.
 */
public class RestoreTabsPromoScreenCoordinator {
    /** The delegate of the class. */
    public interface Delegate {
        /** The user clicked on the selected device item. */
        void onShowDeviceList();
        /** The user clicked on restoring all tabs for the selected device. */
        void onAllTabsChosen();
        /** The user clicked on reviewing tabs for the selected device. */
        void onReviewTabsChosen();
    }
}
