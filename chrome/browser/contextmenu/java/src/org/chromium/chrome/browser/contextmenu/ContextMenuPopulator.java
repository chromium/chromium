// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.Bitmap;
import android.net.Uri;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/**
 * A delegate responsible for populating context menus and processing results from
 * ContextMenuHelper.
 */
public interface ContextMenuPopulator {
    /**
     *  Called when this ContextMenuPopulator is about to be destroyed.
     */
    void onDestroy();

    /**
     * Should be used to populate {@code menu} with the correct context menu items.
     * @param isShoppyImage Whether the selected item was a shoppy image.
     * @return A list separate by groups. Each "group" will contain items related to said group as
     *         well as an integer that is a string resource for the group. Image items will have
     *         items that belong to that are related to that group and the string resource for the
     *         group will likely say "IMAGE". If the link pressed is contains multiple items (like
     *         an image link) the list will have both an image list and a link list.
     */
    List<Pair<Integer, ModelList>> buildContextMenu(boolean isShoppyImage);

    /**
     * Called when a context menu item has been selected.
     * @param itemId The id of the selected menu item.
     * @return       Whether or not the selection was handled.
     */
    boolean onItemSelected(int itemId);

    /**
     * Gets the thumbnail of the current image that triggered the context menu.
     * @param callback Called once the the thumbnail is received.
     */
    void getThumbnail(final Callback<Bitmap> callback);

    /**
     * Retrieves a URI for the selected image for sharing with external apps. If the function fails
     * to retrieve the image bytes or generate a URI the callback will *not* be called.
     * @param imageFormat The image format will be requested.
     * @param callback Called once the image is generated and ready to be shared.
     */
    void retrieveImage(@ContextMenuImageFormat int imageFormat, Callback<Uri> callback);

    /**
     * Called when the context menu is closed.
     */
    void onMenuClosed();

    /**
     * Determines whether the the containing browser is switched to incognito mode.
     */
    boolean isIncognito();
}
