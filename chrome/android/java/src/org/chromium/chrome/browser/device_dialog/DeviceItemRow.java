// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.Nullable;

/**
 * A class representing one data row in the dialog.
 */
public class DeviceItemRow {
    public final String mKey;
    public String mDescription;
    public Drawable mIcon;
    public String mIconDescription;

    /**
     * Creates a device item row which can be shown in the dialog.
     *
     * @param key Item unique identifier.
     * @param description Item description.
     * @param icon Item icon.
     * @param iconDescription Item icon description.
     */
    public DeviceItemRow(String key, String description, @Nullable Drawable icon,
            @Nullable String iconDescription) {
        mKey = key;
        mDescription = description;
        mIcon = icon;
        mIconDescription = iconDescription;
    }

    /**
     * Returns true if all parameters match the corresponding member.
     *
     * @param key Expected item unique identifier.
     * @param description Expected item description.
     * @param icon Expected item icon.
     */
    public boolean hasSameContents(String key, String description, @Nullable Drawable icon,
            @Nullable String iconDescription) {
        if (!TextUtils.equals(mKey, key)) return false;
        if (!TextUtils.equals(mDescription, description)) return false;
        if (!TextUtils.equals(mIconDescription, iconDescription)) return false;

        if (icon != null && mIcon != null) {
            Bitmap myBitmap = Bitmap.createBitmap(
                    icon.getIntrinsicWidth(), icon.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
            Canvas myCanvas = new Canvas();
            myCanvas.setBitmap(myBitmap);
            mIcon.setBounds(0, 0, myCanvas.getWidth(), myCanvas.getHeight());
            mIcon.draw(myCanvas);

            Bitmap theirBitmap = Bitmap.createBitmap(
                    icon.getIntrinsicWidth(), icon.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
            Canvas theirCanvas = new Canvas();
            theirCanvas.setBitmap(theirBitmap);
            icon.setBounds(0, 0, theirCanvas.getWidth(), theirCanvas.getHeight());
            icon.draw(theirCanvas);

            return myBitmap.sameAs(theirBitmap);
        }

        return icon == null && mIcon == null;
    }
}
