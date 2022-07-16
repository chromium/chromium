// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.SaveBitmapDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ScreenshotShareSheetSaveDelegate is in charge of download the current bitmap.
 */
class ScreenshotShareSheetSaveDelegate {
    private final PropertyModel mModel;
    private final Context mContext;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final Runnable mCloseDialogRunnable;

    /**
     * The ScreenshotShareSheetSaveDelegate constructor.
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     */
    ScreenshotShareSheetSaveDelegate(Context context, PropertyModel propertyModel,
            Runnable closeDialogRunnable, AndroidPermissionDelegate permissionDelegate) {
        mContext = context;
        mModel = propertyModel;
        mPermissionDelegate = permissionDelegate;
        mCloseDialogRunnable = closeDialogRunnable;
    }

    /**
     * Saves the current image.
     */
    protected void save() {
        Bitmap bitmap = mModel.get(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP);
        if (bitmap == null) {
            return;
        }

        SaveBitmapDelegate saveBitmapDelegate = new SaveBitmapDelegate(mContext, bitmap,
                R.string.screenshot_filename_prefix, mCloseDialogRunnable, mPermissionDelegate);

        saveBitmapDelegate.save();
    }
}
