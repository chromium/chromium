// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.url.GURL;

/** An interface to handle actions related to tab groups. */
@NullMarked
public interface DataSharingTabGroupsDelegate {
    /**
     * Open the tab group dialog of the given tab group id. The tab group should already be open in
     * the current tab model when this is called.
     *
     * @param tabGroupId The id of the group to open.
     */
    void openTabGroup(@Nullable Token tabGroupId);

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

    /**
     * Tries to discern the correct window id that contains a tab group. If the requested tab group
     * cannot be found, then INVALID_WINDOW_ID is returned.
     *
     * @param tabGroupId The group id to look for.
     */
    @WindowId
    int findWindowIdForTabGroup(@Nullable Token tabGroupId);

    /**
     * Launch an intent in the specified window. The current state of the window is unknown.
     *
     * @param intent The intent to launch.
     * @param windowId The id of the window which has existing tab data.
     */
    void launchIntentInMaybeClosedWindow(Intent intent, @WindowId int windowId);
}
