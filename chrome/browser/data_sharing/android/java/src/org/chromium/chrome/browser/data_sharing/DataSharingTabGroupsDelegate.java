// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.url.GURL;

/** An interface to handle actions related to tab groups. */
public interface DataSharingTabGroupsDelegate {
    /**
     * Open the tab group dialog of the given tab group id.
     *
     * @param tabId The tabId of the first tab in the group.
     */
    void openTabGroupWithTabId(int tabId);

    /**
     * Open url in the Chrome Custom Tab.
     *
     * @param context The context of the current activity.
     * @param gurl The GURL of the page to be opened in CCT.
     */
    void openUrlInChromeCustomTab(Context context, GURL gurl);

    /**
     * Hides tab switcher if it is showing, and brings focus to the given tab. If another tab was
     * showing, it switches to the given tab.
     *
     * @param tabId The ID of the tab that it should switch to.
     */
    void hideTabSwitcherAndShowTab(int tabId);

    /**
     * Create a preview image for the tab group to show in share sheet.
     *
     * @param collaborationId The collaboration ID of the tab group.
     * @param size The expected size in pixels of the preview bitmap.
     * @param onResult The callback to return the preview image.
     */
    void getPreviewBitmap(String collaborationId, int size, Callback<Bitmap> onResult);
}
