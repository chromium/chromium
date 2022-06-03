// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Exposes some well-known menu actions as direct actions.
 *
 * <p>This handler exposes a subset of available menu item actions, exposed by the given {@link
 * MenuOrKeyboardActionController}. By default, no actions are exposed; call {@link
 * #allowlistActions} or {@link #allowAllActions} to enable them.
 */
class MenuDirectActionHandler implements DirectActionHandler {
    /** Maps some menu item actions to direct actions. */
    private static final Map<String, Integer> ACTION_MAP;
    static {
        Map<String, Integer> map = new HashMap<>();
        ACTION_MAP = Collections.unmodifiableMap(map);
    }

    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabModelSelector mTabModelSelector;

    /** If non-null, only actions that belong to this allowlist are available. */
    @Nullable
    private Set<Integer> mActionIdAllowlist = new HashSet<>();

    MenuDirectActionHandler(MenuOrKeyboardActionController menuOrKeyboardActionController,
            TabModelSelector tabModelSelector) {
        this.mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        this.mTabModelSelector = tabModelSelector;
    }

    /** Allows the use of all known actions. */
    void allowAllActions() {
        mActionIdAllowlist = null;
    }

    /**
     * Allows the use of the specified action, identified by their menu item id.
     *
     * <p>Does nothing if the actions are already available.
     */
    void allowlistActions(Integer... itemIds) {
        if (mActionIdAllowlist == null) return;

        for (int itemId : itemIds) {
            mActionIdAllowlist.add(itemId);
        }
    }

    @Override
    public void reportAvailableDirectActions(DirectActionReporter reporter) {
    }

    @Override
    public boolean performDirectAction(
            String actionId, Bundle arguments, Callback<Bundle> callback) {
        Integer menuId = ACTION_MAP.get(actionId);
        if (menuId != null && (mActionIdAllowlist == null || mActionIdAllowlist.contains(menuId))
                && mMenuOrKeyboardActionController.onMenuOrKeyboardAction(
                        menuId, /* fromMenu= */ false)) {
            callback.onResult(Bundle.EMPTY);
            return true;
        }
        return false;
    }
}
