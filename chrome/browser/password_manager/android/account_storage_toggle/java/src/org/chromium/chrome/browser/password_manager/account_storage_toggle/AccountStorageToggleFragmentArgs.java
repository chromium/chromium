// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_toggle;

/**
 * The account storage toggle controls whether signed-in non-syncing users have access to account
 * passwords. It is housed by different fragments depending on certain feature flags. This class
 * contains the common arguments used by such fragments to customize the look of the toggle.
 */
public class AccountStorageToggleFragmentArgs {
    // Name for a boolean argument controlling whether to highlight the toggle.
    public static final String HIGHLIGHT = "highlight_account_storage_toggle";

    private AccountStorageToggleFragmentArgs() {}
}
