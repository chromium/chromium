// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;

import androidx.annotation.IdRes;

/**
 * An interface to get information of context menu.
 */
public interface ContextMenuItem {
    /**
     * Gets the {@link IdRes} menuId of a context menu.
     */
    @IdRes
    int getMenuId();

    /**
     * Gets the title of a context menu item.
     * @param context The context required to get the title from resources.
     * @return The title of the menu item.
     */
    CharSequence getTitle(Context context);
}
