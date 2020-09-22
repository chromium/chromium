// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/**
 * A delegate responsible for populating context menus and processing results from
 * ContextMenuHelper.
 * TODO(jinsukkim): Consider ContextMenuPopulatorFactory.
 */
public interface ContextMenuPopulator {
    /**
     *  Called when this ContextMenuPopulator is about to be destroyed.
     */
    void onDestroy();

    /**
     * Should be used to populate {@code menu} with the correct context menu items.
     * @param context A {@link Context} instance.
     * @param params  The parameters that represent what should be shown in the context menu.
     * @param isShoppyImage Whether the selected item was a shoppy image.
     * @return A list separate by groups. Each "group" will contain items related to said group as
     *         well as an integer that is a string resource for the group. Image items will have
     *         items that belong to that are related to that group and the string resource for the
     *         group will likely say "IMAGE". If the link pressed is contains multiple items (like
     *         an image link) the list will have both an image list and a link list.
     */
    List<Pair<Integer, ModelList>> buildContextMenu(
            Context context, ContextMenuParams params, boolean isShoppyImage);

    /**
     * Called when a context menu item has been selected.
     * @param params The parameters that represent what is being shown in the context menu.
     * @param renderFrameHost {@link RenderFrameHost} to get the encoded images from.
     * @param itemId The id of the selected menu item.
     * @return       Whether or not the selection was handled.
     */
    boolean onItemSelected(ContextMenuParams params, RenderFrameHost renderFrameHost, int itemId);

    /**
     * Gets the thumbnail of the current image that triggered the context menu.
     * @param renderFrameHost {@link RenderFrameHost} to get the render frame from.
     * @param callback Called once the the thumbnail is received.
     */
    void getThumbnail(RenderFrameHost renderFrameHost, final Callback<Bitmap> callback);

    /**
     * Retrieves a URI for the selected image for sharing with external apps. If the function fails
     * to retrieve the image bytes or generate a URI the callback will *not* be called.
     * @param renderFrameHost {@link RenderFrameHost} to get the render frame from.
     * @param imageFormat The image format will be requested.
     * @param callback Called once the image is generated and ready to be shared.
     */
    void retrieveImage(RenderFrameHost renderFrameHost, @ContextMenuImageFormat int imageFormat,
            Callback<Uri> callback);

    /**
     * Called when the context menu is closed.
     */
    void onMenuClosed();
}
