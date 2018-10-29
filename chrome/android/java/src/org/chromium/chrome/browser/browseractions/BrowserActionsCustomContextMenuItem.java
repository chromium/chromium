// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browseractions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.support.annotation.IdRes;
import android.support.customtabs.browseractions.BrowserActionItem;
import android.support.customtabs.browseractions.BrowserServiceImageReadTask;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.contextmenu.ContextMenuItem;

/**
 * A class represents Browser Actions context menu with custom title and icon.
 */
public class BrowserActionsCustomContextMenuItem implements ContextMenuItem {
    @IdRes
    private final int mMenuId;
    private final String mTitle;
    private final Uri mIconUri;
    private Drawable mIcon;

    /**
     * Constructor to build a custom context menu item from {@link BrowserActionItem}.
     * @param id The {@link IdRes} of the custom context menu item.
     * @param title The title of the custom context menu item.
     * @param icon The icon of the custom context menu item.
     * @param iconUri The {@link Uri} used to access the icon of the custom context menu item.
     */
    BrowserActionsCustomContextMenuItem(@IdRes int id, String title, Drawable icon, Uri iconUri) {
        mMenuId = id;
        mTitle = title;
        mIcon = icon;
        mIconUri = iconUri;
    }

    @Override
    public int getMenuId() {
        return mMenuId;
    }

    @Override
    public String getTitle(Context context) {
        return mTitle;
    }

    @Override
    public void getDrawableAsync(Context context, Callback<Drawable> callback) {
        if (mIconUri != null) {
            BrowserServiceImageReadTask task =
                    new BrowserServiceImageReadTask(context.getContentResolver()) {
                        @Override
                        protected void onBitmapFileReady(Bitmap bitmap) {
                            Drawable drawable = new BitmapDrawable(context.getResources(), bitmap);
                            callback.onResult(drawable);
                        }

                    };
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, mIconUri);
        } else {
            callback.onResult(mIcon);
        }
    }

    /**
     * Set the drawable icon of custom context menu item.
     */
    @VisibleForTesting
    void setDrawable(Drawable drawable) {
        mIcon = drawable;
    }

    /**
     * @return the {@link Uri} used to access the icon of custom context menu item.
     */
    public Uri getIconUri() {
        return mIconUri;
    }
}