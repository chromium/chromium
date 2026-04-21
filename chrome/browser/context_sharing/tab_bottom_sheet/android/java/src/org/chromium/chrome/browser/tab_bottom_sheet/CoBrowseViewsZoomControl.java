// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Interface for controlling zoom in the WebContents for CoBrowse views. */
@NullMarked
public interface CoBrowseViewsZoomControl {
    /**
     * Zooms in the WebContents.
     *
     * @param webContents {@link WebContents} to zoom in.
     * @return True if there was a zoom change, false otherwise.
     */
    boolean zoomIn(WebContents webContents);

    /**
     * Zooms out the WebContents.
     *
     * @param webContents {@link WebContents} to zoom out.
     * @return True if there was a zoom change, false otherwise.
     */
    boolean zoomOut(WebContents webContents);
}
