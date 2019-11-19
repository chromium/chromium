// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

/**
 * Set of core Chrome direct actions, introduced in this package.
 *
 * <p>This is not the full set of direct actions potentially available in Chrome. The full set might
 * not even be available at compile time. For an up-to-date list, see
 * http://go.ext.google.com/chrome-direct-action-list Please update this list when adding a new
 * direct action to this set.
 */
public class ChromeDirectActionIds {
    // Equivalent to pressing the go back button. Always available.
    public static final String GO_BACK = "go_back";

    // Reload the current tab.
    public static final String RELOAD = "reload";

    // Navigate the current tab's document forward.
    public static final String GO_FORWARD = "go_forward";

    // Add or update the bookmark for the current page on the current table.
    public static final String BOOKMARK_THIS_PAGE = "bookmark_this_page";

    // Open the downloads page.
    public static final String DOWNLOADS = "downloads";

    // Open the preferences dialog.
    public static final String PREFERENCES = "preferences";

    // Open the history page.
    public static final String OPEN_HISTORY = "open_history";

    // Display the help and feedback dialog appropriate for the current tab.
    public static final String HELP = "help";

    // Open a new tab.
    public static final String NEW_TAB = "new_tab";

    // Close the current tab.
    public static final String CLOSE_TAB = "close_tab";

    // Close all tabs
    public static final String CLOSE_ALL_TABS = "close_all_tabs";

    // If you add a new action to this list, consider extending ChromeDirectActionUsageHistogram to
    // track usage of the new action.

    private ChromeDirectActionIds() {
        // This is a utility class. It is not meant to be instantiated.
    }
}
