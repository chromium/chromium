// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.graphics.drawable.Drawable;
import android.util.Size;

import org.chromium.base.Callback;

/** An interface to get the thumbnails to be shown inside the tab grid cards. */
public interface ThumbnailProvider {
    /**
     * Fetches a tab thumbnail in the form of a drawable. Usually from {@link TabContentManager}.
     *
     * @param tabId The tab ID to fetch the thumbnail of.
     * @param thumbnailSize The size of the thumbnail to retrieve.
     * @param isSelected Whether the tab is currently selected. Ignored if not multi-thumbnail.
     * @param callback Uses a {@link Drawable} instead of a {@link Bitmap} for flexibility. May
     *     receive null if no bitmap is returned.
     */
    void getTabThumbnailWithCallback(
            int tabId, Size thumbnailSize, boolean isSelected, Callback<Drawable> callback);
}
