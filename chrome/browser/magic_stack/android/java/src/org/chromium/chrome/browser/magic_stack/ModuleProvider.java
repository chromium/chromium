// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.content.Context;

import org.chromium.chrome.browser.magic_stack.HomeModulesContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

/** The interface for a module which is shown or hidden on the magic stack. */
public interface ModuleProvider {
    int INVALID_RESOURCE_ID = -1;

    /** Shows the module. */
    void showModule();

    /**
     * Hides the module and cleans up. Called when the home surface is hidden or destroyed, or the
     * module is hidden from actions in context menu.
     */
    void hideModule();

    /**
     * Updates the module's data if necessary. This API allows the module to decide whether to
     * refresh the data.
     */
    default void updateModule() {}

    /** Gets the type of the module. */
    @ModuleType
    int getModuleType();

    /**
     * Returns whether the given context menu item is supported.
     * ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS: supported by all modules by default;
     * ContextMenuItemId.HIDE_MODULE: supported by all modules by default expect the {@link
     * ModuleType.SINGLE_TAB}.
     */
    default boolean isContextMenuItemSupported(@ContextMenuItemId int menuItemId) {
        return false;
    }

    /**
     * Gets the resource id for the text of a context menu item.
     *
     * @param menuItemId The id of the context menu item.
     */
    default int getResourceIdOfContextMenuItem(@ContextMenuItemId int menuItemId) {
        return INVALID_RESOURCE_ID;
    }

    /** Called when a context menu is created and shown. */
    void onContextMenuCreated();

    /** Returns the text shown on the context menu to hide the module. */
    String getModuleContextMenuHideText(Context context);
}
