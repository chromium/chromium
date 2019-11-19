// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * A controller to register/unregister {@link MenuOrKeyboardActionHandler} for menu or keyboard
 * actions and execute them.
 */
public interface MenuOrKeyboardActionController {
    /**
     * A handler for menu or keyboard actions. Register via
     * {@link #registerMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler)}.
     */
    interface MenuOrKeyboardActionHandler {
        /**
         * Handles menu item selection and keyboard shortcuts.
         *
         * @param id The ID of the selected menu item (defined in main_menu.xml) or keyboard
         *           shortcut (defined in values.xml).
         * @param fromMenu Whether this was triggered from the menu.
         * @return Whether the action was handled.
         */
        boolean handleMenuOrKeyboardAction(int id, boolean fromMenu);
    }

    /**
     * @param handler A new {@link MenuOrKeyboardActionHandler} to register.
     */
    void registerMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler handler);

    /**
     * @param handler A {@link MenuOrKeyboardActionHandler} to unregister.
     */
    void unregisterMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler handler);

    /**
     * Performs the specified action.
     *
     * @param id The ID of the selected menu item (defined in main_menu.xml) or keyboard
     *           shortcut (defined in values.xml).
     * @param fromMenu Whether this was triggered from the menu.
     * @return Whether the action was handled.
     */
    boolean onMenuOrKeyboardAction(int id, boolean fromMenu);
}
