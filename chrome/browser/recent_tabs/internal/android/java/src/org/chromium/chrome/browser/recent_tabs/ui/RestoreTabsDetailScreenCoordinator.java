// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

/**
 * Coordinator for the detail screens (device select, review tabs) of the Restore Tabs on FRE promo.
 */
public class RestoreTabsDetailScreenCoordinator {
    /** The delegate of the class. */
    public interface Delegate {
        /** The user clicked on the select/deselect all tabs item. */
        void onChangeSelectionStateForAllTabs();
        /** The user clicked on restoring all selected tabs. */
        void onSelectedTabsChosen();
    }
}
